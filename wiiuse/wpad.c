/*-------------------------------------------------------------

wpad.c -- Wiimote Application Programmers Interface

Copyright (C) 2008-2025
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)
Zarithya

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "asm.h"
#include "processor.h"
#include "bte.h"
#include "conf.h"
#include "ir.h"
#include "speaker.h"
#include "dynamics.h"
#include "guitar_hero_3.h"
#include "wiiboard.h"
#include "wiiuse_internal.h"
#include "wiiuse/wpad.h"

#include "ogcsys.h"

#include "lwp_threads.inl"

#define MAX_STREAMDATA_LEN				20
#define EVENTQUEUE_LENGTH				16

struct _wpad_thresh {
	s32 btns;
	s32 ir;
	s32 js;
	s32 acc;
	s32 wb;
	s32 mp;
};

struct _wpad_cb {
	wiimote *wm;
	s32 data_fmt;
	s32 queue_head;
	s32 queue_tail;
	s32 queue_full;
	u32 queue_length;
	u32 dropped_events;
	s32 idle_time;
	s32 speaker_enabled;
	struct _wpad_thresh thresh;

	void *sound_data;
	u32 sound_len;
	u32 sound_off;
	syswd_t sound_alarm;

	WPADData lstate;
	WPADData *queue_ext;
	WPADData queue_int[EVENTQUEUE_LENGTH];
};

static syswd_t __wpad_timer;
static vu32 __wpads_inited = 0;
static vs32 __wpads_bonded = 0;
static u32 __wpad_idletimeout = 300;
static vu32 __wpads_active = 0;
static vu32 __wpads_used = 0;
static wiimote **__wpads = NULL;
static wiimote_listen __wpads_listen[WPAD_MAX_DEVICES] = {0};
static WPADData wpaddata[WPAD_MAX_DEVICES] = {0};
static struct _wpad_cb __wpdcb[WPAD_MAX_DEVICES] = {0};
static conf_pads __wpad_devs = {0};
static conf_pad_guests __wpad_guests = {0};
static struct linkkey_info __wpad_keys[CONF_PAD_MAX_REGISTERED] = {0};
static struct linkkey_info __wpad_guest_keys[CONF_PAD_ACTIVE_AND_OTHER] = {0};

static sem_t __wpad_confsave_sem;
static bool __wpad_confsave_thread_running = false;
static void* __wpad_confsave_thread_func(void *);
static lwp_t __wpad_confsave_thread = LWP_THREAD_NULL;

static s32 __wpad_onreset(s32 final);
static s32 __wpad_disconnect(struct _wpad_cb *wpdcb);
static void __wpad_eventCB(struct wiimote_t *wm,s32 event);

static s32 GetGuestSlot(struct bd_addr *pad_addr);
static s32 GetRegisteredSlot(struct bd_addr *pad_addr);

static void __wpad_def_powcb(s32 chan);
static void __wpad_def_hostsynccb(u32 held);
static WPADShutdownCallback __wpad_batcb = NULL;
static WPADShutdownCallback __wpad_powcb = __wpad_def_powcb;
static WPADDisconnectCallback __wpad_disconncb = NULL;
static WPADHostSyncBtnCallback __wpad_hostsynccb = __wpad_def_hostsynccb;
static WPADStatusCallback __wpad_statuscb = NULL;

extern void __wiiuse_sensorbar_enable(int enable);

static sys_resetinfo __wpad_resetinfo = {
	{},
	__wpad_onreset,
	127
};

static s32 __wpad_onreset(s32 final)
{
	WIIUSE_DEBUG("__wpad_onreset(%d)",final);
	if(final==FALSE) {
		WPAD_Shutdown();
	}
	return 1;
}

static void __wpad_def_powcb(s32 chan)
{
	SYS_DoPowerCB();
}

static void __wpad_timeouthandler(syswd_t alarm,void *cbarg)
{
	s32 i;
	struct wiimote_t *wm = NULL;
	struct _wpad_cb *wpdcb = NULL;

	if(!__wpads_active) return;

	__lwp_thread_dispatchdisable();
	for(i=0;i<WPAD_MAX_DEVICES;i++) {
		wpdcb = &__wpdcb[i];
		wm = wpdcb->wm;
		if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
			wpdcb->idle_time++;
			if(wpdcb->idle_time>=__wpad_idletimeout) {
				wpdcb->idle_time = 0;
				wiiuse_disconnect(wm);
			}
		}
	}
	__lwp_thread_dispatchunnest();
}

static void __wpad_sounddata_alarmhandler(syswd_t alarm,void *cbarg)
{
	u8 *snd_data;
	u32 snd_off;
	struct wiimote_t *wm;
	struct _wpad_cb *wpdcb = (struct _wpad_cb*)cbarg;

	if(!wpdcb) return;

	if(wpdcb->sound_off>=wpdcb->sound_len) {
		wpdcb->sound_data = NULL;
		wpdcb->sound_len = 0;
		wpdcb->sound_off = 0;
		SYS_CancelAlarm(wpdcb->sound_alarm);
		return;
	}

	wm = wpdcb->wm;
	snd_data = wpdcb->sound_data;
	snd_off = wpdcb->sound_off;
	wpdcb->sound_off += MAX_STREAMDATA_LEN;
	wiiuse_write_streamdata(wm,(snd_data+snd_off),MAX_STREAMDATA_LEN,NULL);
}

static void __wpad_setfmt(s32 chan)
{
	switch(__wpdcb[chan].data_fmt) {
		case WPAD_FMT_BTNS:
			wiiuse_set_flags(__wpads[chan], 0, WIIUSE_CONTINUOUS);
			wiiuse_motion_sensing(__wpads[chan],0);
			if(chan != WPAD_BALANCE_BOARD) wiiuse_set_ir(__wpads[chan],0);
			break;
		case WPAD_FMT_BTNS_ACC:
			wiiuse_set_flags(__wpads[chan], WIIUSE_CONTINUOUS, 0);
			wiiuse_motion_sensing(__wpads[chan],1);
			if(chan != WPAD_BALANCE_BOARD) wiiuse_set_ir(__wpads[chan],0);
			break;
		case WPAD_FMT_BTNS_ACC_IR:
			wiiuse_set_flags(__wpads[chan], WIIUSE_CONTINUOUS, 0);
			wiiuse_motion_sensing(__wpads[chan],1);
			if(chan != WPAD_BALANCE_BOARD) wiiuse_set_ir(__wpads[chan],1);
			break;
		default:
			break;
	}
}

wiimote *__wpad_assign_slot(wiimote_listen *wml, u8 err)
{
	u32 i, level;

	_CPU_ISR_Disable(level);

	for(i=0; i<WPAD_MAX_DEVICES; i++) {
		if(wml == &__wpads_listen[i]) {
			break;
		}
	}

	if (i >= WPAD_MAX_DEVICES)
	{
		WIIUSE_ERROR("WPAD Slot %d Not Valid\n", i);
		_CPU_ISR_Restore(level);
		return NULL;
	}

	if (err) {
		WIIUSE_ERROR("WPAD Connection Error %d\n", (s8)err);
		__wpads_used &= ~(0x01<<i);
		_CPU_ISR_Restore(level);
		return NULL;
	}
	
	WIIUSE_DEBUG("WPAD Assigning Slot %d (used: 0x%02x)", i, __wpads_used);
	if(i>=WPAD_BALANCE_BOARD) {
		BYTES_FROM_BD_ADDR(__wpad_devs.balance_board.bdaddr, &wml->bdaddr);
		memcpy(__wpad_devs.balance_board.name, wml->name, sizeof(__wpad_devs.balance_board.name));
	} else {
		BYTES_FROM_BD_ADDR(__wpad_devs.active[i].bdaddr, &wml->bdaddr);
	}
	LWP_SemPost(__wpad_confsave_sem);
	_CPU_ISR_Restore(level);
	return __wpads[i];
}

static void* __wpad_confsave_thread_func(void *)
{
	__wpad_confsave_thread_running = true;
	
	while (__wpad_confsave_thread_running)
	{
		CONF_SetPadDevices(&__wpad_devs);
		CONF_SaveChanges();
		LWP_SemWait(__wpad_confsave_sem);
	}

	return NULL;
}

static void __wpad_confsave_thread_stop(void)
{
	if (__wpad_confsave_sem != LWP_SEM_NULL) {
		if (__wpad_confsave_thread != LWP_THREAD_NULL) {
			__wpad_confsave_thread_running = false;
			LWP_SemPost(__wpad_confsave_sem);
			LWP_JoinThread(__wpad_confsave_thread, NULL);
			__wpad_confsave_thread = LWP_THREAD_NULL;
		}

		LWP_SemDestroy(__wpad_confsave_sem);
		__wpad_confsave_sem = LWP_SEM_NULL;
	}
}

wiimote *__wpad_register_new(wiimote_listen *wml, u8 err)
{
	u32 i,j,level;
	wiimote *wm;

	int guestEntry = CONF_PAD_MAX_ACTIVE;
	int registeredEntry = CONF_PAD_MAX_REGISTERED;

	_CPU_ISR_Disable(level);
	wm = __wpad_assign_slot(wml, err);
	if (wm)
	{
		guestEntry = GetGuestSlot(&wml->bdaddr);
		registeredEntry = GetRegisteredSlot(&wml->bdaddr);

		if(BTE_GetPairMode() == PAIR_MODE_NORMAL) {
			// Permanent pair, need to save bdaddr as known controller
			if(registeredEntry >= __wpad_devs.num_registered) {
				// If currently guest, we want to allow for upgrade to permanent pair
				if(guestEntry < CONF_PAD_MAX_ACTIVE) {
					// Wipe guest entry and shift later ones up
					for(i = guestEntry; i < (__wpad_guests.num_guests - 1); i++) {
						memcpy(&__wpad_guests.guests[i], &__wpad_guests.guests[i+1], sizeof(conf_pad_guest_device));
					}
					memset(&__wpad_guests.guests[--__wpad_guests.num_guests], 0, sizeof(conf_pad_guest_device));
				}
		
				WIIUSE_DEBUG("Registering Wiimote '%s' with system...", wml->name);
				memmove(&__wpad_devs.registered[1], &__wpad_devs.registered[0], sizeof(conf_pad_device)*(CONF_PAD_MAX_REGISTERED-1));
				BYTES_FROM_BD_ADDR(__wpad_devs.registered[0].bdaddr, &wml->bdaddr);
				memcpy(__wpad_devs.registered[0].name, wml->name, sizeof(__wpad_devs.registered[0].name));
				if (__wpad_devs.num_registered < CONF_PAD_MAX_REGISTERED)
					__wpad_devs.num_registered++;
				LWP_SemPost(__wpad_confsave_sem);
			}
		} else if(registeredEntry >= CONF_PAD_MAX_REGISTERED) {
			// Not permanent pair, need to save bdaddr and link key as guest controller
			if(guestEntry >= CONF_PAD_MAX_ACTIVE) {
				WIIUSE_DEBUG("Saving Wiimote '%s' as guest...", wml->name);
				memmove(&__wpad_guests.guests[1], &__wpad_guests.guests[0], sizeof(conf_pad_guest_device)*(CONF_PAD_MAX_ACTIVE-1));
				BYTES_FROM_BD_ADDR(__wpad_guests.guests[0].bdaddr, &wml->bdaddr);
				memcpy(__wpad_guests.guests[0].name, wml->name, sizeof(__wpad_guests.guests[0].name));
				if (__wpad_guests.num_guests < CONF_PAD_MAX_ACTIVE)
					__wpad_guests.num_guests++;
	
				for(i=0; i<CONF_PAD_MAX_ACTIVE; i++) {
					if(bd_addr_cmp(&wml->bdaddr,&__wpad_guest_keys[i].bdaddr)) {
						u8 *src = __wpad_guest_keys[i].key;
						u8 *dst = __wpad_guests.guests[0].link_key;
						// Keys are stored in config backwards, like bdaddrs
						for (j=0;j<BD_LINK_KEY_LEN;j++) {
							dst[j] = src[BD_LINK_KEY_LEN - 1 - j];
						}
						break;
					}
				}
			}
		}
	}

	_CPU_ISR_Restore(level);

	return wm;
}

static s32 __wpad_setevtfilter_finished(s32 result,void *usrdata)
{
	u32 level;

	WIIUSE_DEBUG("__wpad_setevtfilter_finished(%d)",result);

	_CPU_ISR_Disable(level);
	if((result==ERR_OK) && (__wpads_inited==WPAD_STATE_ENABLING)) {
		__wpads_inited = WPAD_STATE_ENABLED;
	}
	_CPU_ISR_Restore(level);
	return ERR_OK;
}

static s32 __wpad_init_finished(s32 result,void *usrdata)
{
	u32 level;
	
	WIIUSE_DEBUG("__wpad_init_finished(%d)",result);

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_ENABLING) {
		BTE_SetEvtFilter(0x01,0x00,NULL,__wpad_setevtfilter_finished);
	}
	_CPU_ISR_Restore(level);
	return ERR_OK;
}

static s32 __wpad_patch_finished(s32 result,void *usrdata)
{
	u32 level;
	
	WIIUSE_DEBUG("__wpad_patch_finished(%d)",result);

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_ENABLING) {
		BTE_InitSub(__wpad_init_finished);
	}
	_CPU_ISR_Restore(level);
	return ERR_OK;
}

static s32 __readlinkkey_finished(s32 result,void *usrdata)
{
	u32 i, j, level;
	
	WIIUSE_DEBUG("__readlinkkey_finished(%d)",result);
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_ENABLING) {
		__wpads_bonded = result;
 
		for(i=0;i<__wpad_guests.num_guests;i++) {
			BD_ADDR_FROM_BYTES(&__wpad_guest_keys[i].bdaddr, __wpad_guests.guests[i].bdaddr);
			u8 *src = __wpad_guests.guests[i].link_key;
			u8 *dst = __wpad_guest_keys[i].key;
			
			// Keys are stored in config backwards, like bdaddrs
			for (j=0;j<BD_LINK_KEY_LEN;j++) {
				dst[j] = src[BD_LINK_KEY_LEN - 1 - j];
			}
		}
		BTE_ApplyPatch(__wpad_patch_finished);
	}
	_CPU_ISR_Restore(level);
	return ERR_OK;
}

static s32 __initcore_finished(s32 result,void *usrdata)
{
	u32 level;
	
	WIIUSE_DEBUG("__initcore_finished(%d)",result);
	
	_CPU_ISR_Disable(level);
	if((result==ERR_OK) && (__wpads_inited==WPAD_STATE_ENABLING)) {
		BTE_ReadStoredLinkKey(__wpad_keys,CONF_PAD_MAX_REGISTERED,__readlinkkey_finished);
	}
	_CPU_ISR_Restore(level);
	return ERR_OK;
}

static s32 __wpad_disconnect(struct _wpad_cb *wpdcb)
{
	struct wiimote_t *wm;

	WIIUSE_DEBUG("__wpad_disconnect");

	if(wpdcb==NULL) return 0;

	wm = wpdcb->wm;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		wiiuse_disconnect(wm);
	}

	return 0;
}

static void __wpad_calc_data(WPADData *data,WPADData *lstate,struct accel_t *accel_calib,u32 smoothed)
{
	if(data->err!=WPAD_ERR_NONE) return;

	data->orient = lstate->orient;

	data->ir.state = lstate->ir.state;
	data->ir.sensorbar = lstate->ir.sensorbar;
	data->ir.x = lstate->ir.x;
	data->ir.y = lstate->ir.y;
	data->ir.sx = lstate->ir.sx;
	data->ir.sy = lstate->ir.sy;
	data->ir.ax = lstate->ir.ax;
	data->ir.ay = lstate->ir.ay;
	data->ir.distance = lstate->ir.distance;
	data->ir.z = lstate->ir.z;
	data->ir.angle = lstate->ir.angle;
	data->ir.error_cnt = lstate->ir.error_cnt;
	data->ir.glitch_cnt = lstate->ir.glitch_cnt;

	data->btns_l = lstate->btns_h;
	if(data->data_present & WPAD_DATA_ACCEL) {
		calculate_orientation(accel_calib, &data->accel, &data->orient, smoothed);
		calculate_gforce(accel_calib, &data->accel, &data->gforce);
	}
	if(data->data_present & WPAD_DATA_IR) {
		interpret_ir_data(&data->ir,&data->orient);
	}
	if(data->data_present & WPAD_DATA_EXPANSION) {
		switch(data->exp.type) {
			case EXP_NUNCHUK:
			{
				struct nunchuk_t *nc = &data->exp.nunchuk;

				nc->orient = lstate->exp.nunchuk.orient;
				calc_joystick_state(&nc->js,nc->js.pos.x,nc->js.pos.y);
				calculate_orientation(&nc->accel_calib,&nc->accel,&nc->orient,smoothed);
				calculate_gforce(&nc->accel_calib,&nc->accel,&nc->gforce);
				data->btns_h |= (data->exp.nunchuk.btns<<16);
			}
			break;

			case EXP_CLASSIC:
			{
				struct classic_ctrl_t *cc = &data->exp.classic;

				cc->r_shoulder = ((f32)cc->rs_raw/0x1F);
				cc->l_shoulder = ((f32)cc->ls_raw/0x1F);
				calc_joystick_state(&cc->ljs, cc->ljs.pos.x, cc->ljs.pos.y);
				calc_joystick_state(&cc->rjs, cc->rjs.pos.x, cc->rjs.pos.y);

				// overwrite Wiimote buttons (unused) with extra Wii U Pro Controller stick buttons
				if (data->exp.classic.type == CLASSIC_TYPE_WIIU)
					data->btns_h = (data->exp.classic.btns & WII_U_PRO_CTRL_BUTTON_EXTRA) >> 16;

				data->btns_h |= ((data->exp.classic.btns & CLASSIC_CTRL_BUTTON_ALL)<<16);
			}
			break;

			case EXP_GUITAR_HERO_3:
			{
				struct guitar_hero_3_t *gh3 = &data->exp.gh3;

				gh3->touch_bar = 0;
				if (gh3->tb_raw > 0x1B)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_ORANGE;
				else if (gh3->tb_raw > 0x18)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_ORANGE | GUITAR_HERO_3_TOUCH_BLUE;
				else if (gh3->tb_raw > 0x15)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_BLUE;
				else if (gh3->tb_raw > 0x13)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_BLUE | GUITAR_HERO_3_TOUCH_YELLOW;
				else if (gh3->tb_raw > 0x10)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_YELLOW;
				else if (gh3->tb_raw > 0x0D)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_AVAILABLE;
				else if (gh3->tb_raw > 0x0B)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_YELLOW | GUITAR_HERO_3_TOUCH_RED;
				else if (gh3->tb_raw > 0x08)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_RED;
				else if (gh3->tb_raw > 0x05)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_RED | GUITAR_HERO_3_TOUCH_GREEN;
				else if (gh3->tb_raw > 0x02)
					gh3->touch_bar = GUITAR_HERO_3_TOUCH_GREEN;

				gh3->whammy_bar = (gh3->wb_raw - GUITAR_HERO_3_WHAMMY_BAR_MIN) / (float)(GUITAR_HERO_3_WHAMMY_BAR_MAX - GUITAR_HERO_3_WHAMMY_BAR_MIN);
				calc_joystick_state(&gh3->js, gh3->js.pos.x, gh3->js.pos.y);
				data->btns_h |= (data->exp.gh3.btns<<16);
			}
			break;

 			case EXP_WII_BOARD:
 			{
				struct wii_board_t *wb = &data->exp.wb;
				calc_balanceboard_state(wb);
 			}
 			break;

			default:
				break;
		}
	}
	data->btns_d = data->btns_h & ~data->btns_l;
	data->btns_u = ~data->btns_h & data->btns_l;
	*lstate = *data;
}

static void __save_state(struct wiimote_t* wm) {
	/* wiimote */
	wm->lstate.btns = wm->btns;
	wm->lstate.accel = wm->accel;

	/* ir */
	wm->lstate.ir = wm->ir;

	/* expansion */
	switch (wm->exp.type) {
		case EXP_NUNCHUK:
			wm->lstate.exp.nunchuk = wm->exp.nunchuk;
			break;
		case EXP_CLASSIC:
			wm->lstate.exp.classic = wm->exp.classic;
			break;
		case EXP_GUITAR_HERO_3:
			wm->lstate.exp.gh3 = wm->exp.gh3;
			break;
		case EXP_WII_BOARD:
			wm->lstate.exp.wb = wm->exp.wb;
			break;
		case EXP_MOTION_PLUS:
			wm->lstate.exp.mp = wm->exp.mp;
			break;
	}
}

