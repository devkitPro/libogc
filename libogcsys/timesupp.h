#ifndef __TIMESUPP_H__
#define __TIMESUPP_H__

#include <sys/types.h>

#define TB_BUS_CLOCK		162000000u
#define TB_CORE_CLOCK		486000000u
#define TB_TIMER_CLOCK		(TB_BUS_CLOCK/4000)			//4th of the bus frequency

#define TB_REQ				250
#define TB_SUCCESSFUL		0

#define ticks_to_cycles(ticks)		(((ticks)*((TB_CORE_CLOCK*2)/TB_TIMER_CLOCK))/2)
#define ticks_to_secs(ticks)		((ticks)/TB_TIMER_CLOCK)
#define ticks_to_millisecs(ticks)	((ticks)/(TB_TIMER_CLOCK))
#define ticks_to_microsecs(ticks)	(((ticks)*8)/(TB_TIMER_CLOCK/125))
#define ticks_to_nanosecs(ticks)	(((ticks)*8000)/(TB_TIMER_CLOCK/125))

#define secs_to_ticks(sec)			((sec)*TB_TIMER_CLOCK)
#define millisecs_to_ticks(msec)	((msec)*(TB_TIMER_CLOCK))
#define microsecs_to_ticks(usec)	(((usec)*(TB_TIMER_CLOCK/125))/8)
#define nanosecs_to_ticks(nsec)		(((nsec)*(TB_TIMER_CLOCK/125))/8000)

#define diff_ticks(tick0,tick1)		((signed long long)tick0 - (signed long long)tick1)

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

time_t time(time_t *timer);
unsigned int nanosleep(struct timespec *tb);

#endif
