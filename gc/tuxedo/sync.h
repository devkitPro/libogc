#pragma once
#include "types.h"
#include "thread.h"

MK_EXTERN_C_START

typedef struct KMutex {
	KThread* owner;
} KMutex;

typedef struct KRMutex {
	KMutex mutex;
	u32 counter;
} KRMutex;

typedef struct KCondVar {
	u8 dummy;
} KCondVar;

MK_INLINE bool KMutexIsLockedByCurrentThread(KMutex* m)
{
	return m->owner == KThreadGetSelf();
}

bool KMutexTryLock(KMutex* m);

void KMutexLock(KMutex* m);

void KMutexUnlock(KMutex* m);

MK_INLINE void KRMutexLock(KRMutex* m)
{
	if (KMutexIsLockedByCurrentThread(&m->mutex)) {
		m->counter ++;
	} else {
		KMutexLock(&m->mutex);
		m->counter = 1;
	}
}

MK_INLINE bool KRMutexTryLock(KRMutex* m)
{
	bool rc;
	if (KMutexIsLockedByCurrentThread(&m->mutex)) {
		m->counter ++;
		rc = true;
	} else {
		rc = KMutexTryLock(&m->mutex);
		if (rc) {
			m->counter = 1;
		}
	}
	return rc;
}

MK_INLINE void KRMutexUnlock(KRMutex* m)
{
	if (!--m->counter) {
		KMutexUnlock(&m->mutex);
	}
}

void KCondVarSignal(KCondVar* cv);

void KCondVarBroadcast(KCondVar* cv);

void KCondVarWait(KCondVar* cv, KMutex* m);

bool KCondVarWaitTimeoutTicks(KCondVar* cv, KMutex* m, u64 timeout_ticks);

MK_INLINE bool KCondVarWaitNs(KCondVar* cv, KMutex* m, u64 timeout_us)
{
	return KCondVarWaitTimeoutTicks(cv, m, PPCNsToTicks(timeout_us));
}

MK_INLINE bool KCondVarWaitUs(KCondVar* cv, KMutex* m, u64 timeout_us)
{
	return KCondVarWaitTimeoutTicks(cv, m, PPCUsToTicks(timeout_us));
}

MK_INLINE bool KCondVarWaitMs(KCondVar* cv, KMutex* m, u64 timeout_ms)
{
	return KCondVarWaitTimeoutTicks(cv, m, PPCMsToTicks(timeout_ms));
}

MK_EXTERN_C_END
