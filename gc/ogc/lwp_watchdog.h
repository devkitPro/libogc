#ifndef __LWP_WATCHDOG_H__
#define __LWP_WATCHDOG_H__

#include <gctypes.h>
#include <gcbool.h>

#include "../ogcsys.h"
#include "../tuxedo/ppc/clock.h"

#include <time.h>

#define ticks_to_cycles(ticks)		((((u64)(ticks)*(u64)((TB_CORE_CLOCK*2)/TB_TIMER_CLOCK))/2))
#define ticks_to_secs(ticks)		((u64)(ticks)/PPC_TIMER_CLOCK)
#define ticks_to_millisecs(ticks)	PPCTicksToMs(ticks)
#define ticks_to_microsecs(ticks)	PPCTicksToUs(ticks)
#define ticks_to_nanosecs(ticks)	PPCTicksToNs(ticks)

#define tick_microsecs(ticks)		(PPCTicksToUs(ticks)%TB_USPERSEC)
#define tick_nanosecs(ticks)		(PPCTicksToNs(ticks)%TB_NSPERSEC)

#define secs_to_ticks(sec)			((u64)(sec)*PPC_TIMER_CLOCK)
#define millisecs_to_ticks(msec)	PPCMsToTicks(msec)
#define microsecs_to_ticks(usec)	PPCUsToTicks(usec)
#define nanosecs_to_ticks(nsec)		PPCNsToTicks(nsec)

#define diff_ticks(tick0,tick1)		(((u64)(tick1)<(u64)(tick0))?((u64)-1-(u64)(tick0)+(u64)(tick1)):((u64)(tick1)-(u64)(tick0)))

#ifdef __cplusplus
extern "C" {
#endif

MK_INLINE u32 gettick(void)
{
	return PPCMftb();
}

MK_INLINE u64 gettime(void)
{
	return PPCGetTickCount();
}

extern void settime(u64);

MK_INLINE u64 timespec_to_ticks(const struct timespec *tp)
{
	u64 ticks = 0;
	ticks += tp->tv_sec * PPC_TIMER_CLOCK;
	ticks += PPCNsToTicks(tp->tv_nsec);
	return ticks;
}

MK_INLINE u32 diff_sec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start, end);
	return diff / PPC_TIMER_CLOCK;
}

MK_INLINE u32 diff_msec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start, end);
	return PPCTicksToMs(diff);
}

MK_INLINE u32 diff_usec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start, end);
	return PPCTicksToUs(diff);
}

MK_INLINE u32 diff_nsec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start, end);
	return PPCTicksToNs(diff);
}

#ifdef __cplusplus
	}
#endif

#endif