#define ABS(x)		((s32)(x)>0?(s32)(x):-((s32)(x)))

#define STATE_CHECK(thresh, a, b) \
	if(((thresh) > WPAD_THRESH_IGNORE) && (ABS((a)-(b)) > (thresh))) \
		state_changed = 1;

#define STATE_CHECK_SIMPLE(thresh, a, b) \
	if(((thresh) > WPAD_THRESH_IGNORE) && ((a) != (b))) \
		state_changed = 1;

static u32 __wpad_read_expansion(struct wiimote_t *wm,WPADData *data, struct _wpad_thresh *thresh)
{
	int state_changed = 0;
	switch(data->exp.type) {
		case EXP_NUNCHUK:
			data->exp.nunchuk = wm->exp.nunchuk;
			STATE_CHECK_SIMPLE(thresh->btns, wm->exp.nunchuk.btns, wm->lstate.exp.nunchuk.btns);
			STATE_CHECK(thresh->acc, wm->exp.nunchuk.accel.x, wm->lstate.exp.nunchuk.accel.x);
			STATE_CHECK(thresh->acc, wm->exp.nunchuk.accel.y, wm->lstate.exp.nunchuk.accel.y);
			STATE_CHECK(thresh->acc, wm->exp.nunchuk.accel.z, wm->lstate.exp.nunchuk.accel.z);
			STATE_CHECK(thresh->js, wm->exp.nunchuk.js.pos.x, wm->lstate.exp.nunchuk.js.pos.x);
			STATE_CHECK(thresh->js, wm->exp.nunchuk.js.pos.y, wm->lstate.exp.nunchuk.js.pos.y);
			break;
		case EXP_CLASSIC:
			data->exp.classic = wm->exp.classic;
			STATE_CHECK_SIMPLE(thresh->btns, wm->exp.classic.btns, wm->lstate.exp.classic.btns);
			STATE_CHECK(thresh->js, wm->exp.classic.rs_raw, wm->lstate.exp.classic.rs_raw);
			STATE_CHECK(thresh->js, wm->exp.classic.ls_raw, wm->lstate.exp.classic.ls_raw);
			STATE_CHECK(thresh->js, wm->exp.classic.ljs.pos.x, wm->lstate.exp.classic.ljs.pos.x);
			STATE_CHECK(thresh->js, wm->exp.classic.ljs.pos.y, wm->lstate.exp.classic.ljs.pos.y);
			STATE_CHECK(thresh->js, wm->exp.classic.rjs.pos.x, wm->lstate.exp.classic.rjs.pos.x);
			STATE_CHECK(thresh->js, wm->exp.classic.rjs.pos.y, wm->lstate.exp.classic.rjs.pos.y);
			break;
		case EXP_GUITAR_HERO_3:
			data->exp.gh3 = wm->exp.gh3;
			STATE_CHECK_SIMPLE(thresh->btns, wm->exp.gh3.btns, wm->lstate.exp.gh3.btns);
			STATE_CHECK(thresh->js, wm->exp.gh3.wb_raw, wm->lstate.exp.gh3.wb_raw);
			STATE_CHECK(thresh->js, wm->exp.gh3.js.pos.x, wm->lstate.exp.gh3.js.pos.x);
			STATE_CHECK(thresh->js, wm->exp.gh3.js.pos.y, wm->lstate.exp.gh3.js.pos.y);
			break;
 		case EXP_WII_BOARD:
			data->exp.wb = wm->exp.wb;
			for (unsigned i = 0; i < WII_BOARD_NUM_SENSORS; ++i)
				STATE_CHECK(thresh->wb,
					    wm->exp.wb.raw_sensor[i],
					    wm->lstate.exp.wb.raw_sensor[i]);
 			break;
		case EXP_MOTION_PLUS:
			data->exp.mp = wm->exp.mp;
			STATE_CHECK(thresh->mp,wm->exp.mp.rx,wm->lstate.exp.mp.rx);
			STATE_CHECK(thresh->mp,wm->exp.mp.ry,wm->lstate.exp.mp.ry);
			STATE_CHECK(thresh->mp,wm->exp.mp.rz,wm->lstate.exp.mp.rz);
			break;
	}
	return state_changed;
}

