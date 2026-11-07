/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header: /lvm/shared/ds/ds/cvs/devkitpro-cvsbackup/libogc/wiiuse/dynamics.c,v 1.2 2008-11-14 13:34:57 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief Handles the dynamics of the wiimote.
 *
 *	The file includes functions that handle the dynamics
 *	of the wiimote.  Such dynamics include orientation and
 *	motion sensing.
 */

#include "dynamics.h"

#include "guitar_hero_3.h"
#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void apply_smoothing(struct accel_t *ac, struct orient_t *orient, int type) {
	switch (type) {
	case SMOOTH_ROLL:
		{
			/* it's possible last iteration was nan or inf, so set it to 0 if that happened */
			if (isnan(ac->st_roll) || isinf(ac->st_roll))
				ac->st_roll = 0.0f;

			/*
			 *	If the sign changes (which will happen if going from -180 to 180)
			 *	or from (-1 to 1) then don't smooth, just use the new angle.
			 */
			if (((ac->st_roll < 0) && (orient->roll > 0)) || ((ac->st_roll > 0) && (orient->roll < 0))) {
				ac->st_roll = orient->roll;
			} else {
				orient->roll = ac->st_roll + (ac->st_alpha * (orient->a_roll - ac->st_roll));
				ac->st_roll = orient->roll;
			}

			return;
		}

	case SMOOTH_PITCH:
		{
			if (isnan(ac->st_pitch) || isinf(ac->st_pitch))
				ac->st_pitch = 0.0f;

			if (((ac->st_pitch < 0) && (orient->pitch > 0)) || ((ac->st_pitch > 0) && (orient->pitch < 0))) {
				ac->st_pitch = orient->pitch;
			} else {
				orient->pitch = ac->st_pitch + (ac->st_alpha * (orient->a_pitch - ac->st_pitch));
				ac->st_pitch = orient->pitch;
			}

			return;
		}
	}
}

/**
 *	@brief Calculate the roll, pitch, yaw.
 *
 *	@param ac			An accelerometer (accel_t) structure.
 *	@param accel		[in] Pointer to a vec3w_t structure that holds the raw acceleration data.
 *	@param orient		[out] Pointer to a orient_t structure that will hold the orientation data.
 *	@param rorient		[out] Pointer to a orient_t structure that will hold the non-smoothed orientation data.
 *	@param smooth		If smoothing should be performed on the angles calculated. 1 to enable, 0 to disable.
 *
 *	Given the raw acceleration data from the accelerometer struct, calculate
 *	the orientation of the device and set it in the \a orient parameter.
 */
void calculate_orientation(struct accel_t *ac, struct vec3w_t *accel, struct orient_t *orient, int smooth) {
	float xg, yg, zg;
	float x, y, z;

	/*
	 *	roll	- use atan(z / x)		[ ranges from -180 to 180 ]
	 *	pitch	- use atan(z / y)		[ ranges from -180 to 180 ]
	 *	yaw		- impossible to tell without IR
	 */

	/* yaw - set to 0, IR will take care of it if it's enabled */
	orient->yaw = 0.0f;

	/* find out how much it has to move to be 1g */
	xg = (float)ac->cal_g.x;
	yg = (float)ac->cal_g.y;
	zg = (float)ac->cal_g.z;

	/* find out how much it actually moved and normalize to +/- 1g */
	x = ((float)accel->x - (float)ac->cal_zero.x) / xg;
	y = ((float)accel->y - (float)ac->cal_zero.y) / yg;
	z = ((float)accel->z - (float)ac->cal_zero.z) / zg;

	/* make sure x,y,z are between -1 and 1 for the tan functions */
	if (x < -1.0f)                  x = -1.0f;
	else if (x > 1.0f)              x = 1.0f;
	if (y < -1.0f)                  y = -1.0f;
	else if (y > 1.0f)              y = 1.0f;
	if (z < -1.0f)                  z = -1.0f;
	else if (z > 1.0f)              z = 1.0f;

	/* if it is over 1g then it is probably accelerating and not reliable */
	if (abs(accel->x - ac->cal_zero.x) <= (ac->cal_g.x + 10)) {
		/* roll */
		x = RAD_TO_DEGREE(atan2f(x, z));
		if (isfinite(x)) {
			orient->roll = x;
			orient->a_roll = x;
		}
	}

	if (abs(accel->y - ac->cal_zero.y) <= (ac->cal_g.y + 10)) {
		/* pitch */
		y = RAD_TO_DEGREE(atan2f(y, z));
		if (isfinite(y)) {
			orient->pitch = y;
			orient->a_pitch = y;
		}
	}

	/* smooth the angles if enabled */
	if (smooth) {
		apply_smoothing(ac, orient, SMOOTH_ROLL);
		apply_smoothing(ac, orient, SMOOTH_PITCH);
	}
}


