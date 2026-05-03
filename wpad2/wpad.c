/*-------------------------------------------------------------

wpad.c -- Wiimote Application Programmers Interface

Copyright (C) 2008-2026
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)
Zarithya
Alberto Mardegan (mardy)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include "internals.h"

#include "classic.h"
#include "dynamics.h"
#include "event.h"
#include "guitar_hero_3.h"
#include "ir.h"
#include "nunchuk.h"
#include "processor.h"
#include "speaker.h"
#include "tuxedo/sync.h"
#include "tuxedo/tick.h"
#include "wii_board.h"
#include "wiiuse_internal.h"
#include "worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ogcsys.h"

typedef struct {
	WpadThresholds thresholds;
	int idle_time;
} WpadIdleInfo;

#define MAX_REPORT_SIZE 22
typedef struct {
	uint8_t channel;
	WpadEventType type;
	uint8_t report[MAX_REPORT_SIZE];
} WpadStoredEvent;

#define EVENTQUEUE_LENGTH 16
typedef struct {
	KMutex mutex;
	bool connected;
	bool ready;
	bool calibration_data_changed;
	bool exp_calibration_data_changed;
	bool battery_critical;
	bool speaker_enabled;
	uint8_t battery_level;
	uint8_t exp_type;
	union {
		uint8_t exp_subtype; /* when connected */
		uint8_t disconnection_reason; /* when disconnected */
	};
	int idx_head;
	int idx_tail;
	int n_events;
	int n_dropped;
	WpadStoredEvent events[EVENTQUEUE_LENGTH];
	WpadDeviceCalibrationData calibration_data;
	WpadDeviceExpCalibrationData exp_calibration_data;
} WpadEventQueue;

enum {
	WPAD2_HOST_SYNC_BUTTON_REST,
	WPAD2_HOST_SYNC_BUTTON_PRESSED,
	WPAD2_HOST_SYNC_BUTTON_HELD,
};

typedef struct {
	uint8_t host_sync_button_state;
} WpadHostState;

static WpadEventQueue s_event_queue[WPAD2_MAX_DEVICES];
static WpadHostState s_host_state;
static WPADData s_wpaddata[WPAD2_MAX_DEVICES];
static WpadIdleInfo s_idle_info[WPAD2_MAX_DEVICES];
static struct KTickTask s_idle_task;
static int s_idle_timeout_secs = 300;
static accel_t s_calibration[WPAD2_MAX_DEVICES];
static int32_t s_bt_status = WPAD_STATE_DISABLED;

static WPADDisconnectCallback s_disconnect_cb;
static WPADShutdownCallback s_poweroff_cb;
static WPADShutdownCallback s_battery_dead_cb;
static WPADHostSyncBtnCallback s_sync_button_cb;
static WPADStatusCallback s_status_cb;

/* This is invoked as a ISR, so we don't perform lengthy tasks here */
static void idle_cb(KTickTask *task)
{
	for (int chan = 0; chan < WPAD2_MAX_DEVICES; chan++) {
		WpadIdleInfo *idle_info = &s_idle_info[chan];
		if (idle_info->idle_time >= 0)
			idle_info->idle_time++;
	}
}

static s32 reset_cb(s32 final)
{
	WPAD2_DEBUG("final: %d", final);
	if (!final) {
		WPAD_Shutdown();
	}
	return 1;
}

static sys_resetinfo s_reset_info = {
	{},
	reset_cb,
	127
};

static void default_poweroff_cb(s32 chan)
{
	SYS_DoPowerCB();
}

static void default_sync_button_cb(u32 held)
{
	if (held) {
		WPAD_WipeSavedControllers();
	} else {
		WPAD_StartPairing();
	}
}

