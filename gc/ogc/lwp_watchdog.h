#ifndef __LWP_WATCHDOG_H__
#define __LWP_WATCHDOG_H__

#include <gctypes.h>
#include <lwp_queue.h>
#include <time.h>

#define TB_BUS_CLOCK				162000000u
#define TB_CORE_CLOCK				486000000u
#define TB_TIMER_CLOCK				(TB_BUS_CLOCK/4000)			//4th of the bus frequency

#define TB_SECSPERMIN				60
#define TB_MINSPERHR				60
#define TB_MONSPERYR				12
#define TB_DAYSPERYR				365
#define TB_HRSPERDAY				24
#define TB_SECSPERDAY				(TB_SECSPERMIN*TB_MINSPERHR*TB_HRSPERDAY)
#define TB_SECSPERNYR				(365*TB_SECSPERDAY)
								
#define TB_MSPERSEC					1000
#define TB_USPERSEC					1000000
#define TB_NSPERSEC					1000000000
#define TB_NSPERMS					1000000
#define TB_NSPERUS					1000
#define TB_USPERTICK				10000

#define ticks_to_cycles(ticks)		((u32)(((u64)(ticks)*((TB_CORE_CLOCK*2)/TB_TIMER_CLOCK))/2))
#define ticks_to_secs(ticks)		((u32)((u64)(ticks)/(TB_TIMER_CLOCK*1000)))
#define ticks_to_millisecs(ticks)	((u32)((u64)(ticks)/(TB_TIMER_CLOCK)))
#define ticks_to_microsecs(ticks)	((u32)(((u64)(ticks)*8)/(TB_TIMER_CLOCK/125)))
#define ticks_to_nanosecs(ticks)	((u32)(((u64)(ticks)*8000)/(TB_TIMER_CLOCK/125)))

#define secs_to_ticks(sec)			((u64)(sec)*(TB_TIMER_CLOCK*1000))
#define millisecs_to_ticks(msec)	((u64)(msec)*(TB_TIMER_CLOCK))
#define microsecs_to_ticks(usec)	(((u64)(usec)*(TB_TIMER_CLOCK/125))/8)
#define nanosecs_to_ticks(nsec)		(((u64)(nsec)*(TB_TIMER_CLOCK/125))/8000)

#define diff_ticks(tick0,tick1)		((signed long long)tick0 - (signed long long)tick1)

#define LWP_WD_INACTIVE				0
#define LWP_WD_INSERTED				1
#define LWP_WD_ACTIVE				2
#define LWP_WD_REMOVE				3
								
#define LWP_WD_FORWARD				0
#define LWP_WD_BACKWARD				1
								
#define LWP_WD_NOTIMEOUT			0

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
	u64 init_interval;
	u64 delta_interval;
	u64 start_time;
	u64 stop_time;
	wd_service_routine routine;
	void *usr_data;
} wd_cntrl;

void __lwp_watchdog_init();
void __lwp_watchdog_settimer(wd_cntrl *wd);
void __lwp_wd_insert(lwp_queue *,wd_cntrl *);
u32 __lwp_wd_remove(wd_cntrl *);
void __lwp_wd_tickle(lwp_queue *);
void __lwp_wd_adjust(lwp_queue *,u32,u64);

#include <libogc/lwp_watchdog.inl>

#ifdef __cplusplus
	}
#endif

#endif
