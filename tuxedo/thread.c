#include "thread_priv.h"
#include <sys/iosupport.h>

KThread* s_firstThread;
KThread* __ppc_next_ctx;

static KThread s_mainThread, s_idleThread;
static KThrQueue s_joinQueue, s_sleepQueue;

extern char _SDA_BASE_[];
extern char _SDA2_BASE_[];

unsigned __PPCContextPrepareToJump(unsigned ret, PPCIrqState st);
void __PPCContextJump(PPCContext* ctx) MK_NORETURN;

struct _reent* __getreent(void)
{
	if (!PPCIsInExcpt()) {
		return &KThreadGetSelf()->r;
	} else {
		return &s_idleThread.r;
	}
}

static void KThreadIdleMain(void)
{
	for (;;);
}

void KThreadInit(void)
{
	// Set up main thread
	s_firstThread          = &s_mainThread;
	s_mainThread.next      = &s_idleThread;
	s_mainThread.state     = KTHR_STATE_RUNNING;
	s_mainThread.prio      = KTHR_MAIN_PRIO;
	s_mainThread.baseprio  = s_mainThread.prio;
	_REENT_INIT_PTR_ZEROED(&s_mainThread.r);

	// Set up idle thread
	s_idleThread.ctx.pc    = (u32)KThreadIdleMain;
	s_idleThread.ctx.msr   = MSR_EE|MSR_ME|MSR_IR|MSR_DR|MSR_RI;
	s_idleThread.state     = KTHR_STATE_RUNNING;
	s_idleThread.prio      = KTHR_MIN_PRIO+1;
	s_idleThread.baseprio  = s_idleThread.prio;
	_REENT_INIT_PTR_ZEROED(&s_idleThread.r);

	PPCMtspr(SPRG2, (u32)&s_mainThread);
	PPCMtspr(SPRG3, (u32)&s_mainThread);
}

void KThreadPrepare(KThread* t, KThreadFn entrypoint, void* arg, void* stack_top, u16 prio)
{
	// Initialize thread state and context
	memset(t, 0, sizeof(*t));
	t->ctx.pc       = (u32)entrypoint;
	t->ctx.msr      = MSR_EE|MSR_ME|MSR_IR|MSR_DR|MSR_RI;
	t->ctx.lr       = (u32)KThreadExit;
	t->ctx.gpr[1]   = (u32)stack_top &~ 7;
	t->ctx.gpr[2]   = (u32)_SDA2_BASE_;
	t->ctx.gpr[3]   = (u32)arg;
	t->ctx.gpr[13]  = (u32)_SDA_BASE_;
	t->state        = KTHR_STATE_WAITING;
	t->suspend      = 1;
	t->prio         = prio & KTHR_MIN_PRIO;
	t->baseprio     = t->prio;
	_REENT_INIT_PTR_ZEROED(&t->r);

	// Push initial stack frame
	if (t->ctx.gpr[1]) {
		t->ctx.gpr[1] -= 8;
		*(u32*)t->ctx.gpr[1] = UINT32_MAX;
	}

	// Insert into thread list
	PPCIrqState st = PPCIrqLockByMsr();
	KThreadEnqueue(t);
	PPCIrqUnlockByMsr(st);
}

MK_NOINLINE void KThreadSwitchTo(KThread* t, PPCIrqState st)
{
	if (PPCIsInExcpt()) {
		if (!__ppc_next_ctx || t->prio < __ppc_next_ctx->prio) {
			__ppc_next_ctx = t;
		}

		PPCIrqUnlockByMsr(st);
		return;
	}

	if (!__PPCContextPrepareToJump(1, st)) {
		__PPCContextJump(&t->ctx);
	}
}

void KThreadResume(KThread* t)
{
	PPCIrqState st = PPCIrqLockByMsr();

	if (!t->suspend || (--t->suspend)) {
		PPCIrqUnlockByMsr(st);
		return;
	}

	if (!t->wait.queue) {
		t->state = KTHR_STATE_RUNNING;
		KThreadReschedule(t, st);
	} else {
		PPCIrqUnlockByMsr(st);
	}
}

void KThreadSuspend(KThread* t)
{
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	if ((t->suspend++) != 0 || t->state != KTHR_STATE_RUNNING) {
		PPCIrqUnlockByMsr(st);
		return;
	}

	t->state = KTHR_STATE_WAITING;
	t->wait.queue = NULL;

	if (self == t) {
		KThread* next = KThreadFindRunnable(s_firstThread);
		KThreadSwitchTo(next, st);
	} else {
		PPCIrqUnlockByMsr(st);
	}
}