static void parse_calibration_data(uint8_t channel,
                                   const WpadDeviceCalibrationData *cd)
{
	struct accel_t *accel = &s_calibration[channel];
	const uint8_t *data = cd->data;

	accel->cal_zero.x = (data[0] << 2) | ((data[3] >> 4) & 3);
	accel->cal_zero.y = (data[1] << 2) | ((data[3] >> 2) & 3);
	accel->cal_zero.z = (data[2] << 2) | (data[3] & 3);

	accel->cal_g.x = ((data[4] << 2) | ((data[7] >> 4) & 3)) - accel->cal_zero.x;
	accel->cal_g.y = ((data[5] << 2) | ((data[7] >> 2) & 3)) - accel->cal_zero.y;
	accel->cal_g.z = ((data[6] << 2) | (data[7] & 3)) - accel->cal_zero.z;
	accel->st_alpha = WPAD2_DEFAULT_SMOOTH_ALPHA;
}

/* Note that this is invoked from the worker thread, so access to common data
 * needs to be synchronized */
static void event_cb(const WpadEvent *event)
{
	WpadEventQueue *q = &s_event_queue[event->channel];

	bool yield = false;
	KMutexLock(&q->mutex);
	if (event->type == WPAD2_EVENT_TYPE_CONNECTED) {
		q->connected = true;
		q->exp_subtype = 0;
	} else if (event->type == WPAD2_EVENT_TYPE_CALIBRATION) {
		q->calibration_data = *event->u.calibration_data;
		q->calibration_data_changed = true;
	} else if (event->type == WPAD2_EVENT_TYPE_EXP_CONNECTED) {
		q->exp_type = event->u.exp_connected.exp_type;
		q->exp_subtype = event->u.exp_connected.exp_subtype;
		q->exp_calibration_data = *event->u.exp_connected.calibration_data;
		q->exp_calibration_data_changed = true;
	} else if (event->type == WPAD2_EVENT_TYPE_EXP_DISCONNECTED) {
		q->exp_type = EXP_NONE;
	} else if (event->type == WPAD2_EVENT_TYPE_READY) {
		q->ready = true;
	} else if (event->type == WPAD2_EVENT_TYPE_DISCONNECTED) {
		q->connected = false;
		q->disconnection_reason = event->u.disconnection_reason;
		q->ready = false;
		q->idx_tail = q->idx_head = 0;
		q->n_events = 0;
		if (q->n_dropped) {
			WPAD2_WARNING("Wiimote %d: dropped %d events",
			              event->channel, q->n_dropped);
			q->n_dropped = 0;
		}
	} else if (event->type == WPAD2_EVENT_TYPE_REPORT) {
		q->idx_tail = (q->idx_tail + 1) % EVENTQUEUE_LENGTH;
		if (q->n_events >= EVENTQUEUE_LENGTH) {
			q->idx_head = (q->idx_head + 1) % EVENTQUEUE_LENGTH;
			q->n_dropped++;
		} else {
			if (q->n_events == 0) q->idx_head = q->idx_tail;
			q->n_events++;
		}
		WpadStoredEvent *e = &q->events[q->idx_tail];
		e->channel = event->channel;
		e->type = event->type; /* TODO is this needed? */
		WPAD2_DEBUG("worker rep %d buttons %04x head %d tail %d, count %d", event->u.report.len, *(uint16_t *)(event->u.report.data + 1), q->idx_head, q->idx_tail, q->n_events);
		memcpy(e->report, event->u.report.data, event->u.report.len);
		if (q->n_events >= EVENTQUEUE_LENGTH / 2) yield = true;
	} else if (event->type == WPAD2_EVENT_TYPE_BT_INITIALIZED) {
		s_bt_status = WPAD_STATE_ENABLED;
	} else if (event->type == WPAD2_EVENT_TYPE_HOST_SYNC_BTN) {
		s_host_state.host_sync_button_state = event->u.host_sync_button_held ?
			WPAD2_HOST_SYNC_BUTTON_HELD : WPAD2_HOST_SYNC_BUTTON_PRESSED;
	} else if (event->type == WPAD2_EVENT_TYPE_STATUS) {
		q->battery_level = event->u.status.battery_level;
		q->battery_critical = event->u.status.battery_critical;
		q->speaker_enabled = event->u.status.speaker_enabled;
	}
	KMutexUnlock(&q->mutex);
	if (yield) KThreadYield();
}

s32 WPAD_StartPairing(void)
{
	WPAD2_DEBUG("");

	_wpad2_worker_start_pairing();
	return WPAD_ERR_NONE;
}

