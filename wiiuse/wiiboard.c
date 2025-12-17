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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef WIN32
	#include <Winsock2.h>
#endif

#include "definitions.h"
#include "wiiuse_internal.h"
#include "dynamics.h"
#include "events.h"
#include "wiiboard.h"
#include "io.h"

static u16 load_be_u16(u8* data)
{
	return ((u16)(data[0] << 8)) | ((u16)data[1]);
}

/**
 *	@brief Handle the handshake data from the wiiboard.
 *
 *	@param wb		A pointer to a wii_board_t structure.
 *	@param data		The data read in from the device.
 *	@param len		The length of the data block, in bytes.
 *
 *	@return	Returns 1 if handshake was successful, 0 if not.
 */
int wii_board_handshake(struct wiimote_t* wm, struct wii_board_t* wb, ubyte* data, uword len) 
{
	unsigned offset = 0;

	if (data[offset] == 0xff) {
		if (data[offset + 0x10] == 0xff) {
			WIIUSE_DEBUG("Wii Balance Board handshake appears invalid, trying again.");
			wiiuse_read_data(wm, data, WM_EXP_MEM_CALIBR, EXP_HANDSHAKE_LEN, wiiuse_handshake_expansion);
			return 0;
		}
		offset += 0x10;
	}

	
	for (unsigned i = 0; i < WII_BOARD_NUM_SENSORS; ++i) {
		/* The reference values for 0 Kg */
		wb->cal_sensor[i].ref_0  = load_be_u16(data + offset + 0x04 + 2*i);
		/* The reference values for 17 Kg */
		wb->cal_sensor[i].ref_17 = load_be_u16(data + offset + 0x0c + 2*i);
		/* The reference values for 34 Kg */
		wb->cal_sensor[i].ref_34 = load_be_u16(data + offset + 0x14 + 2*i);
	}

	/* The minimum battery value (always 0x6a). */
	wb->cal_bat = data[offset + 0x01];

	/* The reference temperature. */
        wb->cal_temp = data[offset + 0x40];

	/* handshake done */
	wm->event = WIIUSE_WII_BOARD_INSERTED;
	wm->exp.type = EXP_WII_BOARD;

	/* Balance Board only has the first LED, so make sure it's on. */
	wiiuse_set_leds(wm, WIIMOTE_LED_1, NULL);

	return 1; 
}


/**
 *	@brief The wii board disconnected.
 *
 *	@param cc		A pointer to a wii_board_t structure.
 */
void wii_board_disconnected(struct wii_board_t* wb)
{
	memset(wb, 0, sizeof(struct wii_board_t));
}


/**
 *	@brief Handle wii board event.
 *
 *	@param wb		A pointer to a wii_board_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wii_board_event(struct wii_board_t* wb, ubyte* msg)
{
	for (unsigned i = 0; i < WII_BOARD_NUM_SENSORS; ++i)
		wb->raw_sensor[i] = load_be_u16(msg + 2*i);
	wb->raw_temp = msg[0x8];
	wb->raw_bat = msg[0xa];
}
