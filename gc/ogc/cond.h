#ifndef __COND_H__
#define __COND_H__

#include <gctypes.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* cond_t;

u32 LWP_CondInit(cond_t *cond);
u32 LWP_CondWait(cond_t cond,mutex_t *mutex);
u32 LWP_CondSignal(cond_t cond);
u32 LWP_CondBroadcast(cond_t cond);
u32 LWP_CondTimedWait(cond_t cond,mutex_t *mutex,const struct timespec *abstime);
u32 LWP_CondDestroy(cond_t cond);

#ifdef __cplusplus
	}
#endif

#endif