s32 WPAD_WipeSavedControllers(void)
{
	WPAD2_DEBUG("");

	for (int i = 0; i < WPAD2_MAX_DEVICES; i++) {
		WPAD_Disconnect(i);
	}

	_wpad2_worker_wipe_saved_controllers();
	return WPAD_ERR_NONE;
}

s32 WPAD_Search(void)
{
	_wpad2_worker_set_search_active(true);
	return WPAD_ERR_NONE;
}

s32 WPAD_StopSearch(void)
{
	_wpad2_worker_set_search_active(false);
	return WPAD_ERR_NONE;
}

s32 WPAD_Init(void)
{
	for (int i = 0; i < ARRAY_SIZE(s_wpaddata); i++) {
		WPADData *data = &s_wpaddata[i];
		_wpad2_ir_set_aspect_ratio(data, WIIUSE_ASPECT_4_3);
		_wpad2_ir_set_position(data, WIIUSE_IR_ABOVE);
		data->err = WPAD_ERR_NOT_READY;
	}

	s_bt_status = WPAD_STATE_ENABLING;
	for (int i = 0; i < ARRAY_SIZE(s_idle_info); i++) {
		s_idle_info[i].thresholds.btns = WPAD_THRESH_DEFAULT_BUTTONS;
		s_idle_info[i].thresholds.ir = WPAD_THRESH_DEFAULT_IR;
		s_idle_info[i].thresholds.acc = WPAD_THRESH_DEFAULT_ACCEL;
		s_idle_info[i].thresholds.js = WPAD_THRESH_DEFAULT_JOYSTICK;
		s_idle_info[i].thresholds.wb = WPAD_THRESH_DEFAULT_BALANCEBOARD;
		s_idle_info[i].thresholds.mp = WPAD_THRESH_DEFAULT_MOTION_PLUS;
		s_idle_info[i].idle_time = -1;
	}
	s_poweroff_cb = default_poweroff_cb;
	s_sync_button_cb = default_sync_button_cb;
	_wpad2_ir_sensor_bar_enable(true);
	_wpad2_worker_start();
	_wpad2_worker_on_event(event_cb);

	u64 ticks_per_sec = PPCMsToTicks(1000);
	KTickTaskStart(&s_idle_task, idle_cb, ticks_per_sec, ticks_per_sec);

	SYS_RegisterResetFunc(&s_reset_info);

	return WPAD_ERR_NONE;
}

