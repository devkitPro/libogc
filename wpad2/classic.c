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
 *	$Header: /lvm/shared/ds/ds/cvs/devkitpro-cvsbackup/libogc/wiiuse/classic.c,v 1.7 2008-11-14 13:34:57 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief Classic controller expansion device.
 */

#include "classic.h"

#include "dynamics.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static void fix_bad_calibration_values(struct joystick_t *js, short right_stick) {
	if ((js->min.x >= js->center.x) || (js->max.x <= js->center.x)) {
		js->min.x = 0;
		js->max.x = right_stick ? 32 : 64;
		js->center.x = right_stick ? 16 : 32;
	}
	if ((js->min.y >= js->center.y) || (js->max.y <= js->center.y)) {
		js->min.y = 0;
		js->max.y = right_stick ? 32 : 64;
		js->center.y = right_stick ? 16 : 32;
	}
}

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param msg		The message byte specified in the event packet.
 */
static void classic_ctrl_pressed_buttons(struct classic_ctrl_t *cc, const uint8_t *now) {
	u32 buttons = (now[0] << 0x8) | now[1];

	if (cc->type == CLASSIC_TYPE_WIIU) {
		/* append Wii U Pro Controller stick buttons to top 16 bits */
		buttons |= (now[2] << 0x10);

		/* message is inverted (0 is active, 1 is inactive) */
		buttons = ~buttons & WII_U_PRO_CTRL_BUTTON_ALL;
	} else {
		/* message is inverted (0 is active, 1 is inactive) */
		buttons = ~buttons & CLASSIC_CTRL_BUTTON_ALL;
	}

	/* preserve old btns pressed */
	cc->btns_last = cc->btns;

	/* pressed now & were pressed, then held */
	cc->btns_held = (buttons & cc->btns);

	/* were pressed or were held & not pressed now, then released */
	cc->btns_released = ((cc->btns | cc->btns_held) & ~buttons);

	/* buttons pressed now */
	cc->btns = buttons;
}

void _wpad2_classic_calibrate(struct classic_ctrl_t *cc,
                              const WpadDeviceExpCalibrationData *cd)
{
	const uint8_t *data = cd->data;

	cc->btns = 0;
	cc->btns_held = 0;
	cc->btns_released = 0;

	/* is this a wiiu pro? */
	if (cc->type == CLASSIC_TYPE_WIIU) {
		cc->ljs.max.x = cc->ljs.max.y = 208;
		cc->ljs.min.x = cc->ljs.min.y = 48;
		cc->ljs.center.x = cc->ljs.center.y = 0x80;

		cc->rjs = cc->ljs;
	} else {
		/* joystick stuff */
		cc->ljs.max.x = data[0] / 4 == 0 ? 64 : data[0] / 4;
		cc->ljs.min.x = data[1] / 4;
		cc->ljs.center.x = data[2] / 4 == 0 ? 32 : data[2] / 4;
		cc->ljs.max.y = data[3] / 4 == 0 ? 64 : data[3] / 4;
		cc->ljs.min.y = data[4] / 4;
		cc->ljs.center.y = data[5] / 4 == 0 ? 32 : data[5] / 4;

		cc->rjs.max.x = data[6] / 8 == 0 ? 32 : data[6] / 8;
		cc->rjs.min.x = data[7] / 8;
		cc->rjs.center.x = data[8] / 8 == 0 ? 16 : data[8] / 8;
		cc->rjs.max.y = data[9] / 8 == 0 ? 32 : data[9] / 8;
		cc->rjs.min.y = data[10] / 8;
		cc->rjs.center.y = data[11] / 8 == 0 ? 16 : data[11] / 8;

		fix_bad_calibration_values(&cc->ljs, 0);
		fix_bad_calibration_values(&cc->rjs, 1);
	}
}

/**
 *	@brief Handle classic controller event.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param msg		The message specified in the event packet.
 */
void _wpad2_classic_event(struct classic_ctrl_t *cc, const uint8_t *msg) {
	if (cc->type == CLASSIC_TYPE_WIIU) {
		classic_ctrl_pressed_buttons(cc, msg + 8);

		/* 12-bit little endian values adjusted to 8-bit */
		cc->ljs.pos.x = (msg[0] >> 4) | (msg[1] << 4);
		cc->rjs.pos.x = (msg[2] >> 4) | (msg[3] << 4);
		cc->ljs.pos.y = (msg[4] >> 4) | (msg[5] << 4);
		cc->rjs.pos.y = (msg[6] >> 4) | (msg[7] << 4);

		cc->ls_raw = cc->btns & CLASSIC_CTRL_BUTTON_FULL_L ? 0x1F : 0;
		cc->rs_raw = cc->btns & CLASSIC_CTRL_BUTTON_FULL_R ? 0x1F : 0;

		/* Wii U pro controller specific data */
		cc->charging = !(((msg[10] & WII_U_PRO_CTRL_CHARGING) >> 2) & 1);
		cc->wired = !(((msg[10] & WII_U_PRO_CTRL_WIRED) >> 3) & 1);
		cc->battery = ((msg[10] & WII_U_PRO_CTRL_BATTERY) >> 4) & 7;
	} else {
		classic_ctrl_pressed_buttons(cc, msg + 4);

		/* left/right triggers */
		cc->ls_raw = (((msg[2] & 0x60) >> 2) | ((msg[3] & 0xE0) >> 5));
		cc->rs_raw = (msg[3] & 0x1F);

		/* calculate joystick orientation */
		cc->ljs.pos.x = (msg[0] & 0x3F);
		cc->ljs.pos.y = (msg[1] & 0x3F);
		cc->rjs.pos.x = ((msg[0] & 0xC0) >> 3) | ((msg[1] & 0xC0) >> 5) | ((msg[2] & 0x80) >> 7);
		cc->rjs.pos.y = (msg[2] & 0x1F);
	}
}
