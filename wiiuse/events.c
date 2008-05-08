#include <stdio.h>

#ifndef WIN32
	#include <sys/time.h>
	#include <unistd.h>
	#include <errno.h>
#else
	#include <winsock2.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "dynamics.h"
#include "definitions.h"
#include "wiiuse_internal.h"
#include "events.h"
#include "nunchuk.h"
#include "classic.h"
#include "guitar_hero_3.h"
#include "ir.h"
#include "io.h"


extern void hexdump(void *d, int len);


/**
 *	@brief Save important state data.
 *	@param wm	A pointer to a wiimote_t structure.
 */
static void save_state(struct wiimote_t* wm) {
	/* wiimote */
	wm->lstate.btns = wm->btns;
	wm->lstate.accel = wm->accel;

	/* ir */
	if (WIIUSE_USING_IR(wm)) {
		wm->lstate.ir_ax = wm->ir.ax;
		wm->lstate.ir_ay = wm->ir.ay;
		wm->lstate.ir_distance = wm->ir.distance;
	}

	/* expansion */
	switch (wm->exp.type) {
		case EXP_NUNCHUK:
			wm->lstate.exp_ljs_ang = wm->exp.nunchuk.js.ang;
			wm->lstate.exp_ljs_mag = wm->exp.nunchuk.js.mag;
			wm->lstate.exp_btns = wm->exp.nunchuk.btns;
			wm->lstate.exp_accel = wm->exp.nunchuk.accel;
			break;

		case EXP_CLASSIC:
			wm->lstate.exp_ljs_ang = wm->exp.classic.ljs.ang;
			wm->lstate.exp_ljs_mag = wm->exp.classic.ljs.mag;
			wm->lstate.exp_rjs_ang = wm->exp.classic.rjs.ang;
			wm->lstate.exp_rjs_mag = wm->exp.classic.rjs.mag;
			wm->lstate.exp_r_shoulder = wm->exp.classic.r_shoulder;
			wm->lstate.exp_l_shoulder = wm->exp.classic.l_shoulder;
			wm->lstate.exp_btns = wm->exp.classic.btns;
			break;

		case EXP_GUITAR_HERO_3:
			wm->lstate.exp_ljs_ang = wm->exp.gh3.js.ang;
			wm->lstate.exp_ljs_mag = wm->exp.gh3.js.mag;
			wm->lstate.exp_r_shoulder = wm->exp.gh3.whammy_bar;
			wm->lstate.exp_btns = wm->exp.gh3.btns;
			break;

		case EXP_NONE:
			break;
	}
}


/**
 *	@brief Determine if the current state differs significantly from the previous.
 *	@param wm	A pointer to a wiimote_t structure.
 *	@return	1 if a significant change occured, 0 if not.
 */
