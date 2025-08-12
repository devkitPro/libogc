#include <_ansi.h>
#include <_syslist.h>

#include <errno.h>

#include <tuxedo/thread.h>
#include <tuxedo/tick.h>

#include "asm.h"
#include "processor.h"
#include "lwp.h"

#include "exi.h"
#include "system.h"
#include "conf.h"

#include <stdio.h>
#include <sys/time.h>

#include "lwp_watchdog.h"

extern u32 __SYS_GetRTC(u32 *gctime);
extern syssram* __SYS_LockSram(void);
extern u32 __SYS_UnlockSram(u32 write);

void settime(u64 t)
{
	u32 tmp;
	union uulc {
		u64 ull;
		u32 ul[2];
	} v;

	v.ull = t;
	__asm__ __volatile__ (
		"li		%0,0\n\
		 mttbl  %0\n\
		 mttbu  %1\n\
		 mttbl  %2\n"
		 : "=&r" (tmp)
		 : "r" (v.ul[0]), "r" (v.ul[1])
	);
}

void timespec_subtract(const struct timespec *tp_start,const struct timespec *tp_end,struct timespec *result)
{
	struct timespec start_st = *tp_start;
	struct timespec *start = &start_st;
	u32 nsecpersec = TB_NSPERSEC;

	if(tp_end->tv_nsec<start->tv_nsec) {
		int secs = (start->tv_nsec - tp_end->tv_nsec)/nsecpersec+1;
		start->tv_nsec -= nsecpersec * secs;
		start->tv_sec += secs;
	}
	if((tp_end->tv_nsec - start->tv_nsec)>nsecpersec) {
		int secs = (start->tv_nsec - tp_end->tv_nsec)/nsecpersec;
		start->tv_nsec += nsecpersec * secs;
		start->tv_sec -= secs;
	}

	result->tv_sec = (tp_end->tv_sec - start->tv_sec);
	result->tv_nsec = (tp_end->tv_nsec - start->tv_nsec);
}

// this function spins till timeout is reached
void udelay(unsigned us)
{
	unsigned long long start, end;
	start = gettime();
	while (1)
	{
		end = gettime();
		if (diff_usec(start,end) >= us)
			break;
	}
}

int __libogc_nanosleep(const struct timespec *tb, struct timespec *rem)
{
	KThreadSleepTicks(timespec_to_ticks(tb));
	return 0;
}

clock_t clock(void) {
	return -1;
}

int __libogc_gettod_r(struct _reent *ptr, struct timeval *tp, struct timezone *tz)
{
	u64 now;
	if (tp != NULL) {
		now = gettime();
		tp->tv_sec = ticks_to_secs(now) + 946684800;
		tp->tv_usec = tick_microsecs(now);
	}
	if (tz != NULL) {
		tz->tz_minuteswest = 0;
		tz->tz_dsttime = 0;

	}
	return 0;
}

int __syscall_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	if (clock_id != CLOCK_MONOTONIC && clock_id != CLOCK_REALTIME) return EINVAL;

	u64 now = gettime();
	tp->tv_sec = ticks_to_secs(now);
	tp->tv_nsec = tick_nanosecs(now);

	if (clock_id == CLOCK_REALTIME) tp->tv_sec += 946684800;

	return 0;
}

int __syscall_clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	return EPERM;
}

int __syscall_clock_getres(clockid_t clock_id, struct timespec *res)
{
	if (clock_id != CLOCK_MONOTONIC && clock_id != CLOCK_REALTIME) return EINVAL;
	if (res)
	{
		res->tv_sec = 0;
		res->tv_nsec = ticks_to_nanosecs(1);
	}
	return 0;
}

