#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <gctypes.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct _mutex {
	u32 id;
	void *mutex;
} mutex_t;

s32 LWP_MutexInit(mutex_t *mutex,boolean use_recursive);
s32 LWP_MutexDestroy(mutex_t *mutex);
s32 LWP_MutexLock(mutex_t *mutex);
s32 LWP_MutexTryLock(mutex_t *mutex);
s32 LWP_MutexUnlock(mutex_t *mutex);

#ifdef __cplusplus
	}
#endif

#endif
