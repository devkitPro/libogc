#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "processor.h"
#include "bte.h"
#include "conf.h"
#include "wiiuse_internal.h"
#include "wiiuse/wpad.h"

#define MAX_RINGBUFS			2

static vu32 __wpads_inited = 0;
static vs32 __wpads_ponded = 0;
static vu32 __wpads_connected = 0;
static vs32 __wpads_registered = 0;
static wiimote **__wpads = NULL;
static WPADData wpaddata[MAX_WIIMOTES];
static s32 __wpad_samplingbufs_idx[MAX_WIIMOTES];
static conf_pad_device __wpad_devs[MAX_WIIMOTES];
static u32 __wpad_max_autosamplingbufs[MAX_WIIMOTES];
static struct linkkey_info __wpad_keys[MAX_WIIMOTES];
static WPADData __wpad_samplingbufs[MAX_WIIMOTES][MAX_RINGBUFS];
static WPADData *__wpad_autosamplingbufs[MAX_WIIMOTES] = {NULL,NULL,NULL,NULL};
static wpadsamplingcallback __wpad_samplingCB[MAX_WIIMOTES] = {NULL,NULL,NULL,NULL};

static void __wpad_eventCB(struct wiimote_t *wm,s32 event);

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

	//if(result==ERR_OK) {
		BTE_InitSub(__wpad_init_finished);
	//}
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

static void __wpad_read_expansion(struct wiimote_t *wm,WPADData *data)
{
	data->exp.type = wm->exp.type;

	#define CP_JOY(dest, src)	do { \
									(dest).min.x = (src).min.x; \
									(dest).min.y = (src).min.y; \
									(dest).max.x = (src).max.x; \
									(dest).max.y = (src).max.y; \
									(dest).center.x = (src).center.x; \
									(dest).center.y = (src).center.y; \
									(dest).ang = (src).ang; \
									(dest).mag = (src).mag; \
								} while (0)

	switch(wm->exp.type) {
		case EXP_NUNCHUK:
		{
			Nunchaku *nc = &data->exp.nunchuk;
			struct nunchuk_t *wmnc = &wm->exp.nunchuk;

			nc->btns_d = wmnc->btns;
			nc->btns_h = wmnc->btns_held;
			nc->btns_r = wmnc->btns_released;
			nc->btns_l = wmnc->btns_last;
			
			nc->accel.x = wmnc->accel.x;
			nc->accel.y = wmnc->accel.y;
			nc->accel.z = wmnc->accel.z;

			nc->orient.roll = wmnc->orient.roll;
			nc->orient.pitch = wmnc->orient.pitch;
			nc->orient.yaw = wmnc->orient.yaw;

			nc->gforce.x = wmnc->gforce.x;
			nc->gforce.y = wmnc->gforce.y;
			nc->gforce.z = wmnc->gforce.z;

			CP_JOY(nc->js, wmnc->js);
		}
		break;

		case EXP_CLASSIC:
		{
			Classic *cc = &data->exp.classic;
			struct classic_ctrl_t *wmcc = &wm->exp.classic;

			cc->btns_d = wmcc->btns;
			cc->btns_h = wmcc->btns_held;
			cc->btns_r = wmcc->btns_released;
			cc->btns_l = wmcc->btns_last;

			cc->r_shoulder = wmcc->r_shoulder;
			cc->l_shoulder = wmcc->l_shoulder;

			CP_JOY(cc->ljs, wmcc->ljs);
			CP_JOY(cc->rjs, wmcc->rjs);
		}
		break;

		case EXP_GUITAR_HERO_3:
			break;
		default:
			break;
	}

}

