#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* sem_t;

s32 LWP_SemInit(sem_t *sem,u32 start,u32 max);
s32 LWP_SemDestroy(sem_t sem);
s32 LWP_SemWait(sem_t sem);
s32 LWP_SemPost(sem_t sem);

#ifdef __cplusplus
	}
#endif

#endif
