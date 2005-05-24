#ifndef __LWP_THREADQ_H__
#define __LWP_THREADQ_H__

#include <gctypes.h>
#include <lwp_tqdata.h>
#include <lwp_threads.h>
#include <lwp_watchdog.h>

#define LWP_THREADQ_NOTIMEOUT		LWP_WD_NOTIMEOUT

#ifdef __cplusplus
extern "C" {
#endif

lwp_cntrl* __lwp_threadqueue_firstfifo(lwp_thrqueue *);
lwp_cntrl* __lwp_threadqueue_firstpriority(lwp_thrqueue *);
void __lwp_threadqueue_enqueuefifo(lwp_thrqueue *,lwp_cntrl *,u32);
lwp_cntrl* __lwp_threadqueue_dequeuefifo(lwp_thrqueue *);
void __lwp_threadqueue_enqueuepriority(lwp_thrqueue *,lwp_cntrl *,u32);
lwp_cntrl* __lwp_threadqueue_dequeuepriority(lwp_thrqueue *);
void __lwp_threadqueue_init(lwp_thrqueue *,u32,u32,u32);
lwp_cntrl* __lwp_threadqueue_first(lwp_thrqueue *);
void __lwp_threadqueue_enqueue(lwp_thrqueue *,u32);
lwp_cntrl* __lwp_threadqueue_dequeue(lwp_thrqueue *);
void __lwp_threadqueue_flush(lwp_thrqueue *,u32);
void __lwp_threadqueue_extract(lwp_thrqueue *,lwp_cntrl *);
void __lwp_threadqueue_extractfifo(lwp_thrqueue *,lwp_cntrl *);
void __lwp_threadqueue_extractpriority(lwp_thrqueue *,lwp_cntrl *);
u32 __lwp_threadqueue_extractproxy(lwp_cntrl *);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_threadq.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