static void __wpad_read_wiimote(struct wiimote_t *wm, WPADData *data, s32 *idle_time, struct _wpad_thresh *thresh)
{
	int i;
	int state_changed = 0;
	data->err = WPAD_ERR_TRANSFER;
	data->data_present = 0;
	data->battery_level = wm->battery_level;
	data->exp.type = wm->exp.type;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			switch(wm->event_buf[0]) {
				case WM_RPT_BTN:
				case WM_RPT_BTN_ACC:
				case WM_RPT_BTN_ACC_IR:
				case WM_RPT_BTN_EXP:
				case WM_RPT_BTN_ACC_EXP:
				case WM_RPT_BTN_IR_EXP:
				case WM_RPT_BTN_ACC_IR_EXP:
					data->btns_h = (wm->btns&0xffff);
					data->data_present |= WPAD_DATA_BUTTONS;
					STATE_CHECK_SIMPLE(thresh->btns, wm->btns, wm->lstate.btns);
			}
			switch(wm->event_buf[0]) {
				case WM_RPT_BTN_ACC:
				case WM_RPT_BTN_ACC_IR:
				case WM_RPT_BTN_ACC_EXP:
				case WM_RPT_BTN_ACC_IR_EXP:
					data->accel = wm->accel;
					data->data_present |= WPAD_DATA_ACCEL;
					STATE_CHECK(thresh->acc, wm->accel.x, wm->lstate.accel.x);
					STATE_CHECK(thresh->acc, wm->accel.y, wm->lstate.accel.y);
					STATE_CHECK(thresh->acc, wm->accel.z, wm->lstate.accel.z);
			}
			switch(wm->event_buf[0]) {
				//IR requires acceleration
				//case WM_RPT_BTN_IR_EXP:
				case WM_RPT_BTN_ACC_IR:
				case WM_RPT_BTN_ACC_IR_EXP:
					data->ir = wm->ir;
					data->data_present |= WPAD_DATA_IR;
					for(i=0; i<WPAD_MAX_IR_DOTS; i++) {
						STATE_CHECK_SIMPLE(thresh->ir, wm->ir.dot[i].visible, wm->lstate.ir.dot[i].visible);
						STATE_CHECK(thresh->ir, wm->ir.dot[i].rx, wm->lstate.ir.dot[i].rx);
						STATE_CHECK(thresh->ir, wm->ir.dot[i].ry, wm->lstate.ir.dot[i].ry);
					}
			}
			switch(wm->event_buf[0]) {
				case WM_RPT_BTN_EXP:
				case WM_RPT_BTN_ACC_EXP:
				case WM_RPT_BTN_IR_EXP:
				case WM_RPT_BTN_ACC_IR_EXP:
					state_changed |= __wpad_read_expansion(wm,data,thresh);
					data->data_present |= WPAD_DATA_EXPANSION;
			}
			data->err = WPAD_ERR_NONE;
			if(state_changed) {
				*idle_time = 0;
				__save_state(wm);
			}
		} else
			data->err = WPAD_ERR_NOT_READY;
	} else
		data->err = WPAD_ERR_NO_CONTROLLER;
}