s32 WPAD_ReadEvent(s32 chan, WPADData *data)
{
	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	WpadEventQueue *q = &s_event_queue[chan];
	WPADData *our_data = &s_wpaddata[chan];
	WpadIdleInfo *idle_info = &s_idle_info[chan];

	/* Since this mutex blocks the worker thread, let's try to hold it for as
	 * little as possible */
	s32 ret = WPAD_ERR_NONE;
	WpadStoredEvent event;
	union {
		WpadDeviceCalibrationData wiimote;
		WpadDeviceExpCalibrationData exp;
	} calibration_data;
	bool calibration_data_changed = false;
	bool exp_calibration_data_changed = false;
	uint8_t disconnection_reason = 0; /* 0 means we were not disconnected */
	uint8_t exp_old = our_data->exp.type;

	KMutexLock(&q->mutex);

	WpadHostState host_state = s_host_state;
	s_host_state.host_sync_button_state = WPAD2_HOST_SYNC_BUTTON_REST;

	our_data->exp.type = q->exp_type;
	if (q->calibration_data_changed) {
		calibration_data.wiimote = q->calibration_data;
		calibration_data_changed = true;
		q->calibration_data_changed = false;
	} else if (q->exp_calibration_data_changed) {
		/* We have an "else" here in the condition to save some space on the
		 * stack, or we would not be able to use a union for the calibration
		 * data. Even in the unlikely case that we have calibration data for
		 * both the wiimote and the expansion, nothing bad will happen: we'll
		 * process the expansion data in the next call of this function. */
		calibration_data.exp = q->exp_calibration_data;
		if (our_data->exp.type == EXP_CLASSIC) {
			our_data->exp.classic.type = q->exp_subtype;
		}
		exp_calibration_data_changed = true;
		q->exp_calibration_data_changed = false;
	}

	if (!q->connected) {
		ret = WPAD_ERR_NO_CONTROLLER;
		if (our_data->err != WPAD_ERR_NO_CONTROLLER) {
			/* We were disconnected just now */
			disconnection_reason = q->disconnection_reason;
		}
		idle_info->idle_time = -1;
	} else if (!q->ready) {
		ret = WPAD_ERR_NOT_READY;
	} else if (q->n_events == 0) {
		ret = WPAD_ERR_QUEUE_EMPTY;
	} else {
		event = q->events[q->idx_head];
		q->idx_head = (q->idx_head + 1) % EVENTQUEUE_LENGTH;
		q->n_events--;
	}
	our_data->battery_level = q->battery_level;
	KMutexUnlock(&q->mutex);

	if (idle_info->idle_time >= s_idle_timeout_secs) {
		_wpad2_worker_disconnect(chan);
		idle_info->idle_time = -1;
	}

	if (host_state.host_sync_button_state) {
		bool held =
		    host_state.host_sync_button_state == WPAD2_HOST_SYNC_BUTTON_HELD;
		s_sync_button_cb(held);
	}

	if (disconnection_reason != 0) {
		memset(our_data, 0, sizeof(*our_data));
		if (s_disconnect_cb) {
			s_disconnect_cb(chan, disconnection_reason);
		} else if (disconnection_reason == WPAD_DISCON_POWER_OFF) {
			s_poweroff_cb(chan);
		} else if (disconnection_reason == WPAD_DISCON_BATTERY_DIED) {
			if (s_battery_dead_cb) s_battery_dead_cb(chan);
		}
	}

	if (calibration_data_changed) {
		idle_info->idle_time = 0;
		parse_calibration_data(chan, &calibration_data.wiimote);
	}

	if (our_data->exp.type != exp_old) {
		if (our_data->exp.type == EXP_NONE) {
			memset(&our_data->exp, 0, sizeof(our_data->exp));
		}
	}

	if (exp_calibration_data_changed) {
		const WpadDeviceExpCalibrationData *cal = &calibration_data.exp;
		switch (our_data->exp.type) {
		case EXP_NUNCHUK:
			_wpad2_nunchuk_calibrate(&our_data->exp.nunchuk, cal);
			break;
		case EXP_CLASSIC:
			_wpad2_classic_calibrate(&our_data->exp.classic, cal);
			break;
		case EXP_GUITAR_HERO_3:
			_wpad2_guitar_hero_3_calibrate(&our_data->exp.gh3, cal);
			break;
		case EXP_WII_BOARD:
			_wpad2_wii_board_calibrate(&our_data->exp.wb, cal);
			break;
		}
	}

	/* TODO: smoothing */
	if (data) {
		data->err = ret;
		if (ret != WPAD_ERR_NONE) {
			if (ret != WPAD_ERR_QUEUE_EMPTY) {
				our_data->data_present = data->data_present = 0;
				our_data->btns_h = data->btns_h = 0;
				our_data->btns_l = data->btns_l = 0;
				our_data->btns_d = data->btns_d = 0;
				our_data->btns_u = data->btns_u = 0;
			}
			our_data->err = data->err;
			return ret;
		}

		bool changed = _wpad2_event_parse_report(our_data, event.report,
		                                         &idle_info->thresholds, data);
		if (changed) idle_info->idle_time = 0;

		bool smoothed = true;
		_wpad2_calc_data(data, our_data, &s_calibration[chan], smoothed);
		*our_data = *data;
	}
	return ret;
}

s32 WPAD_DroppedEvents(s32 chan)
{
	int dropped = 0;
	s32 ret;

	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD2_MAX_DEVICES; i++)
			if ((ret = WPAD_DroppedEvents(i)) < WPAD_ERR_NONE)
				return ret;
			else
				dropped += ret;
		return dropped;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES)
		return WPAD_ERR_BAD_CHANNEL;

	WpadEventQueue *q = &s_event_queue[chan];
	KMutexLock(&q->mutex);
	if (!q->ready) {
		KMutexUnlock(&q->mutex);
		return WPAD_ERR_NOT_READY;
	}

	dropped = q->n_dropped;
	q->n_dropped = 0;
	KMutexUnlock(&q->mutex);
	return dropped;
}

