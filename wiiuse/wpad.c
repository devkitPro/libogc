#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asm.h"
#include "processor.h"
#include "bte.h"
#include "conf.h"
#include "ir.h"
#include "dynamics.h"
#include "guitar_hero_3.h"
#include "wiiuse_internal.h"
#include "wiiuse/wpad.h"
#include "lwp_threads.h"
#include "ogcsys.h"

#define MAX_RINGBUFS			2

struct _wpad_cb {
	wiimote *wm;
	u32 data_fmt;
	s32 buf_idx;
	u32 max_bufs;
	s32 idle_time;
	WPADData lstate;
	WPADData *ringbuf_ext;
	WPADData ringbuf_int[MAX_RINGBUFS];
	wpadsamplingcallback samplingCB;
};

static syswd_t __wpad_timer;
static vu32 __wpads_inited = 0;
static vs32 __wpads_ponded = 0;
static u32 __wpad_sleeptime = 5;
static vu32 __wpads_active = 0;
static vs32 __wpads_registered = 0;
static wiimote **__wpads = NULL;
static WPADData wpaddata[MAX_WIIMOTES];
static struct _wpad_cb __wpdcb[MAX_WIIMOTES];
static conf_pad_device __wpad_devs[MAX_WIIMOTES];
static struct linkkey_info __wpad_keys[MAX_WIIMOTES];

static s32 __wpad_onreset(s32 final);
static s32 __wpad_disconnect(struct _wpad_cb *wpdcb);
static void __wpad_eventCB(struct wiimote_t *wm,s32 event);

static sys_resetinfo __wpad_resetinfo = {
	{},
	__wpad_onreset,
	127
};

static s32 __wpad_onreset(s32 final)
{
	//printf("__wpad_onreset(%d)\n",final);
	if(final==FALSE) {
		WPAD_Shutdown();
	}
	return 1;
}

static void __wpad_timeouthandler(syswd_t alarm)
{
	s32 i;
	struct wiimote_t *wm = NULL;
	struct _wpad_cb *wpdcb = NULL;

	if(!__wpads_active) return;

	__lwp_thread_dispatchdisable();
	for(i=0;i<MAX_WIIMOTES;i++) {
		wpdcb = &__wpdcb[i];
		wm = wpdcb->wm;
		if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
			wpdcb->idle_time++;
			if(wpdcb->idle_time>=__wpad_sleeptime) {
				wiiuse_disconnect(wm);
			}
		}
	}
	__lwp_thread_dispatchunnest();
}

static s32 __wpad_init_finished(s32 result,void *usrdata)
{
	u32 i;
	struct bd_addr bdaddr;

	//printf("__wpad_init_finished(%d)\n",result);
	
	if(result==ERR_OK) {
		for(i=0;__wpads[i] && i<MAX_WIIMOTES && i<__wpads_registered;i++) {
			BD_ADDR(&(bdaddr),__wpad_devs[i].bdaddr[5],__wpad_devs[i].bdaddr[4],__wpad_devs[i].bdaddr[3],__wpad_devs[i].bdaddr[2],__wpad_devs[i].bdaddr[1],__wpad_devs[i].bdaddr[0]);
			wiiuse_register(__wpads[i],&(bdaddr));
		}
		__wpads_inited = WPAD_STATE_ENABLED;
	}
	return ERR_OK;
}

static s32 __wpad_patch_finished(s32 result,void *usrdata)
{
	//printf("__wpad_patch_finished(%d)\n",result);
	BTE_InitSub(__wpad_init_finished);
	return ERR_OK;
}

static s32 __readlinkkey_finished(s32 result,void *usrdata)
{
	//printf("__readlinkkey_finished(%d)\n",result);

	__wpads_ponded = result;
	BTE_ApplyPatch(__wpad_patch_finished);

	return ERR_OK;
}

static s32 __initcore_finished(s32 result,void *usrdata)
{
	//printf("__initcore_finished(%d)\n",result);

	if(result==ERR_OK) {
		BTE_ReadStoredLinkKey(__wpad_keys,MAX_WIIMOTES,__readlinkkey_finished);
	}
	return ERR_OK;
}

static s32 __wpad_disconnect(struct _wpad_cb *wpdcb)
{
	struct wiimote_t *wm;

	if(wpdcb==NULL) return 0;

	wm = wpdcb->wm;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		wiiuse_disconnect(wm);
	}

	return 0;
}