void KThreadSetPrio(KThread* t, u16 prio)
{
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	unsigned curprio = self->prio;
	t->baseprio = prio & KTHR_MIN_PRIO;
	KThreadUpdateDynamicPrio(t);

	KThread* next = NULL;
	if (t == self) {
		if (self->prio > curprio) {
			next = KThreadFindRunnable(s_firstThread);
		}
	} else if (t->state == KTHR_STATE_RUNNING) {
		next = t;
	}

	KThreadReschedule(next, st);
}

sptr KThreadJoin(KThread* t)
{
	PPCIrqState st = PPCIrqLockByMsr();

	if (t->state >= KTHR_STATE_RUNNING) {
		KThrQueueBlock(&s_joinQueue, (uptr)t);
	}

	sptr rc = t->finish.rc;
	PPCIrqUnlockByMsr(st);
	return rc;
}

void KThreadYield(void)
{
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	KThread* t = KThreadFindRunnable(self->next);
	if (t->prio > self->prio) {
		t = KThreadFindRunnable(s_firstThread);
	}

	if (t != self) {
		KThreadSwitchTo(t, st);
	} else {
		PPCIrqUnlockByMsr(st);
	}
}

static void KThreadSleepWakeup(KTickTask* task)
{
	KThrQueueUnblockAllByValue(&s_sleepQueue, (uptr)task);
}

void KThreadSleepTicks(u64 ticks)
{
	KTickTask task;
	PPCIrqState st = PPCIrqLockByMsr();
	KTickTaskStart(&task, KThreadSleepWakeup, ticks, 0);
	KThrQueueBlock(&s_sleepQueue, (uptr)&task);
	PPCIrqUnlockByMsr(st);
}

void KThreadExit(sptr rc)
{
	KThread* self = KThreadGetSelf();

	u32 msr = PPCMfmsr();
	PPCMtmsr(msr &~ (MSR_EE|MSR_FP|MSR_RI));

	KThreadDequeue(self);

	self->state = KTHR_STATE_FINISHED;
	self->finish.rc = rc;
	KThrQueueUnblockAllByValue(&s_joinQueue, (uptr)self);

	KThread* next = __ppc_next_ctx;
	if (!next) {
		next = KThreadFindRunnable(s_firstThread);
	}

	__PPCContextJump(&next->ctx);
}

MK_NOINLINE uptr KThrQueueBlock(KThrQueue* q, uptr token)
{
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	self->state = KTHR_STATE_WAITING;
	self->wait.token = token;
	KThrQueueInsert(q, self);

	KThread* next = KThreadFindRunnable(s_firstThread);
	KThreadSwitchTo(next, st);

	return self->wait.token;
}

MK_INLINE void KThrQueueUnblockCommon(KThrQueue* q, int max, KThrUnblockMode mode, uptr ref)
{
	PPCIrqState st = PPCIrqLockByMsr();
	KThread* resched = NULL;
	KThread* next;

	for (KThread* cur = q->next; max != 0 && cur; cur = next) {
		next = cur->wait.link.next;
		if (!KThrQueueTestUnblock(cur, mode, ref)) {
			continue;
		}

		KThrQueueRemove(q, cur);

		if (mode == KTHR_UNBLOCK_BY_MASK) {
			cur->wait.token &= ref;
		} else {
			cur->wait.token = 1;
		}

		if (!cur->suspend) {
			cur->state = KTHR_STATE_RUNNING;
			if (!resched) {
				resched = cur; // Remember the first unblocked (highest priority) thread
			}
		}

		if (max > 0) {
			--max;
		}
	}

	KThreadReschedule(resched, st);
}

MK_NOINLINE void KThrQueueUnblockOneByValue(KThrQueue* q, uptr ref)
{
	KThrQueueUnblockCommon(q, +1, KTHR_UNBLOCK_BY_VALUE, ref);
}

MK_NOINLINE void KThrQueueUnblockOneByMask(KThrQueue* q, uptr ref)
{
	KThrQueueUnblockCommon(q, +1, KTHR_UNBLOCK_BY_MASK, ref);
}

MK_NOINLINE void KThrQueueUnblockAllByValue(KThrQueue* q, uptr ref)
{
	KThrQueueUnblockCommon(q, -1, KTHR_UNBLOCK_BY_VALUE, ref);
}

MK_NOINLINE void KThrQueueUnblockAllByMask(KThrQueue* q, uptr ref)
{
	KThrQueueUnblockCommon(q, -1, KTHR_UNBLOCK_BY_MASK, ref);
}

void KThrQueueCancel(KThrQueue* q, KThread* t)
{
	PPCIrqState st = PPCIrqLockByMsr();
	KThread* resched = NULL;

	if (t->state != KTHR_STATE_WAITING || t->wait.queue != q) {
		PPCIrqUnlockByMsr(st);
		return;
	}

	KThrQueueRemove(q, t);

	t->wait.token = 0;

	if (!t->suspend) {
		t->state = KTHR_STATE_RUNNING;
		resched = t;
	}

	KThreadReschedule(resched, st);
}
