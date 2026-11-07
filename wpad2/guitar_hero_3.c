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
 *	$Header: /lvm/shared/ds/ds/cvs/devkitpro-cvsbackup/libogc/wiiuse/guitar_hero_3.c,v 1.7 2008-11-14 13:34:57 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief Guitar Hero 3 expansion device.
 */

#include "guitar_hero_3.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/**
 *	@brief Handle the handshake data from the guitar.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param data		The data read in from the device.
 *	@param len		The length of the data block, in bytes.
 *
 *	@return	Returns 1 if handshake was successful, 0 if not.
 */
void _wpad2_guitar_hero_3_calibrate(struct guitar_hero_3_t *gh3,
                                    const WpadDeviceExpCalibrationData *) {
	/*
	 *	The good fellows that made the Guitar Hero 3 controller
	 *	failed to factory calibrate the devices.  There is no
	 *	calibration data on the device.
	 */

	gh3->btns = 0;
	gh3->btns_held = 0;
	gh3->btns_released = 0;
	gh3->wb_raw = 0;
	gh3->whammy_bar = 0.0f;
	gh3->tb_raw = 0;
	gh3->touch_bar = -1;

	/* joystick stuff */
	gh3->js.max.x = GUITAR_HERO_3_JS_MAX_X;
	gh3->js.min.x = GUITAR_HERO_3_JS_MIN_X;
	gh3->js.center.x = GUITAR_HERO_3_JS_CENTER_X;
	gh3->js.max.y = GUITAR_HERO_3_JS_MAX_Y;
	gh3->js.min.y = GUITAR_HERO_3_JS_MIN_Y;
	gh3->js.center.y = GUITAR_HERO_3_JS_CENTER_Y;
}

static void guitar_hero_3_pressed_buttons(struct guitar_hero_3_t *gh3, uint16_t now) {
	/* message is inverted (0 is active, 1 is inactive) */
	now = ~now & GUITAR_HERO_3_BUTTON_ALL;

	/* preserve old btns pressed */
	gh3->btns_last = gh3->btns;

	/* pressed now & were pressed, then held */
	gh3->btns_held = (now & gh3->btns);

	/* were pressed or were held & not pressed now, then released */
	gh3->btns_released = ((gh3->btns | gh3->btns_held) & ~now);

	/* buttons pressed now */
	gh3->btns = now;
}

/**
 *	@brief Handle guitar event.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param msg		The message specified in the event packet.
 */
void _wpad2_guitar_hero_3_event(struct guitar_hero_3_t *gh3, const uint8_t *msg) {
	uint16_t buttons = read_be16(msg + 4);
	guitar_hero_3_pressed_buttons(gh3, buttons);

	gh3->js.pos.x = (msg[0] & GUITAR_HERO_3_JS_MASK);
	gh3->js.pos.y = (msg[1] & GUITAR_HERO_3_JS_MASK);
	gh3->tb_raw = (msg[2] & GUITAR_HERO_3_TOUCH_MASK);
	gh3->wb_raw = (msg[3] & GUITAR_HERO_3_WHAMMY_MASK);
}