static void __wpad_eventCB(struct wiimote_t *wm,s32 event)
{
	s32 chan;
	u32 maxbufs;
	WPADData *wpadd = NULL;
	struct _wpad_cb *wpdcb = NULL;

	switch(event) {
		case WIIUSE_EVENT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];

			if(wpdcb->queue_ext!=NULL) {
				maxbufs = wpdcb->queue_length;
				wpadd = &(wpdcb->queue_ext[wpdcb->queue_tail]);
			} else {
				maxbufs = EVENTQUEUE_LENGTH;
				wpadd = &(wpdcb->queue_int[wpdcb->queue_tail]);
			}
			if(wpdcb->queue_full == maxbufs) {
				wpdcb->queue_head++;
				wpdcb->queue_head %= maxbufs;
				wpdcb->dropped_events++;
			} else {
				wpdcb->queue_full++;
			}

			__wpad_read_wiimote(wm, wpadd, &wpdcb->idle_time, &wpdcb->thresh);

			wpdcb->queue_tail++;
			wpdcb->queue_tail %= maxbufs;

			break;
		case WIIUSE_STATUS:
			break;
		case WIIUSE_CONNECT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			wpdcb->wm = wm;
			wpdcb->queue_head = 0;
			wpdcb->queue_tail = 0;
			wpdcb->queue_full = 0;
			wpdcb->idle_time = 0;
			memset(&wpdcb->lstate,0,sizeof(WPADData));
			memset(&wpaddata[chan],0,sizeof(WPADData));
			memset(wpdcb->queue_int,0,(sizeof(WPADData)*EVENTQUEUE_LENGTH));
			wiiuse_set_ir_position(wm,(CONF_GetSensorBarPosition()^1));
			wiiuse_set_ir_sensitivity(wm,CONF_GetIRSensitivity());
			wiiuse_set_leds(wm,(WIIMOTE_LED_1<<(chan%WPAD_BALANCE_BOARD)),NULL);
			wiiuse_set_speaker(wm,wpdcb->speaker_enabled);
			__wpad_setfmt(chan);
			__wpads_active |= (0x01<<chan);
			break;
		case WIIUSE_DISCONNECT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			wpdcb->wm = wm;
			wpdcb->queue_head = 0;
			wpdcb->queue_tail = 0;
			wpdcb->queue_full = 0;
			wpdcb->queue_length = 0;
			wpdcb->queue_ext = NULL;
			wpdcb->idle_time = -1;
			memset(&wpdcb->lstate,0,sizeof(WPADData));
			memset(&wpaddata[chan],0,sizeof(WPADData));
			memset(wpdcb->queue_int,0,(sizeof(WPADData)*EVENTQUEUE_LENGTH));
			__wpads_active &= ~(0x01<<chan);
			break;
		case WIIUSE_ACK:
			break;
		default:
			WIIUSE_DEBUG("__wpad_eventCB(%02x)\n", event);
			break;
		}
}

