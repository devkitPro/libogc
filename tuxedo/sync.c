#include "thread_priv.h"
#include <tuxedo/sync.h>

static KThrQueue s_cvWaitQueue;

MK_NOINLINE void KThreadUpdateDynamicPrio(KThread* t)
{
	for (;;) {
		// Calculate the expected dynamic priority of the thread
		unsigned prio = t->baseprio;
		if (t->waiters.next) {
			unsigned waiter_prio = t->waiters.next->prio;
			prio = waiter_prio < prio ? waiter_prio : prio;
		}

		// If the priority is the same we're done
		if (t->prio == prio) {
			break;
		}

		// Requeue the thread
		KThreadDequeue(t);
		t->prio = prio;
		KThreadEnqueue(t);

		// If the thread is running we're done
		if (t->state == KTHR_STATE_RUNNING) {
			break;
		}

		// If the thread is suspended (== not waiting on any queue) we're also done
		KThrQueue* q = t->wait.queue;
		if (!q) {
			break;
		}

		// Reflect the bumped priority too in the queue the thread is blocked on
		KThrQueueRemove(q, t);
		KThrQueueInsert(q, t);

		// If the thread is not waiting on a mutex, we're done
		if (t->state != KTHR_STATE_WAITING_MUTEX) {
			break;
		}

		// Update the holder thread's dynamic priority as well
		t = ((KMutex*)t->wait.token)->owner;
	}
}

MK_NOINLINE static KThread* KThreadRemoveWaiter(KThread* t, uptr token)
{
	KThread* next_owner = NULL;
	KThread* next_waiter;
	for (KThread* cur = t->waiters.next; cur; cur = next_waiter) {
		next_waiter = cur->wait.link.next;
		if (cur->wait.token == token) {
			KThrQueueRemove(&t->waiters, cur);
			if (!next_owner) {
				next_owner = cur;
				cur->state = !cur->suspend ? KTHR_STATE_RUNNING : KTHR_STATE_WAITING;
			} else {
				// TODO: Both the source list and the target list are sorted.
				// Should we try to optimize this to take that into account?
				KThrQueueInsert(&next_owner->waiters, cur);
			}
		}
	}

	// Update dynamic priorities for both threads if needed
	if (next_owner) {
		KThreadUpdateDynamicPrio(t);
		KThreadUpdateDynamicPrio(next_owner);
	}

	return next_owner;
}

MK_INLINE bool KMutexTryLockImpl(KMutex* m, KThread* self)
{
	KThread* no_owner = NULL;
	return __atomic_compare_exchange_n(&m->owner, &no_owner, self, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

bool KMutexTryLock(KMutex* m)
{
	return KMutexTryLockImpl(m, KThreadGetSelf());
}

void KMutexLock(KMutex* m)
{
	KThread* self = KThreadGetSelf();

	// Fastest path: nobody holding the mutex, so we can just take it
	if (KMutexTryLockImpl(m, self)) {
		return;
	}

	PPCIrqState st = PPCIrqLockByMsr();

	// Second fastest path: someone released the mutex between the TryLock and the IrqLock, so we can now take it
	if (!m->owner) {
		m->owner = self;
		PPCIrqUnlockByMsr(st);
		return;
	}

	// Add current thread to owner thread's list of waiters
	self->state = KTHR_STATE_WAITING_MUTEX;
	self->wait.token = (uptr)m;
	KThrQueueInsert(&m->owner->waiters, self);

	// Bump dynamic priority of owner thread if needed
	KThread* next = NULL;
	if (self->prio < m->owner->prio) {
		KThreadUpdateDynamicPrio(m->owner);

		// Fast path when the owner thread is runnable
		if (m->owner->state == KTHR_STATE_RUNNING) {
			next = m->owner;
		}
	}

	// Select next thread to run if above code didn't
	if (!next) {
		next = KThreadFindRunnable(s_firstThread);
	}

	KThreadSwitchTo(next, st);
}

void KMutexUnlock(KMutex* m)
{
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	if (m->owner != self) {
		for (;;); // ERROR
	}

	unsigned old_prio = self->prio;
	m->owner = KThreadRemoveWaiter(self, (uptr)m);

	KThread* next = self;
	if (old_prio < self->prio) {
		next = KThreadFindRunnable(s_firstThread);
	}

	if (next != self) {
		KThreadReschedule(next, st);
	} else {
		PPCIrqUnlockByMsr(st);
	}
}

void KCondVarSignal(KCondVar* cv)
{
	KThrQueueUnblockOneByValue(&s_cvWaitQueue, (uptr)cv);
}

void KCondVarBroadcast(KCondVar* cv)
{
	KThrQueueUnblockAllByValue(&s_cvWaitQueue, (uptr)cv);
}

void KCondVarWait(KCondVar* cv, KMutex* m)
{
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	if (m->owner != self) {
		for (;;); // ERROR
	}

	m->owner = KThreadRemoveWaiter(self, (uptr)m);
	KThrQueueBlock(&s_cvWaitQueue, (uptr)cv);
	KMutexLock(m);
	PPCIrqUnlockByMsr(st);
}

typedef struct KCondVarTimeout {
	KTickTask task;
	KThread* self;
} KCondVarTimeout;

static void KCondVarTimeoutFn(KTickTask* t)
{
	KThread* self = ((KCondVarTimeout*)t)->self;
	KThrQueueCancel(&s_cvWaitQueue, self);
}

bool KCondVarWaitTimeoutTicks(KCondVar* cv, KMutex* m, u64 timeout_ticks)
{
	if (!timeout_ticks) {
		return false;
	}

	KCondVarTimeout t;
	KThread* self = KThreadGetSelf();
	PPCIrqState st = PPCIrqLockByMsr();

	if (m->owner != self) {
		for (;;); // ERROR
	}

	m->owner = KThreadRemoveWaiter(self, (uptr)m);

	t.self = self;
	KTickTaskStart(&t.task, KCondVarTimeoutFn, timeout_ticks, 0);

	bool ret = KThrQueueBlock(&s_cvWaitQueue, (uptr)cv) != 0;

	KTickTaskStop(&t.task);

	KMutexLock(m);
	PPCIrqUnlockByMsr(st);

	return ret;
}