static int state_changed(struct wiimote_t* wm) {
	#define STATE_CHANGED(a, b)		if (a != b)				return 1

	#define CROSS_THRESH(last, now, thresh)										\
				do {															\
					if (WIIMOTE_IS_FLAG_SET(wm, WIIUSE_ORIENT_THRESH)) {		\
						if ((diff_f(last.roll, now.roll) >= thresh) ||			\
							(diff_f(last.pitch, now.pitch) >= thresh) ||		\
							(diff_f(last.yaw, now.yaw) >= thresh))				\
						{														\
							last = now;											\
							return 1;											\
						}														\
					} else {													\
						if (last.roll != now.roll)		return 1;				\
						if (last.pitch != now.pitch)	return 1;				\
						if (last.yaw != now.yaw)		return 1;				\
					}															\
				} while (0)

	#define CROSS_THRESH_XYZ(last, now, thresh)									\
				do {															\
					if (WIIMOTE_IS_FLAG_SET(wm, WIIUSE_ORIENT_THRESH)) {		\
						if ((diff_f(last.x, now.x) >= thresh) ||				\
							(diff_f(last.y, now.y) >= thresh) ||				\
							(diff_f(last.z, now.z) >= thresh))					\
						{														\
							last = now;											\
							return 1;											\
						}														\
					} else {													\
						if (last.x != now.x)		return 1;					\
						if (last.y != now.y)		return 1;					\
						if (last.z != now.z)		return 1;					\
					}															\
				} while (0)

	/* ir */
	if (WIIUSE_USING_IR(wm)) {
		STATE_CHANGED(wm->lstate.ir_ax, wm->ir.ax);
		STATE_CHANGED(wm->lstate.ir_ay, wm->ir.ay);
		STATE_CHANGED(wm->lstate.ir_distance, wm->ir.distance);
	}

	/* accelerometer */
	if (WIIUSE_USING_ACC(wm)) {
		/* raw accelerometer */
		CROSS_THRESH_XYZ(wm->lstate.accel, wm->accel, wm->accel_threshold);

		/* orientation */
 		CROSS_THRESH(wm->lstate.orient, wm->orient, wm->orient_threshold);
	}

	/* expansion */
	switch (wm->exp.type) {
		case EXP_NUNCHUK:
		{
			STATE_CHANGED(wm->lstate.exp_ljs_ang, wm->exp.nunchuk.js.ang);
			STATE_CHANGED(wm->lstate.exp_ljs_mag, wm->exp.nunchuk.js.mag);
			STATE_CHANGED(wm->lstate.exp_btns, wm->exp.nunchuk.btns);

			CROSS_THRESH(wm->lstate.exp_orient, wm->exp.nunchuk.orient, wm->exp.nunchuk.orient_threshold);
			CROSS_THRESH_XYZ(wm->lstate.exp_accel, wm->exp.nunchuk.accel, wm->exp.nunchuk.accel_threshold);
			break;
		}
		case EXP_CLASSIC:
		{
			STATE_CHANGED(wm->lstate.exp_ljs_ang, wm->exp.classic.ljs.ang);
			STATE_CHANGED(wm->lstate.exp_ljs_mag, wm->exp.classic.ljs.mag);
			STATE_CHANGED(wm->lstate.exp_rjs_ang, wm->exp.classic.rjs.ang);
			STATE_CHANGED(wm->lstate.exp_rjs_mag, wm->exp.classic.rjs.mag);
			STATE_CHANGED(wm->lstate.exp_r_shoulder, wm->exp.classic.r_shoulder);
			STATE_CHANGED(wm->lstate.exp_l_shoulder, wm->exp.classic.l_shoulder);
			STATE_CHANGED(wm->lstate.exp_btns, wm->exp.classic.btns);
			break;
		}
		case EXP_GUITAR_HERO_3:
		{
			STATE_CHANGED(wm->lstate.exp_ljs_ang, wm->exp.gh3.js.ang);
			STATE_CHANGED(wm->lstate.exp_ljs_mag, wm->exp.gh3.js.mag);
			STATE_CHANGED(wm->lstate.exp_r_shoulder, wm->exp.gh3.whammy_bar);
			STATE_CHANGED(wm->lstate.exp_btns, wm->exp.gh3.btns);
			break;
		}
		case EXP_NONE:
		{
			break;
		}
	}

	STATE_CHANGED(wm->lstate.btns, wm->btns);

	return 0;
}

