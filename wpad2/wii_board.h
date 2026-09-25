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
 *	$Header: /cvsroot/devkitpro/libogc/wiiuse/wiiboard.h,v 1.1 2008/05/08 09:42:14 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief Wii board expansion device.
 */

#ifndef WII_BOARD_H
#define WII_BOARD_H

#include "device.h"

void _wpad2_wii_board_calibrate(struct wii_board_t *wb,
                                const WpadDeviceExpCalibrationData *cd);
void _wpad2_wii_board_event(struct wii_board_t *wb, const uint8_t *msg);

#endif /* WPAD2_WII_BOARD_H */