static void __wpad_calc_data(WPADData *data,WPADData *lstate,struct accel_t *accel_calib,u32 useacc,u32 useir,u32 useexp,u32 smoothed)
{
	if(data->err!=WPAD_ERR_NONE) return;

	data->orient = lstate->orient;
	if(useacc) {
		calculate_orientation(accel_calib, &data->accel, &data->orient, smoothed);
		calculate_gforce(accel_calib, &data->accel, &data->gforce);
	}
	if(useir) {
		interpret_ir_data(&data->ir,&data->orient,useacc);
	}
	if(useexp) {
		switch(data->exp.type) {
			case EXP_NUNCHUK:
			{
				struct nunchuk_t *nc = &data->exp.nunchuk;

				nc->orient = lstate->exp.nunchuk.orient;
				calc_joystick_state(&nc->js,nc->js.pos.x,nc->js.pos.y);
				calculate_orientation(&nc->accel_calib,&nc->accel,&nc->orient,smoothed);
				calculate_gforce(&nc->accel_calib,&nc->accel,&nc->gforce);
			}
			break;

			case EXP_CLASSIC:
			{
				struct classic_ctrl_t *cc = &data->exp.classic;

				cc->r_shoulder = ((f32)cc->rs_raw/0x1F);
				cc->l_shoulder = ((f32)cc->ls_raw/0x1F);
				calc_joystick_state(&cc->ljs, cc->ljs.pos.x, cc->ljs.pos.y);
				calc_joystick_state(&cc->rjs, cc->rjs.pos.x, cc->rjs.pos.y);
			}
			break;

			case EXP_GUITAR_HERO_3:
			{
				struct guitar_hero_3_t *gh3 = &data->exp.gh3;

				gh3->whammy_bar = (gh3->wb_raw - GUITAR_HERO_3_WHAMMY_BAR_MIN) / (float)(GUITAR_HERO_3_WHAMMY_BAR_MAX - GUITAR_HERO_3_WHAMMY_BAR_MIN);
				calc_joystick_state(&gh3->js, gh3->js.pos.x, gh3->js.pos.y);
			}
			break;

			default:
				break;
		}
	}
	*lstate = *data;
}

static void __wpad_read_expansion(struct wiimote_t *wm,WPADData *data)
{
	data->exp.type = wm->exp.type;
	switch(data->exp.type) {
		case EXP_NUNCHUK:
			data->exp.nunchuk = wm->exp.nunchuk;
			break;
		case EXP_CLASSIC:
			data->exp.classic = wm->exp.classic;
			break;
		case EXP_GUITAR_HERO_3:
			data->exp.gh3 = wm->exp.gh3;
			break;
		default:
			break;
	}

}

static void __wpad_read_wiimote(struct wiimote_t *wm,WPADData *data)
{
	data->err = WPAD_ERR_TRANSFER;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			data->btns_d = wm->btns;
			data->btns_h = wm->btns_held;
			data->btns_r = wm->btns_released;
			data->btns_l = wm->btns_last;

			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_ACC)) {
				data->accel = wm->accel;
			}

			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) {
				data->ir = wm->ir;
			}
			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP)) {
				__wpad_read_expansion(wm,data);
			}
			data->err = WPAD_ERR_NONE;
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
			wpdcb->idle_time = 0;
			if(wpdcb->ringbuf_ext!=NULL) {
				maxbufs = wpdcb->max_bufs;
				wpadd = &(wpdcb->ringbuf_ext[wpdcb->buf_idx]);
			} else {
				maxbufs = MAX_RINGBUFS;
				wpadd = &(wpdcb->ringbuf_int[wpdcb->buf_idx]);
			}

			__wpad_read_wiimote(wm,wpadd);

			wpdcb->buf_idx++;
			wpdcb->buf_idx %= maxbufs;

			if(wpdcb->samplingCB!=NULL) wpdcb->samplingCB(chan);
			break;
		case WIIUSE_STATUS:
			break;
		case WIIUSE_CONNECT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			wpdcb->wm = wm;
			wpdcb->buf_idx = 0;
			wpdcb->idle_time = 0;
			memset(&wpdcb->lstate,0,sizeof(WPADData));
			memset(&wpaddata[chan],0,sizeof(WPADData));
			memset(wpdcb->ringbuf_int,0,(sizeof(WPADData)*MAX_RINGBUFS));
			wiiuse_set_ir_position(wm,(CONF_GetSensorBarPosition()^1));
			wiiuse_set_ir_sensitivity(wm,CONF_GetIRSensitivity());
			wiiuse_set_leds(wm,(WIIMOTE_LED_1<<chan),NULL);
			__wpads_active |= (0x01<<chan);
			break;
		case WIIUSE_DISCONNECT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			wpdcb->wm = NULL;
			wpdcb->buf_idx = 0;
			wpdcb->max_bufs = 0;
			wpdcb->ringbuf_ext = NULL;
			wpdcb->idle_time = -1;
			memset(&wpdcb->lstate,0,sizeof(WPADData));
			memset(&wpaddata[chan],0,sizeof(WPADData));
			memset(wpdcb->ringbuf_int,0,(sizeof(WPADData)*MAX_RINGBUFS));
			__wpads_active &= ~(0x01<<chan);
			break;
		default:
			break;
	}
}