s32 WPAD_Flush(s32 chan)
{
	s32 ret;
	int count = 0;
	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD_MAX_DEVICES; i++)
			if ((ret = WPAD_Flush(i)) < WPAD_ERR_NONE)
				return ret;
			else
				count += ret;
		return count;
	}

	while ((ret = WPAD_ReadEvent(chan, NULL)) >= 0)
		count++;
	if (ret == WPAD_ERR_QUEUE_EMPTY) return count;
	return ret;
}

s32 WPAD_ReadPending(s32 chan, WPADDataCallback datacb)
{
	s32 ret;
	s32 count = 0;

	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD2_MAX_DEVICES; i++)
			if ((ret = WPAD_ReadPending(i, datacb)) >= WPAD_ERR_NONE)
				count += ret;
		return count;
	}

	u32 btns_p = 0;
	u32 btns_l = 0;
	u32 btns_ev = 0;
	u32 btns_nh = 0;

	WPADData data;
	btns_p = btns_nh = btns_l = s_wpaddata[chan].btns_h;
	while (true) {
		ret = WPAD_ReadEvent(chan, &data);
		if (ret < WPAD_ERR_NONE) break;
		if (datacb)
			datacb(chan, &s_wpaddata[chan]);

		/* we ignore everything except _h, since we have our */
		/* own "fake" _l and everything gets recalculated at */
		/* the end of the function */
		u32 btns_h = data.btns_h;

		/* Button event coalescing:
		 * What we're doing here is get the very first button event
		 * (press or release) for each button. This gets propagated
		 * to the output. Held will therefore report an "old" state
		 * for every button that has changed more than once. This is
		 * intentional: next call to WPAD_ReadPending, if this button
		 * hasn't again changed, the opposite event will fire. This
		 * is the behavior that preserves the most information,
		 * within the limitations of trying to coalesce multiple events
		 * into one. It also keeps the output consistent, if possibly
		 * not fully up to date.
		 */

		/* buttons that changed that haven't changed before */
		u32 btns_ch = (btns_h ^ btns_p) & ~btns_ev;
		btns_p = btns_h;
		/* propagate changes in btns_ch to btns_nd */
		btns_nh = (btns_nh & ~btns_ch) | (btns_h & btns_ch);
		/* store these new changes to btns_ev */
		btns_ev |= btns_ch;

		count++;
	}
	if (ret == WPAD_ERR_QUEUE_EMPTY) {
		data.btns_h = s_wpaddata[chan].btns_h = btns_nh;
		data.btns_l = s_wpaddata[chan].btns_l = btns_l;
		data.btns_d = s_wpaddata[chan].btns_d = btns_nh & ~btns_l;
		data.btns_u = s_wpaddata[chan].btns_u = ~btns_nh & btns_l;
		return count;
	}
	return ret;
}

s32 WPAD_SetDataFormat(s32 chan, s32 fmt)
{
	s32 ret;

	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD2_MAX_DEVICES; i++) {
			if ((ret = WPAD_SetDataFormat(i, fmt)) < WPAD_ERR_NONE)
				return ret;
		}
		return WPAD_ERR_NONE;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	switch (fmt) {
	case WPAD_FMT_BTNS:
	case WPAD_FMT_BTNS_ACC:
	case WPAD_FMT_BTNS_ACC_IR:
		_wpad2_worker_set_format(chan, fmt);
		break;
	default:
		return WPAD_ERR_BADVALUE;
	}
	return WPAD_ERR_NONE;
}

