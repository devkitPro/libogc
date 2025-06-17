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
 *	$Header: /lvm/shared/ds/ds/cvs/devkitpro-cvsbackup/libogc/wiiuse/wiiuse_internal.h,v 1.8 2008-12-10 16:16:40 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief General internal wiiuse stuff.
 *
 *	Since Wiiuse is a library, wiiuse.h is a duplicate
 *	of the API header.
 *
 *	The code that would normally go in that file, but
 *	which is not needed by third party developers,
 *	is put here.
 *
 *	So wiiuse_internal.h is included by other files
 *	internally, wiiuse.h is included only here.
 */

#ifndef WIIUSE_INTERNAL_H_INCLUDED
#define WIIUSE_INTERNAL_H_INCLUDED

#if defined(__linux__)
	#include <arpa/inet.h>				/* htons() */
	#include <bluetooth/bluetooth.h>
#endif

#include "definitions.h"

/* wiiuse version */
#define WIIUSE_VERSION					"0.12"

/********************
 *
 *	Wiimote internal codes
 *
 ********************/

/* Communication channels */
#define WM_OUTPUT_CHANNEL			0x11
#define WM_INPUT_CHANNEL			0x13

#define WM_SET_REPORT				0x50
#define WM_DATA						0xA0

/* commands */
#define WM_CMD_RUMBLE				0x10
#define WM_CMD_LED					0x11
#define WM_CMD_REPORT_TYPE			0x12
#define WM_CMD_IR					0x13
#define WM_CMD_SPEAKER_ENABLE		0x14
#define WM_CMD_CTRL_STATUS			0x15
#define WM_CMD_WRITE_DATA			0x16
#define WM_CMD_READ_DATA			0x17
#define WM_CMD_STREAM_DATA			0x18
#define WM_CMD_SPEAKER_MUTE			0x19
#define WM_CMD_IR_2					0x1A

/* input report ids */
#define WM_RPT_CTRL_STATUS			0x20
#define WM_RPT_READ					0x21
#define WM_RPT_ACK					0x22
#define WM_RPT_BTN					0x30
#define WM_RPT_BTN_ACC				0x31
#define WM_RPT_BTN_ACC_IR			0x33
#define WM_RPT_BTN_EXP				0x34
#define WM_RPT_BTN_ACC_EXP			0x35
#define WM_RPT_BTN_IR_EXP			0x36
#define WM_RPT_BTN_ACC_IR_EXP		0x37

#define WM_BT_INPUT					0x01
#define WM_BT_OUTPUT				0x02

/* Identify the wiimote device by its class */
#define WM_DEV_CLASS_0				0x04
#define WM_DEV_CLASS_1				0x25
#define WM_DEV_CLASS_2				0x00
#define WM_VENDOR_ID				0x057E
#define WM_PRODUCT_ID				0x0306

/* controller status stuff */
#define WM_MAX_BATTERY_CODE			0xC8

/* offsets in wiimote memory */
#define WM_MEM_OFFSET_CALIBRATION	0x16
#define WM_EXP_MEM_BASE				0x04A40000
#define WM_EXP_MEM_ENABLE1			0x04A400F0
#define WM_EXP_MEM_ENABLE2			0x04A400FB
#define WM_EXP_MEM_KEY				0x04A40040
#define WM_EXP_MEM_CALIBR			0x04A40020
#define WM_EXP_MOTION_PLUS_ENABLE	0x04A600F0
#define WM_EXP_MOTION_PLUS_MODE		0x04A600FE
#define WM_EXP_ID					0x04A400FA

#define WM_REG_IR					0x04B00030
#define WM_REG_IR_BLOCK1			0x04B00000
#define WM_REG_IR_BLOCK2			0x04B0001A
#define WM_REG_IR_MODENUM			0x04B00033

#define WM_REG_SPEAKER_REG1			0x04A20001
#define WM_REG_SPEAKER_REG2			0x04A20008
#define WM_REG_SPEAKER_REG3			0x04A20009
#define WM_REG_SPEAKER_BLOCK		0x04A20001