void WPAD_Init()
{
	u32 level;
	struct timespec tb;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		__wpads_ponded = 0;
		__wpads_active = 0;
		__wpads_registered = 0;

		memset(__wpdcb,0,sizeof(struct _wpad_cb)*MAX_WIIMOTES);
		memset(__wpad_devs,0,sizeof(conf_pad_device)*MAX_WIIMOTES);
		memset(__wpad_keys,0,sizeof(struct linkkey_info)*MAX_WIIMOTES);

		__wpads_registered = CONF_GetPadDevices(__wpad_devs,MAX_WIIMOTES);
		if(__wpads_registered<=0) return;

		__wpads = wiiuse_init(MAX_WIIMOTES,__wpad_eventCB);
		if(__wpads==NULL) {
			_CPU_ISR_Restore(level);
			return;
		}

		BTE_Init();
		BTE_InitCore(__initcore_finished);

		SYS_CreateAlarm(&__wpad_timer);
		SYS_RegisterResetFunc(&__wpad_resetinfo);
	
		tb.tv_sec = 1;
		tb.tv_nsec = 0;
		SYS_SetPeriodicAlarm(__wpad_timer,&tb,&tb,__wpad_timeouthandler);

		__wpads_inited = WPAD_STATE_ENABLING;
	}
	_CPU_ISR_Restore(level);
}

void WPAD_Read(s32 chan,WPADData *data)
{
	u32 idx;
	u32 level;
	u32 maxbufs,smoothed = 0;
	u32 useir = 0,useacc = 0,useexp = 0;
	struct accel_t *accel_calib = NULL;
	struct _wpad_cb *wpdcb = NULL;
	WPADData *lstate = NULL,*wpadd = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return;

	memset(data,0,sizeof(WPADData));

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		data->err = WPAD_ERR_NOT_READY;
		_CPU_ISR_Restore(level);
		return;
	}

	data->err = WPAD_ERR_TRANSFER;
	if(__wpads[chan] && WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			wpdcb = &__wpdcb[chan];
			if(wpdcb->ringbuf_ext!=NULL) {
				maxbufs = wpdcb->max_bufs;
				wpadd = wpdcb->ringbuf_ext;
			} else {
				maxbufs = MAX_RINGBUFS;
				wpadd = wpdcb->ringbuf_int;
			}

			idx = ((wpdcb->buf_idx-1)+maxbufs)%maxbufs;
			*data = wpadd[idx];
			lstate = &__wpdcb->lstate;
			accel_calib = &__wpads[chan]->accel_calib;
			smoothed = WIIMOTE_IS_FLAG_SET(__wpads[chan], WIIUSE_SMOOTHING);
			useacc = WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_ACC);
			useir = WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_IR);
			useexp = WIIMOTE_IS_SET(__wpads[chan],(WIIMOTE_STATE_EXP|WIIMOTE_STATE_EXP_HANDSHAKE_COMPLETE));
		} else
			data->err = WPAD_ERR_NOT_READY;
	} else
		data->err = WPAD_ERR_NO_CONTROLLER;
	_CPU_ISR_Restore(level);

	__wpad_calc_data(data,lstate,accel_calib,useacc,useir,useexp,smoothed);
}

void WPAD_SetDataFormat(s32 chan,s32 fmt)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return;
	}

	if(__wpads[chan]!=NULL) {
		switch(fmt) {
			case WPAD_FMT_CORE:
				wiiuse_motion_sensing(__wpads[chan],0);
				wiiuse_set_ir(__wpads[chan],0);
				break;
			case WPAD_FMT_CORE_ACC:
				wiiuse_motion_sensing(__wpads[chan],1);
				wiiuse_set_ir(__wpads[chan],0);
				break;
			case WPAD_FMT_CORE_ACC_IR:
				wiiuse_motion_sensing(__wpads[chan],1);
				wiiuse_set_ir(__wpads[chan],1);
				break;
			default:
				break;
		}
	}
	_CPU_ISR_Restore(level);
}

void WPAD_SetVRes(s32 chan,u32 xres,u32 yres)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return;
	}

	if(__wpads[chan]!=NULL) 
		wiiuse_set_ir_vres(__wpads[chan],xres,yres);

	_CPU_ISR_Restore(level);
}