s32 WPAD_SetMotionPlus(s32 chan, u8 enable)
{
	s32 ret;

	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD_MAX_DEVICES; i++)
			if ((ret = WPAD_SetMotionPlus(i, enable)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_wpad2_worker_set_motion_plus(chan, enable);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetVRes(s32 chan, u32 xres, u32 yres)
{
	s32 ret;

	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD2_MAX_DEVICES; i++)
			if ((ret = WPAD_SetVRes(i, xres, yres)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_wpad2_ir_set_vres(&s_wpaddata[chan], xres, yres);
	return WPAD_ERR_NONE;
}

s32 WPAD_GetStatus(void)
{
	return s_bt_status;
}

s32 WPAD_Probe(s32 chan, u32 *type)
{
	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	if (s_bt_status == WPAD_STATE_DISABLED) {
		return WPAD_ERR_NOT_READY;
	}

	WpadEventQueue *q = &s_event_queue[chan];
	KMutexLock(&q->mutex);
	bool connected = q->connected;
	bool ready = q->ready;
	bool exp_type = q->exp_type;
	KMutexUnlock(&q->mutex);

	if (!connected) return WPAD_ERR_NO_CONTROLLER;
	if (!ready) return WPAD_ERR_NOT_READY;

	if (type) *type = exp_type;
	return WPAD_ERR_NONE;
}

[[deprecated]]
s32 WPAD_SetEventBufs(s32 chan, WPADData *bufs, u32 cnt)
{
	WPAD2_WARNING("This function is not implemented (and will never be)");
	return WPAD_ERR_UNKNOWN;
}

[[deprecated]]
void WPAD_SetPowerButtonCallback(WPADShutdownCallback cb)
{
	s_poweroff_cb = cb ? cb : default_poweroff_cb;
}

[[deprecated]]
void WPAD_SetBatteryDeadCallback(WPADShutdownCallback cb)
{
	s_battery_dead_cb = cb;
}

void WPAD_SetDisconnectCallback(WPADDisconnectCallback cb)
{
	s_disconnect_cb = cb;
}

void WPAD_SetHostSyncButtonCallback(WPADHostSyncBtnCallback cb)
{
	s_sync_button_cb = cb ? cb : default_sync_button_cb;
}

void WPAD_SetStatusCallback(WPADStatusCallback cb)
{
	s_status_cb = cb;
}

s32 WPAD_Disconnect(s32 chan)
{
	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	if (s_bt_status != WPAD_STATE_ENABLED) return WPAD_ERR_NOT_READY;

	_wpad2_worker_disconnect(chan);
	/* Wait for disconnection */
	WpadEventQueue *q = &s_event_queue[chan];
	for (int i = 0; i < 3000; i++) {
		KMutexLock(&q->mutex);
		bool connected = q->connected;
		KMutexUnlock(&q->mutex);
		if (!connected) break;
		usleep(50);
	}
	return WPAD_ERR_NONE;
}

s32 WPAD_Shutdown(void)
{
	s_bt_status = WPAD_STATE_DISABLED;

	KTickTaskStop(&s_idle_task);

	_wpad2_worker_stop();
	_wpad2_ir_sensor_bar_enable(false);
	s_disconnect_cb = NULL;
	s_poweroff_cb = NULL;
	s_battery_dead_cb = NULL;
	s_sync_button_cb = NULL;
	s_status_cb = NULL;
	return WPAD_ERR_NONE;
}

void WPAD_SetIdleTimeout(u32 seconds)
{
	s_idle_timeout_secs = seconds;
}

s32 WPAD_ScanPads(void)
{
	return WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
}

s32 WPAD_Rumble(s32 chan, int status)
{
	s32 ret;
	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD_MAX_DEVICES; i++)
			if ((ret = WPAD_Rumble(i, status)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) {
		return WPAD_ERR_BAD_CHANNEL;
	}

	if (s_bt_status == WPAD_STATE_DISABLED) {
		return WPAD_ERR_NOT_READY;
	}
	_wpad2_worker_set_rumble(chan, status);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetIdleThresholds(s32 chan, s32 btns, s32 ir, s32 accel, s32 js, s32 wb, s32 mp)
{
	s32 ret;
	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD_MAX_DEVICES; i++)
			if ((ret = WPAD_SetIdleThresholds(i, btns, ir, accel, js, wb, mp)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) {
		return WPAD_ERR_BAD_CHANNEL;
	}

	if (s_bt_status == WPAD_STATE_DISABLED) {
		return WPAD_ERR_NOT_READY;
	}

	s_idle_info[chan].thresholds.btns = btns;
	s_idle_info[chan].thresholds.ir = ir;
	s_idle_info[chan].thresholds.acc = accel;
	s_idle_info[chan].thresholds.js = js;
	s_idle_info[chan].thresholds.wb = wb;
	s_idle_info[chan].thresholds.mp = mp;
	return WPAD_ERR_NONE;
}

s32 WPAD_ControlSpeaker(s32 chan, s32 enable)
{
	if (s_bt_status == WPAD_STATE_DISABLED) {
		return WPAD_ERR_NOT_READY;
	}

	s32 ret;
	if (chan == WPAD_CHAN_ALL) {
		for (int i = WPAD_CHAN_0; i < WPAD_MAX_DEVICES; i++)
			if ((ret = WPAD_ControlSpeaker(i, enable)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_wpad2_worker_set_speaker(chan, enable);
	return WPAD_ERR_NONE;
}

s32 WPAD_IsSpeakerEnabled(s32 chan)
{
	if (s_bt_status == WPAD_STATE_DISABLED) {
		return WPAD_ERR_NOT_READY;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	WpadEventQueue *q = &s_event_queue[chan];
	KMutexLock(&q->mutex);
	bool enabled = q->speaker_enabled;
	KMutexUnlock(&q->mutex);
	return enabled ? WPAD_ERR_NONE : WPAD_ERR_NOT_READY;
}

s32 WPAD_SendStreamData(s32 chan, void *buf, u32 len)
{
	if (s_bt_status == WPAD_STATE_DISABLED) {
		return WPAD_ERR_NOT_READY;
	}

	if (chan < WPAD_CHAN_0 || chan >= WPAD2_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	WPAD2_DEBUG("Sending buffer %p, len %d", buf, len);
	_wpad2_worker_play_sound(chan, buf, (int)len);
	return WPAD_ERR_NONE;
}

void WPAD_EncodeData(WPADEncStatus *info, u32 flag,
                     const s16 *pcm_samples, s32 num_samples, u8 *out)
{
	_wpad2_speaker_encode((WENCStatus*)info, flag, pcm_samples, num_samples, out);
}

WPADData *WPAD_Data(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return NULL;
	return &s_wpaddata[chan];
}

u8 WPAD_BatteryLevel(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return 0;
	return s_wpaddata[chan].battery_level;
}

u32 WPAD_ButtonsUp(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return 0;
	return s_wpaddata[chan].btns_u;
}

u32 WPAD_ButtonsDown(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return 0;
	return s_wpaddata[chan].btns_d;
}

u32 WPAD_ButtonsHeld(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return 0;
	return s_wpaddata[chan].btns_h;
}

void WPAD_IR(int chan, struct ir_t *ir)
{
	if (chan < 0 || chan >= WPAD_MAX_DEVICES || !ir) return;
	*ir = s_wpaddata[chan].ir;
}

void WPAD_Orientation(int chan, struct orient_t *orient)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES || !orient) return;
	*orient = s_wpaddata[chan].orient;
}

void WPAD_GForce(int chan, struct gforce_t *gforce)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES || !gforce) return;
	*gforce = s_wpaddata[chan].gforce;
}

void WPAD_Accel(int chan, struct vec3w_t *accel)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES || !accel) return;
	*accel = s_wpaddata[chan].accel;
}

void WPAD_Expansion(int chan, struct expansion_t *exp)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES || !exp) return;
	*exp = s_wpaddata[chan].exp;
}

void WPAD_PadStatus(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return;

	WpadEventQueue *q = &s_event_queue[chan];
	KMutexLock(&q->mutex);
	bool connected = q->connected;
	KMutexUnlock(&q->mutex);

	if (connected && s_status_cb) {
		s_status_cb(chan);
	}
}

bool WPAD_IsBatteryCritical(int chan)
{
	if (chan < 0 || chan >= WPAD2_MAX_DEVICES) return false;

	WpadEventQueue *q = &s_event_queue[chan];
	KMutexLock(&q->mutex);
	bool battery_critical = q->battery_critical;
	KMutexUnlock(&q->mutex);
	return battery_critical;
}