/* ir block data */
#define WM_IR_BLOCK1_LEVEL1			"\x02\x00\x00\x71\x01\x00\x64\x00\xfe"
#define WM_IR_BLOCK2_LEVEL1			"\xfd\x05"
#define WM_IR_BLOCK1_LEVEL2			"\x02\x00\x00\x71\x01\x00\x96\x00\xb4"
#define WM_IR_BLOCK2_LEVEL2			"\xb3\x04"
#define WM_IR_BLOCK1_LEVEL3			"\x02\x00\x00\x71\x01\x00\xaa\x00\x64"
#define WM_IR_BLOCK2_LEVEL3			"\x63\x03"
#define WM_IR_BLOCK1_LEVEL4			"\x02\x00\x00\x71\x01\x00\xc8\x00\x36"
#define WM_IR_BLOCK2_LEVEL4			"\x35\x03"
#define WM_IR_BLOCK1_LEVEL5			"\x07\x00\x00\x71\x01\x00\x72\x00\x20"
#define WM_IR_BLOCK2_LEVEL5			"\x1f\x03"

#define WM_IR_TYPE_BASIC			0x01
#define WM_IR_TYPE_EXTENDED			0x03
#define WM_IR_TYPE_FULL				0x05

/* controller status flags for the first message byte */
#define WM_CTRL_STATUS_BYTE1_BATTERY_CRITICAL	0x01
#define WM_CTRL_STATUS_BYTE1_ATTACHMENT			0x02
#define WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED	0x04
#define WM_CTRL_STATUS_BYTE1_IR_ENABLED			0x08
#define WM_CTRL_STATUS_BYTE1_LED_1				0x10
#define WM_CTRL_STATUS_BYTE1_LED_2				0x20
#define WM_CTRL_STATUS_BYTE1_LED_3				0x40
#define WM_CTRL_STATUS_BYTE1_LED_4				0x80

/* aspect ratio */
#define WM_ASPECT_16_9_X	660
#define WM_ASPECT_16_9_Y	370
#define WM_ASPECT_4_3_X		560
#define WM_ASPECT_4_3_Y		420


/**
 *	Expansion stuff
 */

/* encrypted expansion id codes (located at 0x04A400FC) */
#define EXP_ID_CODE_NUNCHUK							0xa4200000
#define EXP_ID_CODE_CLASSIC_CONTROLLER				0xa4200101
#define EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING		0x90908f00
#define EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING2	0x9e9f9c00
#define EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING3	0x908f8f00
#define EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC		0xa5a2a300
#define EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC2		0x98999900
#define EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC3		0xa0a1a000
#define EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC4		0x8d8d8e00
#define EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC5		0x93949400
#define EXP_ID_CODE_CLASSIC_WIIU_PRO            	0xa4200120
#define EXP_ID_CODE_GUITAR							0xa4200103
#define EXP_ID_CODE_WIIBOARD						0xa4200402
#define EXP_ID_CODE_MOTION_PLUS						0xa4200405

#define EXP_HANDSHAKE_LEN					224

/********************
 *
 *	End Wiimote internal codes
 *
 ********************/

