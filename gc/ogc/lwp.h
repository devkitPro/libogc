#ifndef __LWP_H__
#define __LWP_H__

#include <gctypes.h>

#define LWP_SUCCESSFUL				0
#define LWP_ALLREADY_SUSPENDED		1
#define LWP_NOT_SUSPENDED			2

#define LWP_PRIO_IDLE				0
#define LWP_PRIO_HIGHEST		  127

#ifdef __cplusplus
extern "C" {
#endif

typedef void* lwp_t;
typedef void* lwpq_t;

u32 LWP_CreateThread(lwp_t *thethread,void* (*entry)(void *),void *arg,void *stackbase,u32 stack_size,u8 prio);
u32 LWP_SuspendThread(lwp_t thethread);
u32 LWP_ResumeThread(lwp_t thethread);
lwp_t LWP_GetSelf();
void LWP_SetThreadPriority(lwp_t thethread,u32 prio);
void LWP_YieldThread();
void LWP_CloseThread(lwp_t thethread);
u32 LWP_JoinThread(lwp_t thethread,void **value_ptr);
void LWP_InitQueue(lwpq_t *thequeue);
void LWP_CloseQueue(lwpq_t thequeue);
void LWP_SleepThread(lwpq_t thequeue);
void LWP_WakeThread(lwpq_t thequeue);

#ifdef __cplusplus
	}
#endif

#endif
