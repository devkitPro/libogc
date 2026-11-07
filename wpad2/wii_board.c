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
 *	$Header: /cvsroot/devkitpro/libogc/wiiuse/wiiboard.c,v 1.6 2008/05/26 19:24:53 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief Wiiboard expansion device.
 */

#include "wii_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

void _wpad2_wii_board_calibrate(struct wii_board_t *wb,
                                const WpadDeviceExpCalibrationData *cd)
{
	const uint8_t *data = cd->data;

	wb->ctr[0] = (data[4] << 8) | data[5];
	wb->cbr[0] = (data[6] << 8) | data[7];
	wb->ctl[0] = (data[8] << 8) | data[9];
	wb->cbl[0] = (data[10] << 8) | data[11];

	wb->ctr[1] = (data[12] << 8) | data[13];
	wb->cbr[1] = (data[14] << 8) | data[15];
	wb->ctl[1] = (data[16] << 8) | data[17];
	wb->cbl[1] = (data[18] << 8) | data[19];

	wb->ctr[2] = (data[20] << 8) | data[21];
	wb->cbr[2] = (data[22] << 8) | data[23];
	wb->ctl[2] = (data[24] << 8) | data[25];
	wb->cbl[2] = (data[26] << 8) | data[27];
}

void _wpad2_wii_board_event(struct wii_board_t *wb, const uint8_t *msg)
{
	wb->rtr = (msg[0] << 8) | msg[1];
	wb->rbr = (msg[2] << 8) | msg[3];
	wb->rtl = (msg[4] << 8) | msg[5];
	wb->rbl = (msg[6] << 8) | msg[7];
}