/* wiimote state flags - (some duplicated in wiiuse.h)*/
#define WIIMOTE_STATE_DEV_FOUND					0x000001
#define WIIMOTE_STATE_BATTERY_CRITICAL			0x000002
#define WIIMOTE_STATE_HANDSHAKE					0x000004	/* actual connection exists but no handshake yet */
#define WIIMOTE_STATE_HANDSHAKE_COMPLETE		0x000008	/* actual connection exists but no handshake yet */
#define WIIMOTE_STATE_CONNECTED					0x000010
#define WIIMOTE_STATE_EXP_HANDSHAKE				0x000020	/* actual connection exists but no handshake yet */
#define WIIMOTE_STATE_EXP_FAILED				0x000040	/* actual connection exists but no handshake yet */
#define WIIMOTE_STATE_RUMBLE					0x000080
#define WIIMOTE_STATE_ACC						0x000100
#define WIIMOTE_STATE_EXP						0x000200
#define WIIMOTE_STATE_IR						0x000400
#define WIIMOTE_STATE_SPEAKER					0x000800
#define WIIMOTE_STATE_IR_SENS_LVL1				0x001000
#define WIIMOTE_STATE_IR_SENS_LVL2				0x002000
#define WIIMOTE_STATE_IR_SENS_LVL3				0x004000
#define WIIMOTE_STATE_IR_SENS_LVL4				0x008000
#define WIIMOTE_STATE_IR_SENS_LVL5				0x010000
#define WIIMOTE_STATE_IR_INIT					0x020000
#define WIIMOTE_STATE_SPEAKER_INIT				0x040000
#define WIIMOTE_STATE_WIIU_PRO					0x080000
#define WIIMOTE_STATE_MPLUS_PRESENT				0x100000

#define WIIMOTE_INIT_STATES					(WIIMOTE_STATE_IR_SENS_LVL3)

/* macro to manage states */
#define WIIMOTE_IS_SET(wm, s)			((wm->state & (s)) == (s))
#define WIIMOTE_ENABLE_STATE(wm, s)		(wm->state |= (s))
#define WIIMOTE_DISABLE_STATE(wm, s)	(wm->state &= ~(s))
#define WIIMOTE_TOGGLE_STATE(wm, s)		((wm->state & (s)) ? WIIMOTE_DISABLE_STATE(wm, s) : WIIMOTE_ENABLE_STATE(wm, s))

#define WIIMOTE_IS_FLAG_SET(wm, s)		((wm->flags & (s)) == (s))
#define WIIMOTE_ENABLE_FLAG(wm, s)		(wm->flags |= (s))
#define WIIMOTE_DISABLE_FLAG(wm, s)		(wm->flags &= ~(s))
#define WIIMOTE_TOGGLE_FLAG(wm, s)		((wm->flags & (s)) ? WIIMOTE_DISABLE_FLAG(wm, s) : WIIMOTE_ENABLE_FLAG(wm, s))

#define NUNCHUK_IS_FLAG_SET(wm, s)		((*(wm->flags) & (s)) == (s))

/* misc macros */
#define WIIMOTE_ID(wm)					(wm->unid)
#define WIIMOTE_IS_CONNECTED(wm)		(WIIMOTE_IS_SET(wm, WIIMOTE_STATE_CONNECTED))

/*
 *	Smooth tilt calculations are computed with the
 *	exponential moving average formula:
 *		St = St_last + (alpha * (tilt - St_last))
 *	alpha is between 0 and 1
 */
#define WIIUSE_DEFAULT_SMOOTH_ALPHA		0.3f

#define SMOOTH_ROLL						0x01
#define SMOOTH_PITCH					0x02

#include <wiiuse/wiiuse.h>

#ifdef __cplusplus
extern "C" {
#endif

struct op_t
{
	ubyte cmd;
	union {
		struct {
			uint addr;
			uword size;
		} readdata;
		struct {
			uint addr;
			ubyte size;
			ubyte data[16];
		} writedata;
		ubyte __data[MAX_PAYLOAD];
	};

	void *buffer;
	int wait;
} __attribute__((packed));

/* not part of the api */
void wiiuse_init_cmd_queue(struct wiimote_t *wm);
void wiiuse_send_next_command(struct wiimote_t *wm);
int wiiuse_set_report_type(struct wiimote_t* wm,cmd_blk_cb cb);
int wiiuse_sendcmd(struct wiimote_t *wm,ubyte report_type,ubyte *msg,int len,cmd_blk_cb cb);

#ifdef __cplusplus
}
#endif

#endif /* WIIUSE_INTERNAL_H_INCLUDED */