void __wpad_disconnectCB(struct bd_addr *pad_addr, u8 reason)
{
	int i;
	u32 level;
	
	WIIUSE_DEBUG("__wpad_disconnectCB(%d)", reason);

	_CPU_ISR_Disable(level);
	if(__wpads_inited == WPAD_STATE_ENABLED) {
		for(i=0;i<WPAD_MAX_DEVICES;i++) {
			if(bd_addr_cmp(pad_addr,&__wpads_listen[i].bdaddr)) {
				if (__wpad_disconncb) {
					__wpad_disconncb(i, reason);
				} else {
					// For backwards compatibility
					switch (reason) {
						case WPAD_DISCON_BATTERY_DIED:
							if(__wpad_batcb) __wpad_batcb(i);
							break;
						case WPAD_DISCON_POWER_OFF:
							if(__wpad_powcb) __wpad_powcb(i);
							break;
					}
				}
				__wpads_used &= ~(0x01<<i);
				break;
			}
		}
	}
	_CPU_ISR_Restore(level);
}

static void __wpad_hostsyncbuttonCB(u32 held)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	if((__wpads_inited == WPAD_STATE_ENABLED) && __wpad_hostsynccb) {
		__wpad_hostsynccb(held);
	}
	_CPU_ISR_Restore(level);
}

static s32 GetActiveSlot(struct bd_addr *pad_addr)
{
	int i;
	int slot = CONF_PAD_MAX_ACTIVE;
	struct bd_addr bdaddr;

	for(i=0; i<CONF_PAD_MAX_ACTIVE; i++) {
		BD_ADDR_FROM_BYTES(&bdaddr,__wpad_devs.active[i].bdaddr);
		if(bd_addr_cmp(pad_addr,&bdaddr)) {
			slot = i;
			break;
		}
	}

	return slot;
}

s32 __wpad_read_remote_name_finished(s32 result,void *userdata)
{
	u32 level;
	int i;
	struct bd_addr bdaddr;
	int slot = WPAD_MAX_DEVICES;
	struct pad_name_info *info = (struct pad_name_info *)userdata;

	_CPU_ISR_Disable(level);

	if(result == ERR_OK && info != NULL) {
		if (!strncmp("Nintendo RVL-", (char *)info->name, 13)) {
			// Found Wii accessory, is it controller or something else?
			if (!strncmp("Nintendo RVL-CNT-", (char *)info->name, 17)) {
				slot = GetActiveSlot(&info->bdaddr);
				if (slot < CONF_PAD_MAX_ACTIVE) {
					WIIUSE_DEBUG("Wiimote already active in slot %d", slot);
				} else {
					// Not active, try to make active
					slot = WPAD_MAX_DEVICES;
					for(i=0; i<CONF_PAD_MAX_ACTIVE; i++) {
						if(!(__wpads_used & (1<<i))) {
							WIIUSE_DEBUG("Attempting to connect to Wiimote using slot %d", i);
							slot = i;
							break;
						}
					}
				}
			} else {
				// Check for balance board
				BD_ADDR_FROM_BYTES(&(bdaddr),__wpad_devs.balance_board.bdaddr);
				if (bd_addr_cmp(&info->bdaddr,&bdaddr)) {
					WIIUSE_DEBUG("Balance Board already registered");
					slot = WPAD_BALANCE_BOARD;
				}
				
				// Not active, try to make active
				if(slot != WPAD_BALANCE_BOARD) {
					if(!(__wpads_used & (1<<WPAD_BALANCE_BOARD))) {
						WIIUSE_DEBUG("Attempting to connect to Balance Board");
						slot = WPAD_BALANCE_BOARD;
					}
				}
			}

			if(slot < WPAD_MAX_DEVICES) {
				result = wiiuse_connect(&__wpads_listen[slot],&info->bdaddr,info->name,__wpad_register_new);
				__wpads_used |= (0x01<<slot);
				result = ERR_OK;
			} else {
				WIIUSE_WARNING("WPAD All Slots Used\n");
				result = ERR_CONN;
			}
		} else {
			result = ERR_ARG;
		}
	}

	if (info) free(info);
	_CPU_ISR_Restore(level);

	return result;
}

static s32 GetRegisteredSlot(struct bd_addr *pad_addr)
{
	int i;
	int slot = CONF_PAD_MAX_REGISTERED;
	struct bd_addr bdaddr;

	for(i=0; i<__wpad_devs.num_registered; i++) {
		BD_ADDR_FROM_BYTES(&bdaddr,__wpad_devs.registered[i].bdaddr);
		if(bd_addr_cmp(pad_addr,&bdaddr)) {
			slot = i;
			break;
		}
	}

	return slot;
}

static s32 GetGuestSlot(struct bd_addr *pad_addr)
{
	int i;
	int slot = CONF_PAD_MAX_ACTIVE;
	struct bd_addr bdaddr;

	for(i=0; i<__wpad_guests.num_guests; i++) {
		BD_ADDR_FROM_BYTES(&bdaddr,__wpad_guests.guests[i].bdaddr);
		if(bd_addr_cmp(pad_addr,&bdaddr)) {
			slot = i;
			break;
		}
	}

	return slot;
}

static s8 __wpad_connreqCB(void *arg,struct bd_addr *pad_addr,u8 *cod,u8 link_type)
{
	int i, j, level;
	int slot = WPAD_MAX_DEVICES;
	int confslot = CONF_PAD_MAX_ACTIVE;
	struct bd_addr bdaddr;
	u8 *name = NULL;

	WIIUSE_DEBUG("__wpad_connreqCB");
	_CPU_ISR_Disable(level);

	// Only accept connection requests (i.e. "press any button") if not doing guest pairing
	if(BTE_GetPairMode() == PAIR_MODE_NORMAL) {
		if(!bd_addr_cmp(pad_addr,BD_ADDR_ANY)) {
			confslot = GetActiveSlot(pad_addr);
			if (confslot < CONF_PAD_MAX_ACTIVE) {
				BD_ADDR_FROM_BYTES(&bdaddr,__wpad_devs.active[confslot].bdaddr);
				name = (u8 *)__wpad_devs.active[confslot].name;
				WIIUSE_DEBUG("Active pad '%s' found in slot %d", name, confslot);
				if (!(__wpads_used & (1<<confslot)) || bd_addr_cmp(pad_addr,&bdaddr))
					slot = confslot;
				else
					WIIUSE_DEBUG("Slot %d taken! Finding new slot", confslot);
			}
	
			if(slot >= WPAD_MAX_DEVICES) {
				for(i=0; i<CONF_PAD_MAX_REGISTERED; i++) {
					BD_ADDR_FROM_BYTES(&bdaddr,__wpad_devs.registered[i].bdaddr);
					if(bd_addr_cmp(pad_addr,&bdaddr)) {
						name = (u8 *)__wpad_devs.registered[i].name;
						WIIUSE_DEBUG("Registered pad '%s' found in slot %d", name, i);
		
						// Not active, try to make active
						if(!strncmp("Nintendo RVL-CNT-", __wpad_devs.registered[i].name, 17)) {
							for(j=0; j<WPAD_BALANCE_BOARD; j++) {
								if(!(__wpads_used & (1<<j))) {
									WIIUSE_DEBUG("Attempting to accept connection from pad using slot %d", j);
									slot = j;
									break;
								}
							}

							if(slot >= WPAD_BALANCE_BOARD) {
								slot = WPAD_MAX_DEVICES;
							}
						} else {
							if(!(__wpads_used & (1<<WPAD_BALANCE_BOARD))) {
								WIIUSE_DEBUG("Attempting to accept connection from Balance Board");
								slot = WPAD_BALANCE_BOARD;
								break;
							}
						}
						break;
					}
				}
			}

			if(slot < WPAD_MAX_DEVICES) {
				__wpads_used |= (0x01<<slot);
				WIIUSE_DEBUG("Assigning slot %d",slot);

				wiiuse_accept(&__wpads_listen[slot],pad_addr,name,__wpad_assign_slot);
				_CPU_ISR_Restore(level);
				return ERR_OK;
			}
		}

		_CPU_ISR_Restore(level);
		return ERR_VAL;
	}
	_CPU_ISR_Restore(level);
	return ERR_CONN;
}

