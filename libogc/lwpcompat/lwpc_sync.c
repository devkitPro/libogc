#include "lwpc_common.h"
#include <errno.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include <ogc/lwp_watchdog.h>

static u64 lwpc_mutex_used;
static u64 lwpc_mutex_recursive;
static KRMutex lwpc_mutex_table[64];

static u64 lwpc_cond_used;

s32 LWP_MutexInit(mutex_t* mutex, bool use_recursive)
{
	int slot = lwpc_alloc_slot(&lwpc_mutex_used);
	if (slot < 0) {
		*mutex = LWP_MUTEX_NULL;
		return -1;
	}

	PPCIrqState st = PPCIrqLockByMsr();
	if (use_recursive) {
		lwpc_mutex_recursive |= 1ULL << slot;
	} else {
		lwpc_mutex_recursive &= ~(1ULL << slot);
	}
	PPCIrqUnlockByMsr(st);

	*mutex = 1+slot;
	return 0;
}

s32 LWP_MutexDestroy(mutex_t mutex)
{
	if (!mutex || mutex == LWP_MUTEX_NULL) {
		return -1;
	}

	lwpc_free_slot(&lwpc_mutex_used, mutex-1);
	return 0;
}

s32 LWP_MutexLock(mutex_t mutex)
{
	if (!mutex || mutex == LWP_MUTEX_NULL) {
		return -1;
	}

	int slot = mutex-1;
	if (lwpc_mutex_recursive & (1ULL << slot)) {
		KRMutexLock(&lwpc_mutex_table[slot]);
	} else {
		KMutexLock(&lwpc_mutex_table[slot].mutex);
	}

	return 0;
}

s32 LWP_MutexTryLock(mutex_t mutex)
{
	if (!mutex || mutex == LWP_MUTEX_NULL) {
		return -1;
	}

	int slot = mutex-1;
	bool rc;
	if (lwpc_mutex_recursive & (1ULL << slot)) {
		rc = KRMutexTryLock(&lwpc_mutex_table[slot]);
	} else {
		rc = KMutexTryLock(&lwpc_mutex_table[slot].mutex);
	}

	return rc ? 0 : 1;
}

s32 LWP_MutexUnlock(mutex_t mutex)
{
	if (!mutex || mutex == LWP_MUTEX_NULL) {
		return -1;
	}

	int slot = mutex-1;
	if (lwpc_mutex_recursive & (1ULL << slot)) {
		KRMutexUnlock(&lwpc_mutex_table[slot]);
	} else {
		KMutexUnlock(&lwpc_mutex_table[slot].mutex);
	}

	return 0;
}

MK_INLINE KCondVar* lwpc_get_condvar(cond_t cond)
{
	if (!cond || cond == LWP_COND_NULL) {
		return NULL;
	}

	return (KCondVar*)(0xc0000000 + cond);
}

MK_INLINE KRMutex* lwpc_get_mutex_for_cv(mutex_t mutex, bool* is_recursive)
{
	if (!mutex || mutex == LWP_MUTEX_NULL) {
		return NULL;
	}

	int slot = mutex-1;
	*is_recursive = (lwpc_mutex_recursive & (1ULL << slot)) != 0;

	return &lwpc_mutex_table[slot];
}

s32 LWP_CondInit(cond_t* cond)
{
	int slot = lwpc_alloc_slot(&lwpc_cond_used);
	if (slot < 0) {
		*cond = LWP_COND_NULL;
		return -1;
	}

	*cond = slot+1;
	return 0;
}

s32 LWP_CondDestroy(cond_t cond)
{
	if (!cond || cond == LWP_COND_NULL) {
		return -1;
	}

	lwpc_free_slot(&lwpc_cond_used, cond-1);
	return 0;
}

s32 LWP_CondWait(cond_t cond, mutex_t mutex)
{
	return LWP_CondTimedWait(cond, mutex, NULL);
}

s32 LWP_CondSignal(cond_t cond)
{
	KCondVar* cv = lwpc_get_condvar(cond);
	if (!cv) {
		return -1;
	}

	KCondVarSignal(cv);
	return 0;
}

s32 LWP_CondBroadcast(cond_t cond)
{
	KCondVar* cv = lwpc_get_condvar(cond);
	if (!cv) {
		return -1;
	}

	KCondVarBroadcast(cv);
	return 0;
}

static s32 lwpc_cond_timed_wait_impl(KCondVar* cv, KMutex* m, const struct timespec* reltime)
{
	if (reltime) {
		return KCondVarWaitTimeoutTicks(cv, m, timespec_to_ticks(reltime)) ? 0 : ETIMEDOUT;
	} else {
		KCondVarWait(cv, m);
		return 0;
	}
}

s32 LWP_CondTimedWait(cond_t cond, mutex_t mutex, const struct timespec* reltime)
{
	KCondVar* cv = lwpc_get_condvar(cond);
	if (!cv) {
		return -1;
	}

	bool is_recursive = false;
	KRMutex* m = lwpc_get_mutex_for_cv(mutex, &is_recursive);
	if (!m) {
		return -1;
	}

	u32 counter_backup;
	if (is_recursive) {
		counter_backup = m->counter;
		m->counter = 0;
	}

	s32 ret = lwpc_cond_timed_wait_impl(cv, &m->mutex, reltime);

	if (is_recursive) {
		m->counter = counter_backup;
	}

	return ret;
}
