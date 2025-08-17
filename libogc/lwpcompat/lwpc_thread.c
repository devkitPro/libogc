#include "lwpc_common.h"
#include <ogc/lwp.h>
#include <stdlib.h>

MK_INLINE KThread* lwpc_get_thread(lwp_t thethread)
{
	if (!thethread || thethread == LWP_THREAD_NULL) {
		return NULL;
	}

	return (KThread*)~thethread;
}

s32 LWP_CreateThread(lwp_t* thethread, void* (*entry)(void*), void* arg, void* stackbase, u32 stack_size, u8 prio)
{
	if (!thethread || !entry) {
		return -1;
	}

	if (!stack_size) {
		stack_size = 8*1024;
	}

	u32 alloc_sz = sizeof(KThread);
	if (!stackbase) {
		alloc_sz += stack_size;
	}

	KThread* t = malloc(alloc_sz);
	if (!t) {
		*thethread = LWP_THREAD_NULL;
		return -1;
	}

	if (!stackbase) {
		stackbase = t+1;
	}

	KThreadPrepare(t, (KThreadFn)entry, arg, (char*)stackbase + stack_size, KTHR_MIN_PRIO - (prio & LWP_PRIO_HIGHEST));
	KThreadResume(t);

	*thethread = ~(u32)t;
	return 0;
}

s32 LWP_SuspendThread(lwp_t thethread)
{
	KThread* t = lwpc_get_thread(thethread);
	if (!t) {
		return -1;
	}

	KThreadSuspend(t);
	return 0;
}

s32 LWP_ResumeThread(lwp_t thethread)
{
	KThread* t = lwpc_get_thread(thethread);
	if (!t) {
		return -1;
	}

	KThreadResume(t);
	return 0;
}

BOOL LWP_ThreadIsSuspended(lwp_t thethread)
{
	KThread* t = lwpc_get_thread(thethread);
	return t && t->suspend != 0;
}

lwp_t LWP_GetSelf(void)
{
	return ~(u32)KThreadGetSelf();
}

void LWP_SetThreadPriority(lwp_t thethread, u32 prio)
{
	KThread* t = lwpc_get_thread(thethread);
	if (t) {
		KThreadSetPrio(t, KTHR_MIN_PRIO - (prio & LWP_PRIO_HIGHEST));
	}
}

void LWP_YieldThread(void)
{
	KThreadYield();
}

// Not implementable
//void LWP_Reschedule(u32 prio);

s32 LWP_JoinThread(lwp_t thethread, void** value_ptr)
{
	KThread* t = lwpc_get_thread(thethread);
	if (!t) {
		return -1;
	}

	sptr ret = KThreadJoin(t);
	if (value_ptr) {
		*value_ptr = (void*)ret;
	}

	free(t);
	return 0;
}