static s8 __wpad_linkkeyreqCB(void *arg,struct bd_addr *pad_addr)
{
	bool found = false;
	int i;
	
	WIIUSE_DEBUG("__wpad_linkkeyreqCB");
	
	for(i=0; i<__wpads_bonded; i++) {
		if(bd_addr_cmp(pad_addr,&__wpad_keys[i].bdaddr)) {
			found = true;
			BTE_LinkKeyRequestReply(pad_addr,__wpad_keys[i].key);
			break;
		}
	}

	if(!found) {
		for(i=0; i<CONF_PAD_MAX_ACTIVE; i++) {
			if(bd_addr_cmp(pad_addr,&__wpad_guest_keys[i].bdaddr)) {
				found = true;
				BTE_LinkKeyRequestReply(pad_addr,__wpad_guest_keys[i].key);
				break;
			}
		}
	}

	if(!found)
		BTE_LinkKeyRequestNegativeReply(pad_addr);

	return ERR_OK;
}

static s8 __wpad_linkkeynotCB(void *arg,struct bd_addr *pad_addr,u8 *key)
{
	int i;

	WIIUSE_DEBUG("__wpad_linkkeynotCB");
		
	if(BTE_GetPairMode() == PAIR_MODE_NORMAL) {
		// Only write link key to controller if not doing guest pairing
		BTE_WriteStoredLinkKey(pad_addr,key);

		for(i=0; i<CONF_PAD_MAX_REGISTERED; i++) {
			// Old device, new key
			if(bd_addr_cmp(pad_addr,&__wpad_keys[i].bdaddr)) {
				memcpy(__wpad_keys[i].key,key,sizeof(__wpad_keys[i].key));
				break;
			}
			
			// New device
			if(bd_addr_cmp(BD_ADDR_ANY,&__wpad_keys[i].bdaddr)) {
				bd_addr_set(&__wpad_keys[i].bdaddr,pad_addr);
				memcpy(__wpad_keys[i].key,key,sizeof(__wpad_keys[i].key));
				__wpads_bonded++;
				break;
			}
		}
	} else {
		for(i=0; i<CONF_PAD_ACTIVE_AND_OTHER; i++) {
			// Old device, new key
			if(bd_addr_cmp(pad_addr,&__wpad_guest_keys[i].bdaddr)) {
				memcpy(__wpad_guest_keys[i].key,key,sizeof(__wpad_guest_keys[i].key));
				break;
			}
			
			// New device
			if(bd_addr_cmp(BD_ADDR_ANY,&__wpad_guest_keys[i].bdaddr)) {
				bd_addr_set(&__wpad_guest_keys[i].bdaddr,pad_addr);
				memcpy(__wpad_guest_keys[i].key,key,sizeof(__wpad_guest_keys[i].key));
				break;
			}
		}
	}

	return ERR_OK;
}

s32 __wpad_inquiry_finished(s32 result,void *userdata)
{
	int i;
	struct pad_name_info *padinfo;
	struct bte_inquiry_res *inq_res = (struct bte_inquiry_res *)userdata;

	if(result == ERR_OK && inq_res != NULL) {
		// Filter out weird null values
		for(i=0;i<inq_res->count;i++) {
			struct inquiry_info_ex *info = &inq_res->info[i];

			if(!bd_addr_cmp(&info->bdaddr,BD_ADDR_ANY)) {
				// Read name of first valid device
				padinfo = (struct pad_name_info *)malloc(sizeof(struct pad_name_info));
				if (padinfo == NULL)
					return ERR_MEM;
				bd_addr_set(&padinfo->bdaddr, &info->bdaddr);
				BTE_ReadRemoteName(padinfo, __wpad_read_remote_name_finished);
				break;
			}
		}
	}

	return ERR_OK;
}

s32 WPAD_StartPairing(void)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	WIIUSE_INFO("WPAD Begin Pairing");
	BTE_Inquiry(BD_MAX_INQUIRY_DEVS, TRUE, __wpad_inquiry_finished);
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

s32 WPAD_WipeSavedControllers(void)
{
	u32 level;
	int i;

	_CPU_ISR_Disable(level);
	WIIUSE_DEBUG("WPAD Wipe Registered Pads");

	for(i=0;i<WPAD_MAX_DEVICES;i++) {
		__wpad_disconnect(&__wpdcb[i]);
		memset(__wpads_listen[i].name, 0, sizeof(__wpads_listen[i].name));
	}

	__wpads_active = 0;
	__wpads_used = 0;
	__wpads_bonded = 0;
	memset(&__wpad_devs,0,sizeof(__wpad_devs));
	memset(&__wpad_guests,0,sizeof(__wpad_guests));
	memset(__wpad_keys,0,sizeof(__wpad_keys));
	memset(__wpad_guest_keys,0,sizeof(__wpad_guest_keys));
	BTE_ClearStoredLinkKeys();
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

void __wpad_def_hostsynccb(u32 held)
{
	if(held)
		WPAD_WipeSavedControllers();
	else
		WPAD_StartPairing();
}

s32 WPAD_Search(void)
{
	u32 level;
	int i;

	_CPU_ISR_Disable(level);
	WIIUSE_INFO("WPAD Guest Device Search");

	for(i=0;i<WPAD_MAX_DEVICES;i++) {
		__wpad_disconnect(&__wpdcb[i]);
		memset(__wpads_listen[i].name, 0, sizeof(__wpads_listen[i].name));
	}
	__wpads_active = 0;
	__wpads_used = 0;
	memset(__wpad_devs.active,0,sizeof(__wpad_devs.active));
	memset(&__wpad_guests,0,sizeof(__wpad_guests));
	memset(__wpad_guest_keys,0,sizeof(__wpad_guest_keys));

	BTE_PeriodicInquiry(BD_MAX_INQUIRY_DEVS, TRUE, __wpad_inquiry_finished);
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

s32 WPAD_StopSearch(void)
{
	u32 level;

	_CPU_ISR_Disable(level);
	WIIUSE_DEBUG("WPAD Stopping Guest Device Search");
	BTE_ExitPeriodicInquiry();
	_CPU_ISR_Restore(level);
	return ERR_OK;
}

s32 WPAD_Init(void)
{
	u32 level;
	struct timespec tb;
	int i;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		__wpads_bonded = 0;
		__wpads_active = 0;

		WIIUSE_DEBUG("WPAD_Init");

		memset(__wpdcb,0,sizeof(__wpdcb));
		memset(&__wpad_devs,0,sizeof(conf_pads));
		memset(__wpad_keys,0,sizeof(__wpad_keys));
		memset(__wpad_guest_keys,0,sizeof(__wpad_guest_keys));

		for(i=0;i<WPAD_MAX_DEVICES;i++) {
			__wpdcb[i].thresh.btns = WPAD_THRESH_DEFAULT_BUTTONS;
			__wpdcb[i].thresh.ir = WPAD_THRESH_DEFAULT_IR;
			__wpdcb[i].thresh.acc = WPAD_THRESH_DEFAULT_ACCEL;
			__wpdcb[i].thresh.js = WPAD_THRESH_DEFAULT_JOYSTICK;
			__wpdcb[i].thresh.wb = WPAD_THRESH_DEFAULT_BALANCEBOARD;
			__wpdcb[i].thresh.mp = WPAD_THRESH_DEFAULT_MOTION_PLUS;

			if (SYS_CreateAlarm(&__wpdcb[i].sound_alarm) < 0)
			{
				WPAD_Shutdown();
				_CPU_ISR_Restore(level);
				return WPAD_ERR_UNKNOWN;
			}
		}

		if(CONF_GetPadDevices(&__wpad_devs) < 0) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_BADCONF;
		}

		if(__wpad_devs.num_registered > CONF_PAD_MAX_REGISTERED) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_BADCONF;
		}

		if(CONF_GetPadGuestDevices(&__wpad_guests) < 0) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_BADCONF;
		}

		if(__wpad_guests.num_guests > CONF_PAD_MAX_ACTIVE) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_BADCONF;
		}

		// Clear guest devices after loading so guests are unregistered in case of unexpected shutdown
		if(CONF_SetPadGuestDevices(NULL) < 0) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_BADCONF;
		}

		__wpads = wiiuse_init(WPAD_MAX_DEVICES,__wpad_eventCB);
		if(__wpads==NULL) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_UNKNOWN;
		}

		__wiiuse_sensorbar_enable(1);

		__wpads_inited = WPAD_STATE_ENABLING;

		BTE_Init();
		BTE_SetDisconnectCallback(__wpad_disconnectCB);
		BTE_SetHostSyncButtonCallback(__wpad_hostsyncbuttonCB);
		BTE_SetConnectionRequestCallback(__wpad_connreqCB);
		BTE_SetLinkKeyRequestCallback(__wpad_linkkeyreqCB);
		BTE_SetLinkKeyNotificationCallback(__wpad_linkkeynotCB);
		BTE_InitCore(__initcore_finished);

		if (SYS_CreateAlarm(&__wpad_timer) < 0)
		{
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_UNKNOWN;
		}

		SYS_RegisterResetFunc(&__wpad_resetinfo);

		tb.tv_sec = 1;
		tb.tv_nsec = 0;
		SYS_SetPeriodicAlarm(__wpad_timer,&tb,&tb,__wpad_timeouthandler,NULL);

		if (LWP_SemInit(&__wpad_confsave_sem,0,1)<0) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_UNKNOWN;
		}
		
		if(LWP_CreateThread(&__wpad_confsave_thread,__wpad_confsave_thread_func,NULL,NULL,0,80)<0) {
			WPAD_Shutdown();
			_CPU_ISR_Restore(level);
			return WPAD_ERR_UNKNOWN;
		}
	}
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_ReadEvent(s32 chan, WPADData *data)
{
	u32 level;
	u32 maxbufs,smoothed = 0;
	struct accel_t *accel_calib = NULL;
	struct _wpad_cb *wpdcb = NULL;
	WPADData *lstate = NULL,*wpadd = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan] && WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			wpdcb = &__wpdcb[chan];
			if(wpdcb->queue_ext!=NULL) {
				maxbufs = wpdcb->queue_length;
				wpadd = wpdcb->queue_ext;
			} else {
				maxbufs = EVENTQUEUE_LENGTH;
				wpadd = wpdcb->queue_int;
			}
			if(wpdcb->queue_full == 0) {
				_CPU_ISR_Restore(level);
				return WPAD_ERR_QUEUE_EMPTY;
			}
			if(data)
				*data = wpadd[wpdcb->queue_head];
			wpdcb->queue_head++;
			wpdcb->queue_head %= maxbufs;
			wpdcb->queue_full--;
			lstate = &wpdcb->lstate;
			accel_calib = &__wpads[chan]->accel_calib;
			smoothed = WIIMOTE_IS_FLAG_SET(__wpads[chan], WIIUSE_SMOOTHING);
		} else {
			_CPU_ISR_Restore(level);
			return WPAD_ERR_NOT_READY;
		}
	} else {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NO_CONTROLLER;
	}

	_CPU_ISR_Restore(level);
	if(data)
		__wpad_calc_data(data,lstate,accel_calib,smoothed);
	return 0;
}