s32 WPAD_GetStatus()
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

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return WPAD_ERR_NO_CONTROLLER;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	wm = __wpads[chan];
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			dev = WPAD_DEV_CORE;
			if(WIIMOTE_IS_SET(wm,(WIIMOTE_STATE_EXP|WIIMOTE_STATE_EXP_HANDSHAKE_COMPLETE))) {
				switch(wm->exp.type) {
					case WPAD_EXP_NUNCHAKU:
						dev = WPAD_DEV_NUNCHAKU;
						break;
					case WPAD_EXP_CLASSIC:
						dev = WPAD_DEV_CLASSIC;
						break;
					case WPAD_EXP_GUITAR_HERO3:
						dev = WPAD_DEV_GUITARHERO3;
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

wpadsamplingcallback WPAD_SetSamplingCallback(s32 chan,wpadsamplingcallback cb)
{
	u32 level;
	struct _wpad_cb *wpdcb = NULL;
	wpadsamplingcallback ret = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return NULL;

	_CPU_ISR_Disable(level);
	wpdcb = &__wpdcb[chan];
	ret = wpdcb->samplingCB;
	wpdcb->samplingCB = cb;
	_CPU_ISR_Restore(level);

	return ret;
}

void WPAD_SetSamplingBufs(s32 chan,void *bufs,u32 cnt)
{
	u32 level;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return;

	_CPU_ISR_Disable(level);
	wpdcb = &__wpdcb[chan];
	wpdcb->buf_idx = 0;
	wpdcb->max_bufs = 0;
	wpdcb->ringbuf_ext = bufs;
	_CPU_ISR_Restore(level);
}

u32 WPAD_GetLatestBufIndex(s32 chan)
{
	u32 idx;
	u32 level;
	u32 maxbufs;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return 0;

	_CPU_ISR_Disable(level);
	wpdcb = &__wpdcb[chan];
	if(wpdcb->ringbuf_ext!=NULL)
		maxbufs = wpdcb->max_bufs;
	else
		maxbufs = MAX_RINGBUFS;

	idx = ((wpdcb->buf_idx-1)+maxbufs)%maxbufs;
	_CPU_ISR_Restore(level);

	return idx;
}

void WPAD_Disconnect(s32 chan)
{
	u32 level;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return;
	}
	
	wpdcb = &__wpdcb[chan];
	__wpad_disconnect(wpdcb);
	_CPU_ISR_Restore(level);

	while(__wpads_active&(0x01<<chan));
}

void WPAD_Shutdown()
{
	s32 i;
	u32 level;
	struct _wpad_cb *wpdcb = NULL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return;
	}

	SYS_RemoveAlarm(__wpad_timer);
	for(i=0;i<MAX_WIIMOTES;i++) {
		wpdcb = &__wpdcb[i];
		__wpad_disconnect(wpdcb);
	}

	__wpads_inited = WPAD_STATE_DISABLED;
	_CPU_ISR_Restore(level);
	
	while(__wpads_active);

	BTE_Shutdown();
}

void WPAD_SetSleepTime(u32 sleep)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_sleeptime = sleep;
	_CPU_ISR_Restore(level);
}

u32 WPAD_ScanPads() 
{
	u32 btns_l = 0;
	u32 i, connected = 0;

	for(i=0;i<MAX_WIIMOTES;i++) {
		btns_l = wpaddata[i].btns_d;
		WPAD_Read(i,&wpaddata[i]);
		wpaddata[i].btns_l = btns_l;

		switch(wpaddata[i].exp.type) {
			case WPAD_EXP_NONE:
				break;
			case WPAD_EXP_NUNCHAKU:
				wpaddata[i].btns_d |= (wpaddata[i].exp.nunchuk.btns<<16);
				break;
			case WPAD_EXP_CLASSIC:
				wpaddata[i].btns_d |= (wpaddata[i].exp.classic.btns<<16);
				break;
			case WPAD_EXP_GUITAR_HERO3:
				wpaddata[i].btns_d |= (wpaddata[i].exp.gh3.btns<<16);
				break;
		}
	}
	return connected;		
}

u32 WPAD_ButtonsUp(int pad)
{
	if(pad<0 || pad>MAX_WIIMOTES) return 0;
	return ((~wpaddata[pad].btns_d)&wpaddata[pad].btns_l);
}

u32 WPAD_ButtonsDown(int pad)
{
	if(pad<0 || pad>MAX_WIIMOTES) return 0;
	return (wpaddata[pad].btns_d&~wpaddata[pad].btns_l);
}

u32 WPAD_ButtonsHeld(int pad) 
{
	if(pad<0 || pad>MAX_WIIMOTES) return 0;
	return wpaddata[pad].btns_d;
}

