#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "processor.h"
#include "bte.h"
#include "conf.h"
#include "wiiuse_internal.h"
#include "wiiuse/wpad.h"

#define MAX_WIIMOTES			4

static u32 __wpads_inited = 0;
static s32 __wpads_ponded = 0;
static u32 __wpads_connected = 0;
static s32 __wpads_registered = 0;
static wiimote **__wpads = {NULL};
static conf_pad_device __wpad_devs[MAX_WIIMOTES];
static struct linkkey_info __wpad_keys[MAX_WIIMOTES];

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

	if(result==ERR_OK) {
		BTE_InitSub(__wpad_init_finished);
	}
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

static void __wpad_eventCB(struct wiimote_t *wm,s32 event)
{
	//printf("__wpad_eventCB(%p,%02x)\n",wm,event);
	switch(event) {
		case WIIUSE_EVENT:
			break;
		case WIIUSE_STATUS:
			break;
		case WIIUSE_CONNECT:
			printf("wiimote connected\n");
			wiiuse_set_ir_position(wm,CONF_GetSensorBarPosition());
			wiiuse_set_ir_sensitivity(wm,CONF_GetIRSensitivity());
			wiiuse_set_leds(wm,(WIIMOTE_LED_1<<(wm->unid-1)),NULL);
			__wpads_connected |= (0x01<<(wm->unid-1));
			break;
		case WIIUSE_DISCONNECT:
			printf("wiimote disconnected\n");
			__wpads_connected &= ~(0x01<<(wm->unid-1));
			break;
		default:
			break;
	}
}

static void __wpad_read_expansion(struct wiimote_t *wm,WPADData *data)
{
	data->exp.type = wm->exp.type;

	switch(wm->exp.type) {
		case EXP_NUNCHUK:
		{
			Nunchaku *nc = &data->exp.nunchuk;
			struct nunchuk_t *wmnc = &wm->exp.nunchuk;

			nc->btns_d = wmnc->btns;
			nc->btns_h = wmnc->btns_held;
			nc->btns_r = wmnc->btns_released;
			
			nc->accel.x = wmnc->accel.x;
			nc->accel.y = wmnc->accel.y;
			nc->accel.z = wmnc->accel.z;

			nc->orient.roll = wmnc->orient.roll;
			nc->orient.pitch = wmnc->orient.pitch;
			nc->orient.yaw = wmnc->orient.yaw;

			nc->gforce.x = wmnc->gforce.x;
			nc->gforce.y = wmnc->gforce.y;
			nc->gforce.z = wmnc->gforce.z;
		}
		break;

		case EXP_CLASSIC:
			break;
		case EXP_GUITAR_HERO_3:
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
		if(__wpads_registered<=0) return;

		__wpads = wiiuse_init(MAX_WIIMOTES,__wpad_eventCB);
		if(__wpads==NULL) {
			_CPU_ISR_Restore(level);
			return;
		}

		BTE_Init();
		BTE_InitCore(__initcore_finished);

		__wpads_inited = WPAD_STATE_ENABLING;
	}
	_CPU_ISR_Restore(level);
}

void WPAD_Read(WPADData *data)
{
	s32 i,j,k;
	u32 level;

	if(data==NULL) return;

	memset(data,0,sizeof(WPADData)*MAX_WIIMOTES);

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return;
	}
	
	for(i=0;i<MAX_WIIMOTES;i++) {
		if(__wpads[i] && WIIMOTE_IS_SET(__wpads[i],(WIIMOTE_STATE_CONNECTED|WIIMOTE_STATE_HANDSHAKE_COMPLETE))) {
			data[i].btns_d = __wpads[i]->btns;
			data[i].btns_h = __wpads[i]->btns_held;
			data[i].btns_r = __wpads[i]->btns_released;

			if(WIIMOTE_IS_SET(__wpads[i],WIIMOTE_STATE_ACC)) {
				data->accel.x = __wpads[i]->accel.x;
				data->accel.y = __wpads[i]->accel.y;
				data->accel.z = __wpads[i]->accel.z;

				data->orient.roll = __wpads[i]->orient.roll;
				data->orient.pitch = __wpads[i]->orient.pitch;
				data->orient.yaw = __wpads[i]->orient.yaw;
			}
			if(WIIMOTE_IS_SET(__wpads[i],WIIMOTE_STATE_IR)) {
				for(j=0,k=0;j<WPAD_MAX_IR_DOTS;j++) {
					if(__wpads[i]->ir.dot[j].visible) {
						data->ir.dot[k].x = __wpads[i]->ir.dot[j].x;
						data->ir.dot[k].y = __wpads[i]->ir.dot[j].y;
						data->ir.dot[k].order = __wpads[i]->ir.dot[j].order;
						data->ir.dot[k].size = __wpads[i]->ir.dot[j].size;

						k++;
					}
				}
				data->ir.num_dots = k;

				data->ir.x = __wpads[i]->ir.x;
				data->ir.y = __wpads[i]->ir.y;
				data->ir.z = __wpads[i]->ir.z;
				data->ir.dist = __wpads[i]->ir.distance;

			}
			if(WIIMOTE_IS_SET(__wpads[i],WIIMOTE_STATE_EXP)) {
				__wpad_read_expansion(__wpads[i],data);
			}
		}
	}
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
