#include <string.h>
#include <tuxedo/tick.h>
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>
#include "../lwp_wkspace.h"
#include "../lwp_wkspace.inl"

typedef struct lwpc_alarm {
	KTickTask task;
	alarmcallback cb;
	void* cbarg;
} lwpc_alarm;

static void lwpc_alarm_cb(KTickTask* t)
{
	lwpc_alarm* alarm = (lwpc_alarm*)t;
	alarm->cb((syswd_t)alarm, alarm->cbarg);
}

s32 SYS_CreateAlarm(syswd_t* thealarm)
{
	lwpc_alarm* alarm = __lwp_wkspace_allocate(sizeof(*alarm));
	if (!alarm) {
		return -1;
	}

	memset(alarm, 0, sizeof(*alarm));
	*thealarm = (u32)alarm;
	return 0;
}

static s32 SYS_SetAlarmCommon(syswd_t thealarm, u64 start, u64 period, alarmcallback cb, void* cbarg)
{
	lwpc_alarm* alarm = (void*)thealarm;
	if (!alarm) {
		return -1;
	}

	PPCIrqState st = PPCIrqLockByMsr();
	alarm->cb = cb;
	alarm->cbarg = cbarg;
	KTickTaskStart(&alarm->task, lwpc_alarm_cb, start, period);
	PPCIrqUnlockByMsr(st);
	return 0;
}

s32 SYS_SetAlarm(syswd_t thealarm, const struct timespec* tp, alarmcallback cb, void* cbarg)
{
	return SYS_SetAlarmCommon(thealarm, timespec_to_ticks(tp), 0, cb, cbarg);
}

s32 SYS_SetPeriodicAlarm(syswd_t thealarm, const struct timespec* tp_start, const struct timespec* tp_period, alarmcallback cb, void* cbarg)
{
	return SYS_SetAlarmCommon(thealarm, timespec_to_ticks(tp_start), timespec_to_ticks(tp_period), cb, cbarg);
}

s32 SYS_RemoveAlarm(syswd_t thealarm)
{
	s32 ret = SYS_CancelAlarm(thealarm);
	if (ret >= 0) {
		__lwp_wkspace_free((void*)thealarm);
	}

	return ret;
}

s32 SYS_CancelAlarm(syswd_t thealarm)
{
	lwpc_alarm* alarm = (void*)thealarm;
	if (!alarm) {
		return -1;
	}

	KTickTaskStop(&alarm->task);
	return 0;
}
