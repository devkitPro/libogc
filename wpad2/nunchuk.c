#include "nunchuk.h"

#include "dynamics.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param nc		Pointer to a nunchuk_t structure.
 *	@param msg		The message byte specified in the event packet.
 */
static void nunchuk_pressed_buttons(struct nunchuk_t *nc, uint8_t now) {
	/* message is inverted (0 is active, 1 is inactive) */
	now = ~now & NUNCHUK_BUTTON_ALL;

	/* preserve old btns pressed */
	nc->btns_last = nc->btns;

	/* pressed now & were pressed, then held */
	nc->btns_held = (now & nc->btns);

	/* were pressed or were held & not pressed now, then released */
	nc->btns_released = ((nc->btns | nc->btns_held) & ~now);

	/* buttons pressed now */
	nc->btns = now;
}

void _wpad2_nunchuk_calibrate(struct nunchuk_t *nc,
                              const WpadDeviceExpCalibrationData *cd)
{
	const uint8_t *data = cd->data;

	memset(nc, 0, sizeof(*nc));
	nc->accel_calib.cal_zero.x = (data[0] << 2) | ((data[3] >> 4) & 3);
	nc->accel_calib.cal_zero.y = (data[1] << 2) | ((data[3] >> 2) & 3);
	nc->accel_calib.cal_zero.z = (data[2] << 2) | (data[3] & 3);

	nc->accel_calib.cal_g.x = (((data[4] << 2) | ((data[7] >> 4) & 3)) - nc->accel_calib.cal_zero.x);
	nc->accel_calib.cal_g.y = (((data[5] << 2) | ((data[7] >> 2) & 3)) - nc->accel_calib.cal_zero.y);
	nc->accel_calib.cal_g.z = (((data[6] << 2) | (data[7] & 3)) - nc->accel_calib.cal_zero.z);

	nc->js.max.x = data[8];
	nc->js.min.x = data[9];
	nc->js.center.x = data[10];
	nc->js.max.y = data[11];
	nc->js.min.y = data[12];
	nc->js.center.y = data[13];

	/* set to defaults (averages from 5 nunchuks) if calibration data is invalid */
	if (nc->accel_calib.cal_zero.x == 0)
		nc->accel_calib.cal_zero.x = 499;
	if (nc->accel_calib.cal_zero.y == 0)
		nc->accel_calib.cal_zero.y = 509;
	if (nc->accel_calib.cal_zero.z == 0)
		nc->accel_calib.cal_zero.z = 507;
	if (nc->accel_calib.cal_g.x == 0)
		nc->accel_calib.cal_g.x = 703;
	if (nc->accel_calib.cal_g.y == 0)
		nc->accel_calib.cal_g.y = 709;
	if (nc->accel_calib.cal_g.z == 0)
		nc->accel_calib.cal_g.z = 709;
	if (nc->js.max.x == 0)
		nc->js.max.x = 223;
	if (nc->js.min.x == 0)
		nc->js.min.x = 27;
	if (nc->js.center.x == 0)
		nc->js.center.x = 126;
	if (nc->js.max.y == 0)
		nc->js.max.y = 222;
	if (nc->js.min.y == 0)
		nc->js.min.y = 30;
	if (nc->js.center.y == 0)
		nc->js.center.y = 131;

	nc->accel_calib.st_alpha = WPAD2_DEFAULT_SMOOTH_ALPHA;
}

/**
 *	@brief Handle nunchuk event.
 *
 *	@param nc		A pointer to a nunchuk_t structure.
 *	@param msg		The message specified in the event packet.
 */
void _wpad2_nunchuk_event(struct nunchuk_t *nc, const uint8_t *msg) {
	/*int i; */

	/* decrypt data */
	/*
	for (i = 0; i < 6; ++i)
		msg[i] = (msg[i] ^ 0x17) + 0x17;
	*/
	/* get button states */
	nunchuk_pressed_buttons(nc, msg[5]);

	nc->js.pos.x = msg[0];
	nc->js.pos.y = msg[1];

	/* extend min and max values to physical range of motion */
	if (nc->js.center.x) {
		if (nc->js.min.x > nc->js.pos.x) nc->js.min.x = nc->js.pos.x;
		if (nc->js.max.x < nc->js.pos.x) nc->js.max.x = nc->js.pos.x;
	}
	if (nc->js.center.y) {
		if (nc->js.min.y > nc->js.pos.y) nc->js.min.y = nc->js.pos.y;
		if (nc->js.max.y < nc->js.pos.y) nc->js.max.y = nc->js.pos.y;
	}

	/* calculate orientation */
	nc->accel.x = (msg[2] << 2) + ((msg[5] >> 2) & 3);
	nc->accel.y = (msg[3] << 2) + ((msg[5] >> 4) & 3);
	nc->accel.z = (msg[4] << 2) + ((msg[5] >> 6) & 3);
}