/**
 *	@brief Calculate the gravity forces on each axis.
 *
 *	@param ac			An accelerometer (accel_t) structure.
 *	@param accel		[in] Pointer to a vec3w_t structure that holds the raw acceleration data.
 *	@param gforce		[out] Pointer to a gforce_t structure that will hold the gravity force data.
 */
void calculate_gforce(struct accel_t *ac, struct vec3w_t *accel, struct gforce_t *gforce) {
	float xg, yg, zg;

	/* find out how much it has to move to be 1g */
	xg = (float)ac->cal_g.x;
	yg = (float)ac->cal_g.y;
	zg = (float)ac->cal_g.z;

	/* find out how much it actually moved and normalize to +/- 1g */
	gforce->x = ((float)accel->x - (float)ac->cal_zero.x) / xg;
	gforce->y = ((float)accel->y - (float)ac->cal_zero.y) / yg;
	gforce->z = ((float)accel->z - (float)ac->cal_zero.z) / zg;
}


/**
 *	@brief Calculate the angle and magnitude of a joystick.
 *
 *	@param js	[out] Pointer to a joystick_t structure.
 *	@param x	The raw x-axis value.
 *	@param y	The raw y-axis value.
 */
void calc_joystick_state(struct joystick_t *js, float x, float y) {
	float rx, ry;

	/*
	 *	Since the joystick center may not be exactly:
	 *		(min + max) / 2
	 *	Then the range from the min to the center and the center to the max
	 *	may be different.
	 *	Because of this, depending on if the current x or y value is greater
	 *	or less than the assoicated axis center value, it needs to be interpolated
	 *	between the center and the minimum or maxmimum rather than between
	 *	the minimum and maximum.
	 *
	 *	So we have something like this:
	 *		(x min) [-1] ---------*------ [0] (x center) [0] -------- [1] (x max)
	 *	Where the * is the current x value.
	 *	The range is therefore -1 to 1, 0 being the exact center rather than
	 *	the middle of min and max.
	 */
	if (x == js->center.x)
		rx = 0;
	else if (x >= js->center.x)
		rx = ((float)(x - js->center.x) / (float)(js->max.x - js->center.x));
	else
		rx = ((float)(x - js->min.x) / (float)(js->center.x - js->min.x)) - 1.0f;

	if (y == js->center.y)
		ry = 0;
	else if (y >= js->center.y)
		ry = ((float)(y - js->center.y) / (float)(js->max.y - js->center.y));
	else
		ry = ((float)(y - js->min.y) / (float)(js->center.y - js->min.y)) - 1.0f;

	/* calculate the joystick angle and magnitude */
	js->ang = RAD_TO_DEGREE(atan2f(rx, ry));
	js->mag = hypotf(rx, ry);
}


