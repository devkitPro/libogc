// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <tuxedo/tick.h>

static KTickTask* s_firstTask;

MK_CONSTEXPR bool KTickIsSequential(u64 lhs, u64 rhs)
{
	return (s64)(rhs - lhs) > 0;
}

MK_INLINE KTickTask* KTickTaskGetInsertPosition(KTickTask* t)
{
	KTickTask* pos = NULL;
	for (KTickTask* cur = s_firstTask; cur && KTickIsSequential(cur->target, t->target); cur = cur->next) {
		pos = cur;
	}
	return pos;
}

MK_INLINE KTickTask* KTickTaskGetPrevious(KTickTask* t)
{
	for (KTickTask* cur = s_firstTask; cur; cur = cur->next) {
		if (cur->next == t) {
			return cur;
		}
	}
	return NULL;
}

MK_INLINE void KTickTaskEnqueue(KTickTask* t)
{
	KTickTask* pos = KTickTaskGetInsertPosition(t);
	if (pos) {
		t->next = pos->next;
		pos->next = t;
	} else {
		t->next = s_firstTask;
		s_firstTask = t;
	}
}

MK_INLINE void KTickTaskDequeue(KTickTask* t)
{
	if (s_firstTask == t) {
		s_firstTask = t->next;
	} else {
		KTickTask* prev = KTickTaskGetPrevious(t);
		if (prev) {
			prev->next = t->next;
		}
	}
}

MK_NOINLINE static void KTickTaskReschedule(void)
{
	if (!s_firstTask) {
		return;
	}

	s64 diff = s_firstTask->target - PPCGetTickCount();
	u32 decr;
	if (diff < 0) {
		decr = 0;
	} else if (diff <= INT32_MAX) {
		decr = diff;
	} else {
		decr = INT32_MAX;
	}

	PPCMtspr(DEC, decr);
}

void KTickTaskRun(void)
{
	while (s_firstTask && !KTickIsSequential(PPCGetTickCount(), s_firstTask->target)) {
		KTickTask* cur = s_firstTask;
		s_firstTask = cur->next;

		KTickTaskFn fn = cur->fn;
		if (cur->period) {
			cur->target += cur->period;
			KTickTaskEnqueue(cur);
		} else {
			cur->fn = NULL;
		}

		fn(cur);
	}

	KTickTaskReschedule();
}

void KTickTaskStart(KTickTask* t, KTickTaskFn fn, u64 delay_ticks, u64 period_ticks)
{
	PPCIrqState st = PPCIrqLockByMsr();
	KTickTask* old = s_firstTask;

	if (t->fn) {
		KTickTaskDequeue(t);
	}

	t->target = PPCGetTickCount() + delay_ticks;
	t->period = period_ticks;
	t->fn = fn;
	KTickTaskEnqueue(t);

	if (s_firstTask != old) {
		KTickTaskReschedule();
	}

	PPCIrqUnlockByMsr(st);
}

void KTickTaskStop(KTickTask* t)
{
	PPCIrqState st = PPCIrqLockByMsr();

	if (!t->fn) {
		PPCIrqUnlockByMsr(st);
		return;
	}

	bool need_resched = s_firstTask == t;
	KTickTaskDequeue(t);

	if (need_resched) {
		KTickTaskReschedule();
	}

	t->fn = NULL;
	PPCIrqUnlockByMsr(st);
}
