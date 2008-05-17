#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"
#include "wiiuse_internal.h"
#include "nunchuk.h"
#include "classic.h"
#include "guitar_hero_3.h"
#include "io.h"
#include "lwp_wkspace.h"

void wiiuse_handshake(struct wiimote_t *wm,ubyte *data,uword len)
{
	ubyte *buf = NULL;
	struct accel_t *accel = &wm->accel_calib;

	//printf("wiiuse_handshake(%d,%p,%d)\n",wm->handshake_state,data,len);

	switch(wm->handshake_state) {
		case 0:
			wm->handshake_state++;

			wiiuse_set_leds(wm,WIIMOTE_LED_NONE,NULL);

			buf = __lwp_wkspace_allocate(sizeof(ubyte)*8);
			wiiuse_read_data(wm,buf,WM_MEM_OFFSET_CALIBRATION,7,wiiuse_handshake);
			break;
		case 1:
			wm->handshake_state++;

			accel->cal_zero.x = data[0];
			accel->cal_zero.y = data[1];
			accel->cal_zero.z = data[2];

			accel->cal_g.x = (data[4] - accel->cal_zero.x);
			accel->cal_g.y = (data[5] - accel->cal_zero.y);
			accel->cal_g.z = (data[6] - accel->cal_zero.z);
			__lwp_wkspace_free(data);
			
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);

			wm->event = WIIUSE_CONNECT;
			wiiuse_status(wm,NULL);
			break;
		default:
			break;

	}
}

void wiiuse_handshake_expansion_enabled(struct wiimote_t *wm,ubyte *data,uword len)
{
	ubyte *buf;

	//printf("wiiuse_handshake_expansion_enabled(%p,%d)\n",data,len);

	buf = __lwp_wkspace_allocate(sizeof(ubyte)*EXP_HANDSHAKE_LEN);
	wiiuse_read_data(wm,buf,WM_EXP_MEM_CALIBR,EXP_HANDSHAKE_LEN,wiiuse_handshake_expansion);

	WIIMOTE_ENABLE_STATE(wm, (WIIMOTE_STATE_EXP|WIIMOTE_STATE_EXP_HANDSHAKE));
}

void wiiuse_handshake_expansion(struct wiimote_t *wm,ubyte *data,uword len)
{
	int id;
	ubyte buf;

	//printf("wiiuse_handshake_expansion(%p,%d)\n",data,len);

	if(data==NULL) {
		buf = 0x00;
		wiiuse_write_data(wm,WM_EXP_MEM_ENABLE,&buf,1,wiiuse_handshake_expansion_enabled);
		return;
	}

	id = BIG_ENDIAN_LONG(*(int*)(&data[220]));
	switch(id) {
		case EXP_ID_CODE_NUNCHUK:
			if(!nunchuk_handshake(wm,&wm->exp.nunchuk,data,len)) return;
			wm->event = WIIUSE_NUNCHUK_INSERTED;
			break;
		case EXP_ID_CODE_CLASSIC_CONTROLLER:
			if(!classic_ctrl_handshake(wm,&wm->exp.classic,data,len)) return;
			wm->event = WIIUSE_CLASSIC_CTRL_INSERTED;
			break;
		case EXP_ID_CODE_GUITAR:
			if(!guitar_hero_3_handshake(wm,&wm->exp.gh3,data,len)) return;
			wm->event = WIIUSE_GUITAR_HERO_3_CTRL_INSERTED;
			break;
		default:
			printf("unknown expansion type connected.\n");
			break;
	}
	__lwp_wkspace_free(data);
	
	wiiuse_status(wm,NULL);
}

void wiiuse_disable_expansion(struct wiimote_t *wm)
{
	if(!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP)) return;

	/* tell the associated module the expansion was removed */
	switch(wm->exp.type) {
		case EXP_NUNCHUK:
			nunchuk_disconnected(&wm->exp.nunchuk);
			wm->event = WIIUSE_NUNCHUK_REMOVED;
			break;
		case EXP_CLASSIC:
			classic_ctrl_disconnected(&wm->exp.classic);
			wm->event = WIIUSE_CLASSIC_CTRL_REMOVED;
			break;
		case EXP_GUITAR_HERO_3:
			guitar_hero_3_disconnected(&wm->exp.gh3);
			wm->event = WIIUSE_GUITAR_HERO_3_CTRL_REMOVED;
			break;
		default:
			break;
	}

	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
	wm->exp.type = EXP_NONE;
}