static void event_data_read(struct wiimote_t *wm,ubyte *msg)
{
	ubyte err;
	ubyte len;
	uword offset;
	struct op_t *op;
	struct cmd_blk_t *cmd = wm->cmd_head;

	wiiuse_pressed_buttons(wm,msg);
	
	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_READ_DATA)) return;

	//printf("event_data_read(%p)\n",cmd);

	err = msg[2]&0x0f;
	op = (struct op_t*)cmd->data;
	if(err) {
		wm->cmd_head = cmd->next;

		cmd->state = CMD_DONE;
		if(cmd->cb!=NULL) cmd->cb(wm,op->buffer,(op->readdata.size - op->wait));

		__lwp_queue_append(&wm->cmdq,&cmd->node);
		wiiuse_send_next_command(wm);
		return;
	}

	len = ((msg[2]&0xf0)>>4)+1;
	offset = BIG_ENDIAN_SHORT(*(uword*)(msg+3));
	
	//printf("addr: %08x\noffset: %d\nlen: %d\n",req->addr,offset,len);

	op->readdata.addr = (op->readdata.addr&0xffff);
	op->wait -= len;
	if(op->wait>=op->readdata.size) op->wait = 0;

	memcpy((op->buffer+offset-op->readdata.addr),(msg+5),len);
	if(!op->wait) {
		wm->cmd_head = cmd->next;

		wm->event = WIIUSE_READ_DATA;
		cmd->state = CMD_DONE;
		if(cmd->cb!=NULL) cmd->cb(wm,op->buffer,op->readdata.size);

		__lwp_queue_append(&wm->cmdq,&cmd->node);
		wiiuse_send_next_command(wm);
	}
}

static void event_data_write(struct wiimote_t *wm,ubyte *msg)
{
	struct cmd_blk_t *cmd = wm->cmd_head;

	wiiuse_pressed_buttons(wm,msg);

	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_WRITE_DATA)) return;

	//printf("event_data_write(%p)\n",cmd);

	wm->cmd_head = cmd->next;

	wm->event = WIIUSE_WRITE_DATA;
	cmd->state = CMD_DONE;
	if(cmd->cb) cmd->cb(wm,NULL,0);

	__lwp_queue_append(&wm->cmdq,&cmd->node);
	wiiuse_send_next_command(wm);
}

static void event_status(struct wiimote_t *wm,ubyte *msg)
{
	int ir = 0;
	int attachment = 0;
	int led[4]= {0};
	int exp_changed = 0;
	struct cmd_blk_t *cmd = wm->cmd_head;

	//printf("event_status(%p)\n",cmd);

	wiiuse_pressed_buttons(wm,msg);

	wm->event = WIIUSE_STATUS;
	if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_1) led[0] = 1;
	if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_2) led[1] = 1;
	if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_3) led[2] = 1;
	if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_4) led[3] = 1;

	if((msg[2]&WM_CTRL_STATUS_BYTE1_ATTACHMENT)==WM_CTRL_STATUS_BYTE1_ATTACHMENT) attachment = 1;
		
	if((msg[2]&WM_CTRL_STATUS_BYTE1_IR_ENABLED)==WM_CTRL_STATUS_BYTE1_IR_ENABLED) ir = 1;

	wm->battery_level = ((msg[5] / (float)WM_MAX_BATTERY_CODE)*100.0F);

	if(attachment && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP)) {
		wiiuse_handshake_expansion(wm,NULL,0);
		exp_changed = 1;
	} else if(!attachment && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP)) {
		wiiuse_disable_expansion(wm);
		exp_changed = 0;
	}

	if(!exp_changed) {
		if(!ir && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) {
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
			wiiuse_set_ir(wm, 1);
		} else
			wiiuse_set_report_type(wm,NULL);
	}

	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_CTRL_STATUS)) return;

	wm->cmd_head = cmd->next;

	cmd->state = CMD_DONE;
	if(cmd->cb!=NULL) cmd->cb(wm,msg,6);
	
	__lwp_queue_append(&wm->cmdq,&cmd->node);
	wiiuse_send_next_command(wm);
}

static void handle_expansion(struct wiimote_t *wm,ubyte *msg)
{
	switch (wm->exp.type) {
		case EXP_NUNCHUK:
			nunchuk_event(&wm->exp.nunchuk, msg);
			break;
		case EXP_CLASSIC:
			classic_ctrl_event(&wm->exp.classic, msg);
			break;
		case EXP_GUITAR_HERO_3:
			guitar_hero_3_event(&wm->exp.gh3, msg);
			break;
		default:
			break;
	}
}