static void __wpad_read_wiimote(struct wiimote_t *wm,WPADData *data)
{
	s32 j,k;

	data->err = WPAD_ERR_TRANSFER;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			data->btns_d = wm->btns;
			data->btns_h = wm->btns_held;
			data->btns_r = wm->btns_released;
			data->btns_l = wm->btns_last;

			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_ACC)) {
				data->accel.x = wm->accel.x;
				data->accel.y = wm->accel.y;
				data->accel.z = wm->accel.z;

				data->orient.roll = wm->orient.roll;
				data->orient.pitch = wm->orient.pitch;
				data->orient.yaw = wm->orient.yaw;
			}
			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) {
				for(j=0,k=0;j<WPAD_MAX_IR_DOTS;j++) {
					if(wm->ir.dot[j].visible) {
						data->ir.dot[k].x = wm->ir.dot[j].x;
						data->ir.dot[k].y = wm->ir.dot[j].y;
						data->ir.dot[k].order = wm->ir.dot[j].order;
						data->ir.dot[k].size = wm->ir.dot[j].size;
						data->ir.dot[k].visible = 1;

						k++;
					}
				}
				data->ir.num_dots = k;

				data->ir.x = wm->ir.x;
				data->ir.y = wm->ir.y;
				data->ir.z = wm->ir.z;
				data->ir.dist = wm->ir.distance;

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
	u32 maxbufs;
	WPADData *wpadd = NULL;

	//printf("__wpad_eventCB(%p,%02x,%d)\n",wm,event,__wpad_samplingbufs_idx[(wm->unid-1)]);
	switch(event) {
		case WIIUSE_EVENT:
			if(__wpad_autosamplingbufs[(wm->unid-1)]!=NULL) {
				maxbufs = __wpad_max_autosamplingbufs[(wm->unid-1)];
				wpadd = &(__wpad_autosamplingbufs[(wm->unid-1)][__wpad_samplingbufs_idx[(wm->unid-1)]]);
			} else {
				maxbufs = MAX_RINGBUFS;
				wpadd = &(__wpad_samplingbufs[(wm->unid-1)][__wpad_samplingbufs_idx[(wm->unid-1)]]);
			}

			__wpad_read_wiimote(wm,wpadd);

			__wpad_samplingbufs_idx[(wm->unid-1)]++;
			__wpad_samplingbufs_idx[(wm->unid-1)] %= maxbufs;

			if(__wpad_samplingCB[(wm->unid-1)]!=NULL) __wpad_samplingCB[(wm->unid-1)]((wm->unid-1));
			break;
		case WIIUSE_STATUS:
			break;
		case WIIUSE_CONNECT:
			//printf("wiimote connected\n");
			__wpad_samplingbufs_idx[(wm->unid-1)] = 0;
			memset(&wpaddata[(wm->unid-1)],0,sizeof(WPADData));
			memset(__wpad_samplingbufs[(wm->unid-1)],0,MAX_RINGBUFS);
			wiiuse_set_ir_position(wm,(CONF_GetSensorBarPosition()^1));
			wiiuse_set_ir_sensitivity(wm,CONF_GetIRSensitivity());
			wiiuse_set_leds(wm,(WIIMOTE_LED_1<<(wm->unid-1)),NULL);
			__wpads_connected |= (0x01<<(wm->unid-1));
			break;
		case WIIUSE_DISCONNECT:
			printf("wiimote disconnected\n");
			__wpad_samplingCB[(wm->unid-1)] = NULL;
			__wpad_samplingbufs_idx[(wm->unid-1)] = 0;
			__wpad_autosamplingbufs[(wm->unid-1)] = NULL;
			__wpad_max_autosamplingbufs[(wm->unid-1)] = 0;
			memset(__wpad_samplingbufs[(wm->unid-1)],0,MAX_RINGBUFS);
			memset(&wpaddata[(wm->unid-1)],0,sizeof(WPADData));
			__wpads_connected &= ~(0x01<<(wm->unid-1));
			break;
		default:
			break;
	}
}


void WPAD_Init()
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		__wpads_ponded = 0;
		__wpads_connected = 0;
		__wpads_registered = 0;

		memset(__wpad_devs,0,sizeof(conf_pad_device)*MAX_WIIMOTES);
		memset(__wpad_keys,0,sizeof(struct linkkey_info)*MAX_WIIMOTES);

		__wpads_registered = CONF_GetPadDevices(__wpad_devs,MAX_WIIMOTES);
		//printf("%d pads registered\n", __wpads_registered);
		if(__wpads_registered<=0) return;

		__wpads = wiiuse_init(MAX_WIIMOTES,__wpad_eventCB);
		if(__wpads==NULL) {
			_CPU_ISR_Restore(level);
			return;
		}

		//printf("BTE init\n");
		BTE_Init();
		BTE_InitCore(__initcore_finished);

		__wpads_inited = WPAD_STATE_ENABLING;
	}
	_CPU_ISR_Restore(level);
}

