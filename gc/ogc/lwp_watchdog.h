#ifndef __LWP_WATCHDOG_H__
#define __LWP_WATCHDOG_H__

#include <gctypes.h>
#include <lwp_queue.h>

#define LWP_WD_INACTIVE			0
#define LWP_WD_INSERTED			1
#define LWP_WD_ACTIVE			2
#define LWP_WD_REMOVE			3

#define LWP_WD_FORWARD			0
#define LWP_WD_BACKWARD			1

#define LWP_WD_NOTIMEOUT		0

#ifdef __cplusplus
extern "C" {
#endif

extern vu32 _wd_sync_level;
extern vu32 _wd_sync_count;
extern u32 _wd_ticks_since_boot;

extern lwp_queue _wd_ticks_queue;
extern lwp_queue _wd_secs_queue;

typedef void (*wd_service_routine)(void *);

typedef struct _wdcntrl {
	lwp_node node;
	u32 state;
	u32 init_interval;
	u32 delta_interval;
	u32 start_time;
	u32 stop_time;
	wd_service_routine routine;
	void *usr_data;
} wd_cntrl;

void __lwp_watchdog_init();
void __lwp_wd_insert(lwp_queue *,wd_cntrl *);
u32 __lwp_wd_remove(wd_cntrl *);
void __lwp_wd_tickle(lwp_queue *);
void __lwp_wd_adjust(lwp_queue *,u32,u32);

#include <libogc/lwp_watchdog.inl>

#ifdef __cplusplus
	}
#endif

#endif