/**
 *	@brief Called on a cycle where no significant change occurs.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
void idle_cycle(struct wiimote_t* wm) 
{
	/*
	 *	Smooth the angles.
	 *
	 *	This is done to make sure that on every cycle the orientation
	 *	angles are smoothed.  Normally when an event occurs the angles
	 *	are updated and smoothed, but if no packet comes in then the
	 *	angles remain the same.  This means the angle wiiuse reports
	 *	is still an old value.  Smoothing needs to be applied in this
	 *	case in order for the angle it reports to converge to the true
	 *	angle of the device.
	 */
	//printf("idle_cycle()\n");///
	if (WIIUSE_USING_ACC(wm) && WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING)) {
		apply_smoothing(&wm->accel_calib, &wm->orient, SMOOTH_ROLL);
		apply_smoothing(&wm->accel_calib, &wm->orient, SMOOTH_PITCH);
	}
}

void parse_event(struct wiimote_t *wm)
{
	ubyte event;
	ubyte *msg;

	//save_state(wm);

	event = wm->event_buf[0];
	msg = wm->event_buf+1;
	switch(event) {
		case WM_RPT_CTRL_STATUS:
			event_status(wm,msg);
			return;
		case WM_RPT_READ:
			event_data_read(wm,msg);
			return;
		case WM_RPT_WRITE:
			event_data_write(wm,msg);
			return;
		case WM_RPT_BTN:
			wiiuse_pressed_buttons(wm,msg);
			break;
		case WM_RPT_BTN_ACC:
			wiiuse_pressed_buttons(wm,msg);

			wm->accel.x = msg[2];
			wm->accel.y = msg[3];
			wm->accel.z = msg[4];

			/* calculate the remote orientation */
			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));

			/* calculate the gforces on each axis */
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
			break;
		case WM_RPT_BTN_ACC_IR:
			wiiuse_pressed_buttons(wm,msg);

			wm->accel.x = msg[2];
			wm->accel.y = msg[3];
			wm->accel.z = msg[4];

			/* calculate the remote orientation */
			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));

			/* calculate the gforces on each axis */
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
			
			calculate_extended_ir(wm, msg+5);
			break;
		case WM_RPT_BTN_EXP:
			wiiuse_pressed_buttons(wm,msg);
			handle_expansion(wm,msg+2);
			break;
		case WM_RPT_BTN_ACC_EXP:
			/* button - motion - expansion */
			wiiuse_pressed_buttons(wm, msg);

			wm->accel.x = msg[2];
			wm->accel.y = msg[3];
			wm->accel.z = msg[4];

			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);

			handle_expansion(wm, msg+5);
			break;
		case WM_RPT_BTN_IR_EXP:
			wiiuse_pressed_buttons(wm,msg);
			calculate_basic_ir(wm, msg+2);
			handle_expansion(wm,msg+12);
			break;
		case WM_RPT_BTN_ACC_IR_EXP:
			/* button - motion - ir - expansion */
			wiiuse_pressed_buttons(wm, msg);

			wm->accel.x = msg[2];
			wm->accel.y = msg[3];
			wm->accel.z = msg[4];

			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);

			/* ir */
			calculate_basic_ir(wm, msg+5);

			handle_expansion(wm, msg+15);
			break;
		default:
			WIIUSE_WARNING("Unknown event, can not handle it [Code 0x%x].", event);
			return;
	}

	/* was there an event? */
	//if(state_changed(wm)) 
	wm->event = WIIUSE_EVENT;
}

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wiiuse_pressed_buttons(struct wiimote_t* wm, ubyte* msg) {
	short now;

	/* convert to big endian */
	now = BIG_ENDIAN_SHORT(*(short*)msg) & WIIMOTE_BUTTON_ALL;

	/* pressed now & were pressed, then held */
	wm->btns_held = (now & wm->btns);

	/* were pressed or were held & not pressed now, then released */
	wm->btns_released = ((wm->btns | wm->btns_held) & ~now);

	/* buttons pressed now */
	wm->btns = now;
}
