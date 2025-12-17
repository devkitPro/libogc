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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef WIN32
	#include <float.h>
#endif

#include "definitions.h"
#include "wiiuse_internal.h"
#include "ir.h"
#include "dynamics.h"

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
void calculate_orientation(struct accel_t* ac, struct vec3w_t* accel, struct orient_t* orient, int smooth) {
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
	if (x < -1.0f)			x = -1.0f;
	else if (x > 1.0f)		x = 1.0f;
	if (y < -1.0f)			y = -1.0f;
	else if (y > 1.0f)		y = 1.0f;
	if (z < -1.0f)			z = -1.0f;
	else if (z > 1.0f)		z = 1.0f;

	/* if it is over 1g then it is probably accelerating and not reliable */
	if (abs(accel->x - ac->cal_zero.x) <= (ac->cal_g.x+10)) {
		/* roll */
		x = RAD_TO_DEGREE(atan2f(x, z));
		if(isfinite(x)) {
			orient->roll = x;
			orient->a_roll = x;
		}
	}

	if (abs(accel->y - ac->cal_zero.y) <= (ac->cal_g.y+10)) {
		/* pitch */
		y = RAD_TO_DEGREE(atan2f(y, z));
		if(isfinite(y)) {
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
void calculate_gforce(struct accel_t* ac, struct vec3w_t* accel, struct gforce_t* gforce) {
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
void calc_joystick_state(struct joystick_t* js, float x, float y) {
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


void apply_smoothing(struct accel_t* ac, struct orient_t* orient, int type) {
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

static float calc_balanceboard_weight(wii_board_t *wb, int idx)
{
	/*
	 * Calculations borrowed from wiili.org - No names to mention sadly :(
	 * http://www.wiili.org/index.php/Wii_Balance_Board_PC_Drivers
	 */
	short raw = wb->raw_sensor[idx];
	const wii_board_sensor_cal_t *cal = &wb->cal_sensor[idx];
	if (raw < cal->ref_17)
		return 17.0f * (raw - cal->ref_0 ) / (cal->ref_17 - cal->ref_0 );
	else
		return 17.0f * (raw - cal->ref_17) / (cal->ref_34 - cal->ref_17) + 17.0f;
}

static float calc_balanceboard_temp_correct(float w, const wii_board_t* wb)
{
	/* Temperature correction, based on https://wiibrew.org/wiki/Wii_Balance_Board */
	static const double a = 0.9990813732147217;
	static const double b = 0.007000000216066837;
	double c = 1.0 - b * (wb->raw_temp - wb->cal_temp) / 10.0;
	return a * w * c;
}

static unsigned calc_balanceboard_battery_bars(const struct wii_board_t *wb)
{
    if (wb->raw_bat >= 0x82)
	return 4;
    if (wb->raw_bat >= 0x7d)
	return 3;
    if (wb->raw_bat >= 0x78)
	return 2;
    if (wb->raw_bat >= wb->cal_bat)
	return 1;
    return 0;
}

void calc_balanceboard_state(struct wii_board_t *wb)
{
	/* Convert raw sensor data to Kg. */
	for (unsigned i = 0; i < WII_BOARD_NUM_SENSORS; ++i)
		wb->weight[i] = calc_balanceboard_weight(wb, i);

	/* Apply temperature and zero correction. */
	for (unsigned i = 0; i < WII_BOARD_NUM_SENSORS; ++i)
		wb->weight[i] = calc_balanceboard_temp_correct(wb->weight[i], wb);

	wb->total_weight = 0;
	for (unsigned i = 0; i < WII_BOARD_NUM_SENSORS; ++i)
		wb->total_weight += wb->weight[i];

	/* Calculate x,y members only if there's at least 2 Kg on the board. */
	if (wb->total_weight >= 2.0f) {
		wb->x = ((wb->weight[WII_BOARD_SENSOR_ID_TOP_RIGHT] +
			  wb->weight[WII_BOARD_SENSOR_ID_BOT_RIGHT])
			 -
			 (wb->weight[WII_BOARD_SENSOR_ID_TOP_LEFT] +
			  wb->weight[WII_BOARD_SENSOR_ID_BOT_LEFT]))
			/ wb->total_weight;
		wb->y = ((wb->weight[WII_BOARD_SENSOR_ID_TOP_RIGHT] +
			  wb->weight[WII_BOARD_SENSOR_ID_TOP_LEFT])
			 -
			 (wb->weight[WII_BOARD_SENSOR_ID_BOT_RIGHT] +
			  wb->weight[WII_BOARD_SENSOR_ID_BOT_LEFT]))
			/ wb->total_weight;
	} else {
		wb->x = 0;
		wb->y = 0;
	}

	wb->battery = calc_balanceboard_battery_bars(wb);
}