void calc_balanceboard_state(struct wii_board_t *wb)
{
	/*
	Interpolate values
	Calculations borrowed from wiili.org - No names to mention sadly :( http://www.wiili.org/index.php/Wii_Balance_Board_PC_Drivers
	*/

	if (wb->rtr < wb->ctr[1]) {
		wb->tr = 17.0f * (f32)(wb->rtr - wb->ctr[0]) / (f32)(wb->ctr[1] - wb->ctr[0]);
	} else {
		wb->tr = 17.0f * (f32)(wb->rtr - wb->ctr[1]) / (f32)(wb->ctr[2] - wb->ctr[1]) + 17.0f;
	}

	if (wb->rtl < wb->ctl[1]) {
		wb->tl = 17.0f * (f32)(wb->rtl - wb->ctl[0]) / (f32)(wb->ctl[1] - wb->ctl[0]);
	} else {
		wb->tl = 17.0f * (f32)(wb->rtl - wb->ctl[1]) / (f32)(wb->ctl[2] - wb->ctl[1]) + 17.0f;
	}

	if (wb->rbr < wb->cbr[1]) {
		wb->br = 17.0f * (f32)(wb->rbr - wb->cbr[0]) / (f32)(wb->cbr[1] - wb->cbr[0]);
	} else {
		wb->br = 17.0f * (f32)(wb->rbr - wb->cbr[1]) / (f32)(wb->cbr[2] - wb->cbr[1]) + 17.0f;
	}

	if (wb->rbl < wb->cbl[1]) {
		wb->bl = 17.0f * (f32)(wb->rbl - wb->cbl[0]) / (f32)(wb->cbl[1] - wb->cbl[0]);
	} else {
		wb->bl = 17.0f * (f32)(wb->rbl - wb->cbl[1]) / (f32)(wb->cbl[2] - wb->cbl[1]) + 17.0f;
	}

	wb->x = ((wb->tr + wb->br) - (wb->tl + wb->bl)) / 2.0f;
	wb->y = ((wb->bl + wb->br) - (wb->tl + wb->tr)) / 2.0f;
}

void _wpad2_calc_data(WPADData *data, const WPADData *lstate,
                      struct accel_t *accel_calib, bool smoothed)
{
	if (data->err != WPAD_ERR_NONE) return;

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
	data->ir.aspect = lstate->ir.aspect;
	data->ir.vres[0] = lstate->ir.vres[0];
	data->ir.vres[1] = lstate->ir.vres[1];
	data->ir.offset[0] = lstate->ir.offset[0];
	data->ir.offset[1] = lstate->ir.offset[1];

	data->btns_l = lstate->btns_h;
	if (data->data_present & WPAD_DATA_ACCEL) {
		calculate_orientation(accel_calib, &data->accel, &data->orient, smoothed);
		calculate_gforce(accel_calib, &data->accel, &data->gforce);
	}
	if (data->data_present & WPAD_DATA_IR) {
		_wpad2_interpret_ir_data(&data->ir, &data->orient);
	}
	if (data->data_present & WPAD_DATA_EXPANSION) {
		WPAD2_DEBUG("calculating data with expansion %d", data->exp.type);
		switch (data->exp.type) {
		case EXP_NUNCHUK:
			{
				struct nunchuk_t *nc = &data->exp.nunchuk;

				nc->orient = lstate->exp.nunchuk.orient;
				calc_joystick_state(&nc->js, nc->js.pos.x, nc->js.pos.y);
				calculate_orientation(&nc->accel_calib, &nc->accel, &nc->orient, smoothed);
				calculate_gforce(&nc->accel_calib, &nc->accel, &nc->gforce);
				data->btns_h |= (data->exp.nunchuk.btns << 16);
			}
			break;

		case EXP_CLASSIC:
			{
				struct classic_ctrl_t *cc = &data->exp.classic;

				cc->r_shoulder = ((f32)cc->rs_raw / 0x1F);
				cc->l_shoulder = ((f32)cc->ls_raw / 0x1F);
				calc_joystick_state(&cc->ljs, cc->ljs.pos.x, cc->ljs.pos.y);
				calc_joystick_state(&cc->rjs, cc->rjs.pos.x, cc->rjs.pos.y);

				/* overwrite Wiimote buttons (unused) with extra Wii U Pro Controller stick buttons */
				if (data->exp.classic.type == CLASSIC_TYPE_WIIU)
					data->btns_h = (data->exp.classic.btns & WII_U_PRO_CTRL_BUTTON_EXTRA) >> 16;

				data->btns_h |= ((data->exp.classic.btns & CLASSIC_CTRL_BUTTON_ALL) << 16);
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
				data->btns_h |= (data->exp.gh3.btns << 16);
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
}