void WPAD_Read(s32 chan,WPADData *data)
{
	u32 idx;
	u32 level;
	u32 maxbufs;
	WPADData *wpadd = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return;

	memset(data,0,sizeof(WPADData));

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		data->err = WPAD_ERR_NOT_READY;
		_CPU_ISR_Restore(level);
		return;
	}

	if(__wpad_autosamplingbufs[chan]!=NULL) {
		maxbufs = __wpad_max_autosamplingbufs[chan];
		wpadd = __wpad_autosamplingbufs[chan];
	} else {
		maxbufs = MAX_RINGBUFS;
		wpadd = __wpad_samplingbufs[chan];
	}

	idx = ((__wpad_samplingbufs_idx[chan]-1)+maxbufs)%maxbufs;
	*data = wpadd[idx];
	_CPU_ISR_Restore(level);
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

wpadsamplingcallback WPAD_SetSamplingCallback(s32 chan,wpadsamplingcallback cb)
{
	u32 level;
	wpadsamplingcallback ret = NULL;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return NULL;

	_CPU_ISR_Disable(level);
	ret = __wpad_samplingCB[chan];
	__wpad_samplingCB[chan] = cb;
	_CPU_ISR_Restore(level);

	return ret;
}

void WPAD_SetSamplingBufs(s32 chan,void *bufs,u32 cnt)
{
	u32 level;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return;

	_CPU_ISR_Disable(level);
	__wpad_samplingbufs_idx[chan] = 0;
	__wpad_autosamplingbufs[chan] = bufs;
	__wpad_max_autosamplingbufs[chan] = cnt;
	_CPU_ISR_Restore(level);
}

u32 WPAD_GetLatestBufIndex(s32 chan)
{
	u32 idx;
	u32 level;
	u32 maxbufs;

	if(chan<WPAD_CHAN_0 || chan>WPAD_CHAN_3) return 0;

	_CPU_ISR_Disable(level);
	if(__wpad_autosamplingbufs[chan]!=NULL)
		maxbufs = __wpad_max_autosamplingbufs[chan];
	else
		maxbufs = MAX_RINGBUFS;

	idx = ((__wpad_samplingbufs_idx[chan]-1)+maxbufs)%maxbufs;
	_CPU_ISR_Restore(level);

	return idx;
}

void WPAD_Shutdown()
{
	s32 i;

	printf("WPAD_Shutdown()\n");
	for(i=0;i<MAX_WIIMOTES;i++) {
		if(__wpads[i] && WIIMOTE_IS_SET(__wpads[i],WIIMOTE_STATE_CONNECTED)) {
			wiiuse_disconnect(__wpads[i]);
		}
	}
	while(__wpads_connected);

	BTE_Reset();
}

u32 WPAD_ScanPads() 
{
	u16 btns_l;
	u32 i, connected = 0;

	for(i=0;i<MAX_WIIMOTES;i++) {
		btns_l = wpaddata[i].btns_d;
		WPAD_Read(i,&wpaddata[i]);
		wpaddata[i].btns_l = btns_l;

	}
	return connected;		
}

u16 WPAD_ButtonsUp(int pad)
{
	if(pad<0 || pad>MAX_WIIMOTES) return 0;
	return ((~wpaddata[pad].btns_d)&wpaddata[pad].btns_l);
}

u16 WPAD_ButtonsDown(int pad)
{
	if(pad<0 || pad>MAX_WIIMOTES) return 0;
	return (wpaddata[pad].btns_d&~wpaddata[pad].btns_l);
}

u16 WPAD_ButtonsHeld(int pad) 
{
	if(pad<0 || pad>MAX_WIIMOTES) return 0;
	return wpaddata[pad].btns_d;
}