s32 WPAD_DroppedEvents(s32 chan)
{
	u32 level;
	s32 ret;
	int i;
	int dropped = 0;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_DroppedEvents(i)) < WPAD_ERR_NONE)
				return ret;
			else
				dropped += ret;
		return dropped;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) {
		dropped = __wpdcb[chan].dropped_events;
		__wpdcb[chan].dropped_events = 0;
	}
	_CPU_ISR_Restore(level);
	return dropped;
}

s32 WPAD_Flush(s32 chan)
{
	s32 ret;
	int i;
	int count = 0;
	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_Flush(i)) < WPAD_ERR_NONE)
				return ret;
			else
				count += ret;
		return count;
	}

	while((ret = WPAD_ReadEvent(chan, NULL)) >= 0)
		count++;
	if(ret == WPAD_ERR_QUEUE_EMPTY) return count;
	return ret;
}

s32 WPAD_ReadPending(s32 chan, WPADDataCallback datacb)
{
	u32 btns_p = 0;
	u32 btns_h = 0;
	u32 btns_l = 0;
	u32 btns_ch = 0;
	u32 btns_ev = 0;
	u32 btns_nh = 0;
	u32 i;
	s32 count = 0;
	s32 ret;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_ReadPending(i, datacb)) >= WPAD_ERR_NONE)
				count += ret;
		return count;
	}

	btns_p = btns_nh = btns_l = wpaddata[chan].btns_h;
	while(1) {
		ret = WPAD_ReadEvent(chan,&wpaddata[chan]);
		if(ret < WPAD_ERR_NONE) break;
		if(datacb)
			datacb(chan, &wpaddata[chan]);

		// we ignore everything except _h, since we have our
		// own "fake" _l and everything gets recalculated at
		// the end of the function
		btns_h = wpaddata[chan].btns_h;

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

		// buttons that changed that haven't changed before
		btns_ch = (btns_h ^ btns_p) & ~btns_ev;
		btns_p = btns_h;
		// propagate changes in btns_ch to btns_nd
		btns_nh = (btns_nh & ~btns_ch) | (btns_h & btns_ch);
		// store these new changes to btns_ev
		btns_ev |= btns_ch;

		count++;
	}
	wpaddata[chan].btns_h = btns_nh;
	wpaddata[chan].btns_l = btns_l;
	wpaddata[chan].btns_d = btns_nh & ~btns_l;
	wpaddata[chan].btns_u = ~btns_nh & btns_l;
	if(ret == WPAD_ERR_QUEUE_EMPTY) return count;
	return ret;
}

s32 WPAD_SetDataFormat(s32 chan, s32 fmt)
{
	u32 level;
	s32 ret;
	int i;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_SetDataFormat(i, fmt)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) {
		switch(fmt) {
			case WPAD_FMT_BTNS:
			case WPAD_FMT_BTNS_ACC:
			case WPAD_FMT_BTNS_ACC_IR:
				__wpdcb[chan].data_fmt = fmt;
				__wpad_setfmt(chan);
				break;
			default:
				_CPU_ISR_Restore(level);
				return WPAD_ERR_BADVALUE;
		}
	}
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetMotionPlus(s32 chan, u8 enable)
{
	u32 level;
	s32 ret;
	int i;
	
	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_SetMotionPlus(i, enable)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) {
		wiiuse_set_motion_plus(__wpads[chan], enable);
	}
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetVRes(s32 chan,u32 xres,u32 yres)
{
	u32 level;
	s32 ret;
	int i;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_SetVRes(i, xres, yres)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL)
		wiiuse_set_ir_vres(__wpads[chan],xres,yres);

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_GetStatus(void)
{
	s32 ret;
	u32 level;

	_CPU_ISR_Disable(level);
	ret = __wpads_inited;
	_CPU_ISR_Restore(level);

	return ret;
}

s32 WPAD_Probe(s32 chan,u32 *type)
{
	s32 ret;
	u32 level,dev;
	wiimote *wm = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	wm = __wpads[chan];
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			dev = WPAD_EXP_NONE;
			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP)) {
				switch(wm->exp.type) {
					case WPAD_EXP_NUNCHUK:
					case WPAD_EXP_CLASSIC:
					case WPAD_EXP_GUITARHERO3:
 					case WPAD_EXP_WIIBOARD:
						dev = wm->exp.type;
						break;
				}
			}
			if(type!=NULL) *type = dev;
			ret = WPAD_ERR_NONE;
		} else
			ret = WPAD_ERR_NOT_READY;
	} else
		ret = WPAD_ERR_NO_CONTROLLER;
	_CPU_ISR_Restore(level);

	return ret;
}

s32 WPAD_SetEventBufs(s32 chan, WPADData *bufs, u32 cnt)
{
	u32 level;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	wpdcb = &__wpdcb[chan];
	wpdcb->queue_head = 0;
	wpdcb->queue_tail = 0;
	wpdcb->queue_full = 0;
	wpdcb->queue_length = cnt;
	wpdcb->queue_ext = bufs;
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

[[deprecated]]
void WPAD_SetPowerButtonCallback(WPADShutdownCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(cb)
		__wpad_powcb = cb;
	else
		__wpad_powcb = __wpad_def_powcb;
	_CPU_ISR_Restore(level);
}

[[deprecated]]
void WPAD_SetBatteryDeadCallback(WPADShutdownCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_batcb = cb;
	_CPU_ISR_Restore(level);
}

void WPAD_SetDisconnectCallback(WPADDisconnectCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_disconncb = cb;
	_CPU_ISR_Restore(level);
}

void WPAD_SetHostSyncButtonCallback(WPADHostSyncBtnCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(cb)
		__wpad_hostsynccb = cb;
	else
		__wpad_hostsynccb = __wpad_def_hostsynccb;
	_CPU_ISR_Restore(level);
}

void WPAD_SetStatusCallback(WPADStatusCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_statuscb = cb;
	_CPU_ISR_Restore(level);
}

s32 WPAD_Disconnect(s32 chan)
{
	s32 err = WPAD_ERR_NONE;
	u32 level, cnt = 0;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_ENABLED) {
		wpdcb = &__wpdcb[chan];
		__wpad_disconnect(wpdcb);

		_CPU_ISR_Restore(level);

		while(__wpads_used&(0x01<<chan)) {
			usleep(50);
			if(++cnt > 3000) break;
		}
	} else {
		_CPU_ISR_Restore(level);
		err = WPAD_ERR_NOT_READY;
	}

	return err;
}

s32 WPAD_Shutdown(void)
{
	s32 err = WPAD_ERR_NONE;
	s32 i;
	u32 level;
	struct _wpad_cb *wpdcb = NULL;
	
	WIIUSE_DEBUG("WPAD_Shutdown");
	
	_CPU_ISR_Disable(level);
	BTE_Close();

	__wpad_confsave_thread_stop();
	if(__wpads_inited!=WPAD_STATE_DISABLED) {
		CONF_SetPadGuestDevices(&__wpad_guests);
		CONF_SaveChanges();
	}

	__wpads_inited = WPAD_STATE_DISABLED;
	SYS_RemoveAlarm(__wpad_timer);
	for(i=0;i<WPAD_MAX_DEVICES;i++) {
		wpdcb = &__wpdcb[i];
		SYS_RemoveAlarm(wpdcb->sound_alarm);
		bte_free(__wpads_listen[i].sock);
		__wpads_listen[i].sock = NULL;
	}

	__wpads_active = 0;
	__wpads_used = 0;

	__wiiuse_sensorbar_enable(0);
	_CPU_ISR_Restore(level);

	BTE_Shutdown();

	return err;
}

void WPAD_SetIdleTimeout(u32 seconds)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_idletimeout = seconds;
	_CPU_ISR_Restore(level);
}

s32 WPAD_ScanPads(void)
{
	return WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
}

s32 WPAD_Rumble(s32 chan, int status)
{
	int i;
	s32 ret;
	u32 level;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_Rumble(i,status)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL)
		wiiuse_rumble(__wpads[chan],status);

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetIdleThresholds(s32 chan, s32 btns, s32 ir, s32 accel, s32 js, s32 wb, s32 mp)
{
	int i;
	s32 ret;
	u32 level;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_SetIdleThresholds(i,btns,ir,accel,js,wb,mp)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	__wpdcb[chan].thresh.btns = (btns<0) ? -1 : 0;
	__wpdcb[chan].thresh.ir = ir;
	__wpdcb[chan].thresh.acc = accel;
	__wpdcb[chan].thresh.js = js;
	__wpdcb[chan].thresh.wb = wb;
	__wpdcb[chan].thresh.mp = mp;
	

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_ControlSpeaker(s32 chan,s32 enable)
{
	int i;
	s32 ret;
	u32 level;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_DEVICES; i++)
			if((ret = WPAD_ControlSpeaker(i,enable)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) {
		__wpdcb[chan].speaker_enabled = enable;
		wiiuse_set_speaker(__wpads[chan],enable);
	}

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_IsSpeakerEnabled(s32 chan)
{
	s32 ret;
	u32 level;
	wiimote *wm = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	wm = __wpads[chan];
	ret = WPAD_ERR_NOT_READY;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)
			&& WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) ret = WPAD_ERR_NONE;
	}

	_CPU_ISR_Restore(level);
	return ret;
}

s32 WPAD_SendStreamData(s32 chan,void *buf,u32 len)
{
	u32 level;
	struct timespec tb;
	wiimote *wm = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_DEVICES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	wm = __wpads[chan];
	if(wm!=NULL  && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)
			&& WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) {
			__wpdcb[chan].sound_data = buf;
			__wpdcb[chan].sound_len = len;
			__wpdcb[chan].sound_off = 0;

			tb.tv_sec = 0;
			tb.tv_nsec = 6666667;
			SYS_SetPeriodicAlarm(__wpdcb[chan].sound_alarm,&tb,&tb,__wpad_sounddata_alarmhandler, &__wpdcb[chan]);
		}
	}

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

void WPAD_EncodeData(WPADEncStatus *info,u32 flag,const s16 *pcmSamples,s32 numSamples,u8 *encData)
{
	int n;
	short *samples = (short*)pcmSamples;
	WENCStatus *status = (WENCStatus*)info;

	if(!(flag&WPAD_ENC_CONT)) status->step = 0;

	n = (numSamples+1)/2;
	for(;n>0;n--) {
		int nibble;
		nibble = (wencdata(status,samples[0]))<<4;
		nibble |= (wencdata(status,samples[1]));
		*encData++ = nibble;
		samples += 2;
	}
}

WPADData *WPAD_Data(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return NULL;
	return &wpaddata[chan];
}

u8 WPAD_BatteryLevel(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return 0;
	return wpaddata[chan].battery_level;
}

u32 WPAD_ButtonsUp(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return 0;
	return wpaddata[chan].btns_u;
}

u32 WPAD_ButtonsDown(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return 0;
	return wpaddata[chan].btns_d;
}

u32 WPAD_ButtonsHeld(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return 0;
	return wpaddata[chan].btns_h;
}

void WPAD_IR(int chan, struct ir_t *ir)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES || ir==NULL ) return;
	*ir = wpaddata[chan].ir;
}

void WPAD_Orientation(int chan, struct orient_t *orient)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES || orient==NULL ) return;
	*orient = wpaddata[chan].orient;
}

void WPAD_GForce(int chan, struct gforce_t *gforce)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES || gforce==NULL ) return;
	*gforce = wpaddata[chan].gforce;
}

void WPAD_Accel(int chan, struct vec3w_t *accel)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES || accel==NULL ) return;
	*accel = wpaddata[chan].accel;
}

void WPAD_Expansion(int chan, struct expansion_t *exp)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES || exp==NULL ) return;
	*exp = wpaddata[chan].exp;
}

void __wpad_status_finished(struct wiimote_t *wm,ubyte *data,uword len)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if (wm && __wpad_statuscb) __wpad_statuscb(wm->unid);
	_CPU_ISR_Restore(level);
}

void WPAD_PadStatus(int chan)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return;
	if(__wpads[chan] && WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_CONNECTED)) wiiuse_status(__wpads[chan], __wpad_status_finished);

	_CPU_ISR_Restore(level);
}

bool WPAD_IsBatteryCritical(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_DEVICES) return false;
	return WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_BATTERY_CRITICAL);
}
