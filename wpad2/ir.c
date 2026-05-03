#include "ir.h"

#include "ogc/irq.h"

#include <stdio.h>
#include <math.h>
#include <time.h>

static vu32* const _ipcReg = (u32*)0xCD000000;

static inline u32 ACR_ReadReg(u32 reg)
{
	return _ipcReg[reg >> 2];
}

static inline void ACR_WriteReg(u32 reg, u32 val)
{
	_ipcReg[reg >> 2] = val;
}

/**
 *	@brief Correct for the IR bounding box.
 *
 *	@param x		[out] The current X, it will be updated if valid.
 *	@param y		[out] The current Y, it will be updated if valid.
 *	@param aspect	Aspect ratio of the screen.
 *	@param offset_x	The X offset of the bounding box.
 *	@param offset_y	The Y offset of the bounding box.
 *
 *	@return Returns 1 if the point is valid and was updated.
 *
 *	Nintendo was smart with this bit. They sacrifice a little
 *	precision for a big increase in usability.
 */
static int ir_correct_for_bounds(float *x, float *y, enum aspect_t aspect, int offset_x, int offset_y) {
	float x0, y0;
	int xs, ys;

	if (aspect == WIIUSE_ASPECT_16_9) {
		xs = WM_ASPECT_16_9_X;
		ys = WM_ASPECT_16_9_Y;
	} else {
		xs = WM_ASPECT_4_3_X;
		ys = WM_ASPECT_4_3_Y;
	}

	x0 = ((1024 - xs) / 2) + offset_x;
	y0 = ((768 - ys) / 2) + offset_y;

	if ((*x >= x0)
	    && (*x <= (x0 + xs))
	    && (*y >= y0)
	    && (*y <= (y0 + ys))) {
		*x -= offset_x;
		*y -= offset_y;

		return 1;
	}

	return 0;
}

/**
 *	@brief Interpolate the point to the user defined virtual screen resolution.
 */
static void ir_convert_to_vres(float *x, float *y, enum aspect_t aspect, unsigned int vx, unsigned int vy) {
	int xs, ys;

	if (aspect == WIIUSE_ASPECT_16_9) {
		xs = WM_ASPECT_16_9_X;
		ys = WM_ASPECT_16_9_Y;
	} else {
		xs = WM_ASPECT_4_3_X;
		ys = WM_ASPECT_4_3_Y;
	}

	*x -= ((1024 - xs) / 2);
	*y -= ((768 - ys) / 2);

	*x = (*x / (float)xs) * vx;
	*y = (*y / (float)ys) * vy;
}

/**
 *	@brief	Get the IR sensitivity settings.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param block1	[out] Pointer to where block1 will be set.
 *	@param block2	[out] Pointer to where block2 will be set.
 *
 *	@return Returns the sensitivity level.
 */
static int get_ir_sens(uint8_t sensor_level,
                       const uint8_t **block1, const uint8_t **block2) {
	switch (sensor_level) {
	case 1:
		*block1 = WM_IR_BLOCK1_LEVEL1;
		*block2 = WM_IR_BLOCK2_LEVEL1;
		return 1;
	case 2:
		*block1 = WM_IR_BLOCK1_LEVEL2;
		*block2 = WM_IR_BLOCK2_LEVEL2;
		return 2;
	case 3:
		*block1 = WM_IR_BLOCK1_LEVEL3;
		*block2 = WM_IR_BLOCK2_LEVEL3;
		return 3;
	case 4:
		*block1 = WM_IR_BLOCK1_LEVEL4;
		*block2 = WM_IR_BLOCK2_LEVEL4;
		return 4;
	case 5:
		*block1 = WM_IR_BLOCK1_LEVEL5;
		*block2 = WM_IR_BLOCK2_LEVEL5;
		return 5;
	}

	*block1 = NULL;
	*block2 = NULL;
	return 0;
}

static void rotate_dots(struct fdot_t *in, struct fdot_t *out, int count, float ang) {
	float s, c;
	int i;

	if (ang == 0) {
		for (i = 0; i < count; ++i) {
			out[i].x = in[i].x;
			out[i].y = in[i].y;
		}
		return;
	}

	s = sin(DEGREE_TO_RAD(ang));
	c = cos(DEGREE_TO_RAD(ang));

	/*
	 *	[ cos(theta)  -sin(theta) ][ ir->rx ]
	 *	[ sin(theta)  cos(theta)  ][ ir->ry ]
	 */

	for (i = 0; i < count; ++i) {
		out[i].x = (c * in[i].x) + (-s * in[i].y);
		out[i].y = (s * in[i].x) + (c * in[i].y);
	}
}

static bool ir_set_mode(WpadDevice *device)
{
	uint8_t buf = device->exp_attached ? WM_IR_TYPE_BASIC : WM_IR_TYPE_EXTENDED;
	if (!_wpad2_device_write_data(device, WM_REG_IR_MODENUM, &buf, 1)) return false;
	device->state = STATE_IR_SET_MODE;
	return true;
}

static bool ir_enable(WpadDevice *device, bool enable)
{
	if (_wpad2_device_get_slot(device) == WPAD_BALANCE_BOARD) return false;

	/*
	 *  Check to make sure a sensitivity setting is selected.
	 */
	const uint8_t *block1, *block2;
	int ir_level = get_ir_sens(device->ir_sensor_level, &block1, &block2);
	if (!ir_level) {
		WPAD2_ERROR("No IR sensitivity setting selected.");
		return false;
	}

	if (enable == device->ir_enabled ||
	    (device->exp_type == EXP_CLASSIC &&
	     device->exp_subtype == CLASSIC_TYPE_WIIU)) {
		return false;
	}

	uint8_t buf = (enable ? 0x04 : 0x00);
	device->state = STATE_IR_ENABLING_1;
	_wpad2_device_send_command(device, WM_CMD_IR, &buf, 1);

	WPAD2_DEBUG("IR cameras for wiimote %i was set to %d", device->unid, enable);
	return true;
}

static bool ir_set_sensitivity(WpadDevice *device)
{
	const uint8_t *block1, *block2;
	int ir_level = get_ir_sens(device->ir_sensor_level, &block1, &block2);
	if (!ir_level) {
		return false;
	}

	if (device->state == STATE_IR_ENABLING_2) {
		uint8_t buf = 0x08;
		if (!_wpad2_device_write_data(device, WM_REG_IR, &buf, 1)) return false;
		device->state = STATE_IR_SENSITIVITY_1;
	} else if (device->state == STATE_IR_SENSITIVITY_1) {
		if (!_wpad2_device_write_data(device, WM_REG_IR_BLOCK1, block1, 9)) return false;
		device->state = STATE_IR_SENSITIVITY_2;
	} else if (device->state == STATE_IR_SENSITIVITY_2) {
		if (!_wpad2_device_write_data(device, WM_REG_IR_BLOCK2, block2, 2)) return false;
		device->state = STATE_IR_SENSITIVITY_3;
	}

	return true;
}

/**
 *	@brief	Set the virtual screen resolution for IR tracking.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param status	1 to enable, 0 to disable.
 */
void _wpad2_ir_set_vres(WPADData *data, unsigned int x, unsigned int y) {
	data->ir.vres[0] = x - 1;
	data->ir.vres[1] = y - 1;
}

/**
 *	@brief	Set the XY position for the IR cursor.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
void _wpad2_ir_set_position(WPADData *data, enum ir_position_t pos) {
	data->ir.pos = pos;

	switch (pos) {

	case WIIUSE_IR_ABOVE:
		data->ir.offset[0] = 0;

		if (data->ir.aspect == WIIUSE_ASPECT_16_9)
			data->ir.offset[1] = WM_ASPECT_16_9_Y / 2 - 70;
		else if (data->ir.aspect == WIIUSE_ASPECT_4_3)
			data->ir.offset[1] = WM_ASPECT_4_3_Y / 2 - 100;

		return;

	case WIIUSE_IR_BELOW:
		data->ir.offset[0] = 0;

		if (data->ir.aspect == WIIUSE_ASPECT_16_9)
			data->ir.offset[1] = -WM_ASPECT_16_9_Y / 2 + 70;
		else if (data->ir.aspect == WIIUSE_ASPECT_4_3)
			data->ir.offset[1] = -WM_ASPECT_4_3_Y / 2 + 100;

		return;

	default:
		return;
	};
}

/**
 *	@brief	Set the aspect ratio of the TV/monitor.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param aspect	Either WIIUSE_ASPECT_16_9 or WIIUSE_ASPECT_4_3
 */
void _wpad2_ir_set_aspect_ratio(WPADData *data, enum aspect_t aspect) {
	data->ir.aspect = aspect;

	if (aspect == WIIUSE_ASPECT_4_3) {
		data->ir.vres[0] = WM_ASPECT_4_3_X;
		data->ir.vres[1] = WM_ASPECT_4_3_Y;
	} else {
		data->ir.vres[0] = WM_ASPECT_16_9_X;
		data->ir.vres[1] = WM_ASPECT_16_9_Y;
	}

	/* reset the position offsets */
	_wpad2_ir_set_position(data, data->ir.pos);
}

/**
 *	@brief Calculate the data from the IR spots.  Basic IR mode.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param data		Data returned by the wiimote for the IR spots.
 */
void _wpad2_ir_parse_basic(WPADData *out, const uint8_t *data) {
	struct ir_dot_t *dot = out->ir.dot;
	int i;

	dot[0].rx = 1023 - (data[0] | ((data[2] & 0x30) << 4));
	dot[0].ry = data[1] | ((data[2] & 0xC0) << 2);

	dot[1].rx = 1023 - (data[3] | ((data[2] & 0x03) << 8));
	dot[1].ry = data[4] | ((data[2] & 0x0C) << 6);

	dot[2].rx = 1023 - (data[5] | ((data[7] & 0x30) << 4));
	dot[2].ry = data[6] | ((data[7] & 0xC0) << 2);

	dot[3].rx = 1023 - (data[8] | ((data[7] & 0x03) << 8));
	dot[3].ry = data[9] | ((data[7] & 0x0C) << 6);

	/* set each IR spot to visible if spot is in range */
	for (i = 0; i < 4; ++i) {
		dot[i].rx = be16toh(dot[i].rx);
		dot[i].ry = be16toh(dot[i].ry);

		if (dot[i].ry == 1023)
			dot[i].visible = 0;
		else {
			dot[i].visible = 1;
			dot[i].size = 0;                /* since we don't know the size, set it as 0 */
		}
	}
	out->data_present |= WPAD_DATA_IR;
}

/**
 *	@brief Calculate the data from the IR spots.  Extended IR mode.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param data		Data returned by the wiimote for the IR spots.
 */
void _wpad2_ir_parse_extended(WPADData *out, const uint8_t *data) {
	struct ir_dot_t *dot = out->ir.dot;
	int i;

	for (i = 0; i < 4; ++i) {
		dot[i].rx = 1023 - (data[3 * i] | ((data[(3 * i) + 2] & 0x30) << 4));
		dot[i].ry = data[(3 * i) + 1] | ((data[(3 * i) + 2] & 0xC0) << 2);

		dot[i].size = data[(3 * i) + 2];

		dot[i].rx = be16toh(dot[i].rx);
		dot[i].ry = be16toh(dot[i].ry);

		dot[i].size = dot[i].size & 0x0f;

		/* if in range set to visible */
		if (dot[i].ry == 1023)
			dot[i].visible = 0;
		else
			dot[i].visible = 1;
	}
	out->data_present |= WPAD_DATA_IR;
}

enum {
	IR_STATE_DEAD = 0,
	IR_STATE_GOOD,
	IR_STATE_SINGLE,
	IR_STATE_LOST,
};

/* half-height of the IR sensor if half-width is 1 */
#define HEIGHT (384.0f / 512.0f)
/* maximum sensor bar slope (tan(35 degrees)) */
#define MAX_SB_SLOPE 0.7f
/* minimum sensor bar width in view, relative to half of the IR sensor area */
#define MIN_SB_WIDTH 0.1f
/* reject "sensor bars" that happen to have a dot towards the middle */
#define SB_MIDDOT_REJECT 0.05f

/* physical dimensions */
/* cm center to center of emitters */
#define SB_WIDTH        19.5f
/* half-width in cm of emitters */
#define SB_DOT_WIDTH 2.25f
/* half-height in cm of emitters (with some tolerance) */
#define SB_DOT_HEIGHT 1.0f

#define SB_DOT_WIDTH_RATIO (SB_DOT_WIDTH / SB_WIDTH)
#define SB_DOT_HEIGHT_RATIO (SB_DOT_HEIGHT / SB_WIDTH)

/* dots further out than these coords are allowed to not be picked up */
/* otherwise assume something's wrong */
/*#define SB_OFF_SCREEN_X 0.8f */
/*#define SB_OFF_SCREEN_Y (0.8f * HEIGHT) */

/* disable, may be doing more harm than good due to sensor pickup glitches */
#define SB_OFF_SCREEN_X 0.0f
#define SB_OFF_SCREEN_Y 0.0f

/* if a point is closer than this to one of the previous SB points */
/* when it reappears, consider it the same instead of trying to guess */
/* which one of the two it is */
#define SB_SINGLE_NOGUESS_DISTANCE (100.0 * 100.0)

/* width of the sensor bar in pixels at one meter from the Wiimote */
#define SB_Z_COEFFICIENT 256.0f

/* distance in meters from the center of the FOV to the left or right edge, */
/* when the wiimote is at one meter */
#define WIIMOTE_FOV_COEFFICIENT 0.39f

#define SQUARED(x) ((x)*(x))
#define WMAX(x, y) ((x>y)?(x):(y))
#define WMIN(x, y) ((x<y)?(x):(y))

/**
 *	@brief Interpret IR data into more user friendly variables.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
static void find_sensorbar(struct ir_t *ir, struct orient_t *orient) {
	struct fdot_t dots[4];
	struct fdot_t acc_dots[4];
	struct sb_t cand;
	struct sb_t candidates[6];
	struct sb_t sb;

	fdot_t difference;

	int num_candidates = 0;

	int i;
	int j;
	int first, second;

	WPAD2_DEBUG("IR: orient angle: %.02f", orient->roll);

	/* count visible dots and populate dots structure */
	/* dots[] is in -1..1 units for width */
	ir->num_dots = 0;
	for (i = 0; i < 4; i++) {
		if (ir->dot[i].visible) {
			dots[ir->num_dots].x = (ir->dot[i].rx - 512.0f) / 512.0f;
			dots[ir->num_dots].y = (ir->dot[i].ry - 384.0f) / 512.0f;
			WPAD2_DEBUG("IR: dot %d at (%d,%d) (%.03f,%.03f)", ir->num_dots, ir->dot[i].rx, ir->dot[i].ry, dots[ir->num_dots].x, dots[ir->num_dots].y);
			ir->num_dots++;
		}
	}

	WPAD2_DEBUG("IR: found %d dots", ir->num_dots);

	/* nothing to track */
	if (ir->num_dots == 0) {
		if (ir->state != IR_STATE_DEAD)
			ir->state = IR_STATE_LOST;
		ir->ax = 0;
		ir->ay = 0;
		ir->distance = 0.0f;
		ir->raw_valid = 0;
		return;
	}

	/* ==== Find the Sensor Bar ==== */

	/* first rotate according to accelerometer orientation */
	rotate_dots(dots, acc_dots, ir->num_dots, orient->roll);
	if (ir->num_dots > 1) {
		WPAD2_DEBUG("IR: locating sensor bar candidates");

		/* iterate through all dot pairs */
		for (first = 0; first < (ir->num_dots - 1); first++) {
			for (second = (first + 1); second < ir->num_dots; second++) {
				WPAD2_DEBUG("IR: trying dots %d and %d", first, second);
				/* order the dots leftmost first into cand */
				/* storing both the raw dots and the accel-rotated dots */
				if (acc_dots[first].x > acc_dots[second].x) {
					cand.dots[0] = dots[second];
					cand.dots[1] = dots[first];
					cand.acc_dots[0] = acc_dots[second];
					cand.acc_dots[1] = acc_dots[first];
				} else {
					cand.dots[0] = dots[first];
					cand.dots[1] = dots[second];
					cand.acc_dots[0] = acc_dots[first];
					cand.acc_dots[1] = acc_dots[second];
				}
				difference.x = cand.acc_dots[1].x - cand.acc_dots[0].x;
				difference.y = cand.acc_dots[1].y - cand.acc_dots[0].y;

				/* check angle */
				if (fabsf(difference.y / difference.x) > MAX_SB_SLOPE)
					continue;
				WPAD2_DEBUG("IR: passed angle check");
				/* rotate to the true sensor bar angle */
				cand.off_angle = -RAD_TO_DEGREE(atan2(difference.y, difference.x));
				cand.angle = cand.off_angle + orient->roll;
				rotate_dots(cand.dots, cand.rot_dots, 2, cand.angle);
				WPAD2_DEBUG("IR: off_angle: %.02f, angle: %.02f", cand.off_angle, cand.angle);
				/* recalculate x distance - y should be zero now, so ignore it */
				difference.x = cand.rot_dots[1].x - cand.rot_dots[0].x;

				/* check distance */
				if (difference.x < MIN_SB_WIDTH)
					continue;
				/* middle dot check. If there's another source somewhere in the */
				/* middle of this candidate, then this can't be a sensor bar */

				for (i = 0; i < ir->num_dots; i++) {
					float wadj, hadj;
					struct fdot_t tdot;
					if (i == first || i == second) continue;
					hadj = SB_DOT_HEIGHT_RATIO * difference.x;
					wadj = SB_DOT_WIDTH_RATIO * difference.x;
					rotate_dots(&dots[i], &tdot, 1, cand.angle);
					if (((cand.rot_dots[0].x + wadj) < tdot.x) &&
					    ((cand.rot_dots[1].x - wadj) > tdot.x) &&
					    ((cand.rot_dots[0].y + hadj) > tdot.y) &&
					    ((cand.rot_dots[0].y - hadj) < tdot.y))
						break;
				}
				/* failed middle dot check */
				if (i < ir->num_dots) continue;
				WPAD2_DEBUG("IR: passed middle dot check");

				cand.score = 1 / (cand.rot_dots[1].x - cand.rot_dots[0].x);

				/* we have a candidate, store it */
				WPAD2_DEBUG("IR: new candidate %d", num_candidates);
				candidates[num_candidates++] = cand;
			}
		}
	}

	if (num_candidates == 0) {
		int closest = -1;
		int closest_to = 0;
		float best = 999.0f;
		float d;
		float dx[2];
		struct sb_t sbx[2];
		/* no sensor bar candidates, try to work with a lone dot */
		WPAD2_DEBUG("IR: no candidates");
		switch (ir->state) {
		case IR_STATE_DEAD:
			WPAD2_DEBUG("IR: we're dead");
			/* we've never seen a sensor bar before, so we're screwed */
			ir->ax = 0.0f;
			ir->ay = 0.0f;
			ir->distance = 0.0f;
			ir->raw_valid = 0;
			return;
		case IR_STATE_GOOD:
		case IR_STATE_SINGLE:
		case IR_STATE_LOST:
			WPAD2_DEBUG("IR: trying to keep track of single dot");
			/* try to find the dot closest to the previous sensor bar position */
			for (i = 0; i < ir->num_dots; i++) {
				WPAD2_DEBUG("IR: checking dot %d (%.02f, %.02f)", i, acc_dots[i].x, acc_dots[i].y);
				for (j = 0; j < 2; j++) {
					WPAD2_DEBUG("      to dot %d (%.02f, %.02f)", j, ir->sensorbar.acc_dots[j].x, ir->sensorbar.acc_dots[j].y);
					d = SQUARED(acc_dots[i].x - ir->sensorbar.acc_dots[j].x);
					d += SQUARED(acc_dots[i].y - ir->sensorbar.acc_dots[j].y);
					if (d < best) {
						best = d;
						closest_to = j;
						closest = i;
					}
				}
			}
			WPAD2_DEBUG("IR: closest dot is %d to %d", closest, closest_to);
			if (ir->state != IR_STATE_LOST || best < SB_SINGLE_NOGUESS_DISTANCE) {
				/* now work out where the other dot would be, in the acc frame */
				sb.acc_dots[closest_to] = acc_dots[closest];
				sb.acc_dots[closest_to ^ 1].x = ir->sensorbar.acc_dots[closest_to ^ 1].x - ir->sensorbar.acc_dots[closest_to].x + acc_dots[closest].x;
				sb.acc_dots[closest_to ^ 1].y = ir->sensorbar.acc_dots[closest_to ^ 1].y - ir->sensorbar.acc_dots[closest_to].y + acc_dots[closest].y;
				/* get the raw frame */
				rotate_dots(sb.acc_dots, sb.dots, 2, -orient->roll);
				if ((fabsf(sb.dots[closest_to ^ 1].x) < SB_OFF_SCREEN_X) && (fabsf(sb.dots[closest_to ^ 1].y) < SB_OFF_SCREEN_Y)) {
					/* this dot should be visible but isn't, since the candidate section failed. */
					/* fall through and try to pick out the sensor bar without previous information */
					WPAD2_DEBUG("IR: dot falls on screen, falling through");
				} else {
					/* calculate the rotated dots frame */
					/* angle tends to drift, so recalculate */
					sb.off_angle = -RAD_TO_DEGREE(atan2(sb.acc_dots[1].y - sb.acc_dots[0].y, sb.acc_dots[1].x - sb.acc_dots[0].x));
					sb.angle = ir->sensorbar.off_angle + orient->roll;
					rotate_dots(sb.acc_dots, sb.rot_dots, 2, ir->sensorbar.off_angle);
					WPAD2_DEBUG("IR: kept track of single dot\n");
					break;
				}
			} else {
				WPAD2_DEBUG("IR: lost the dot and new one is too far away");
			}
			/* try to find the dot closest to the sensor edge */
			WPAD2_DEBUG("IR: trying to find best dot");
			for (i = 0; i < ir->num_dots; i++) {
				d = WMIN(1.0f - fabsf(dots[i].x), HEIGHT - fabsf(dots[i].y));
				if (d < best) {
					best = d;
					closest = i;
				}
			}
			WPAD2_DEBUG("IR: best dot: %d", closest);
			/* now try it as both places in the sensor bar */
			/* and pick the one that places the other dot furthest off-screen */
			for (i = 0; i < 2; i++) {
				sbx[i].acc_dots[i] = acc_dots[closest];
				sbx[i].acc_dots[i ^ 1].x = ir->sensorbar.acc_dots[i ^ 1].x - ir->sensorbar.acc_dots[i].x + acc_dots[closest].x;
				sbx[i].acc_dots[i ^ 1].y = ir->sensorbar.acc_dots[i ^ 1].y - ir->sensorbar.acc_dots[i].y + acc_dots[closest].y;
				rotate_dots(sbx[i].acc_dots, sbx[i].dots, 2, -orient->roll);
				dx[i] = WMAX(fabsf(sbx[i].dots[i ^ 1].x), fabsf(sbx[i].dots[i ^ 1].y / HEIGHT));
			}
			if (dx[0] > dx[1]) {
				WPAD2_DEBUG("IR: dot is LEFT: %.02f > %.02f", dx[0], dx[1]);
				sb = sbx[0];
			} else {
				WPAD2_DEBUG("IR: dot is RIGHT: %.02f < %.02f", dx[0], dx[1]);
				sb = sbx[1];
			}
			/* angle tends to drift, so recalculate */
			sb.off_angle = -RAD_TO_DEGREE(atan2(sb.acc_dots[1].y - sb.acc_dots[0].y, sb.acc_dots[1].x - sb.acc_dots[0].x));
			sb.angle = ir->sensorbar.off_angle + orient->roll;
			rotate_dots(sb.acc_dots, sb.rot_dots, 2, ir->sensorbar.off_angle);
			WPAD2_DEBUG("IR: found new dot to track");
			break;
		}
		sb.score = 0;
		ir->state = IR_STATE_SINGLE;
	} else {
		int bestidx = 0;
		float best = 0.0f;
		WPAD2_DEBUG("IR: finding best candidate");
		/* look for the best candidate */
		/* for now, the formula is simple: pick the one with the smallest distance */
		for (i = 0; i < num_candidates; i++) {
			if (candidates[i].score > best) {
				bestidx = i;
				best = candidates[i].score;
			}
		}
		WPAD2_DEBUG("IR: best candidate: %d", bestidx);
		sb = candidates[bestidx];
		ir->state = IR_STATE_GOOD;
	}

	ir->raw_valid = 1;
	ir->ax = ((sb.rot_dots[0].x + sb.rot_dots[1].x) / 2) * 512.0 + 512.0;
	ir->ay = ((sb.rot_dots[0].y + sb.rot_dots[1].y) / 2) * 512.0 + 384.0;
	ir->sensorbar = sb;
	ir->distance = (sb.rot_dots[1].x - sb.rot_dots[0].x) * 512.0;

}

#define SMOOTH_IR_RADIUS 8.0f
#define SMOOTH_IR_SPEED 0.25f
#define SMOOTH_IR_DEADZONE 2.5f

/**
 *	@brief Smooth the IR pointer position
 *
 *	@param ir		Pointer to an ir_t structure.
 */
static void apply_ir_smoothing(struct ir_t *ir) {
	f32 dx, dy, d, theta;

	WPAD2_DEBUG("Smooth: OK (%.02f, %.02f) LAST (%.02f, %.02f) ", ir->ax, ir->ay, ir->sx, ir->sy);
	dx = ir->ax - ir->sx;
	dy = ir->ay - ir->sy;
	d = sqrtf(dx * dx + dy * dy);
	if (d > SMOOTH_IR_DEADZONE) {
		if (d < SMOOTH_IR_RADIUS) {
			WPAD2_DEBUG("INSIDE");
			ir->sx += dx * SMOOTH_IR_SPEED;
			ir->sy += dy * SMOOTH_IR_SPEED;
		} else {
			WPAD2_DEBUG("OUTSIDE");
			theta = atan2f(dy, dx);
			ir->sx = ir->ax - cosf(theta) * SMOOTH_IR_RADIUS;
			ir->sy = ir->ay - sinf(theta) * SMOOTH_IR_RADIUS;
		}
	} else {
		WPAD2_DEBUG("DEADZONE");
	}
}

/* max number of errors before cooked data drops out */
#define ERROR_MAX_COUNT 8
/* max number of glitches before cooked data updates */
#define GLITCH_MAX_COUNT 5
/* squared delta over which we consider something a glitch */
#define GLITCH_DIST (150.0f * 150.0f)

/**
 *	@brief Calculate yaw given the IR data.
 *
 *	@param ir	IR data structure.
 */
float calc_yaw(struct ir_t *ir) {
	float x;

	x = ir->ax - 512;
	x *= WIIMOTE_FOV_COEFFICIENT / 512.0;

	return RAD_TO_DEGREE(atanf(x));
}

/**
 *	@brief Interpret IR data into more user friendly variables.
 *
 *	@param ir		Pointer to an ir_t structure.
 *	@param orient	Pointer to an orient_t structure.
 */
void _wpad2_interpret_ir_data(struct ir_t *ir, struct orient_t *orient) {

	float x, y;
	float d;

	find_sensorbar(ir, orient);

	if (ir->raw_valid) {
		ir->angle = ir->sensorbar.angle;
		ir->z = SB_Z_COEFFICIENT / ir->distance;
		orient->yaw = calc_yaw(ir);
		if (ir->error_cnt >= ERROR_MAX_COUNT) {
			ir->sx = ir->ax;
			ir->sy = ir->ay;
			ir->glitch_cnt = 0;
		} else {
			d = SQUARED(ir->ax - ir->sx) + SQUARED(ir->ay - ir->sy);
			if (d > GLITCH_DIST) {
				if (ir->glitch_cnt > GLITCH_MAX_COUNT) {
					apply_ir_smoothing(ir);
					ir->glitch_cnt = 0;
				} else {
					ir->glitch_cnt++;
				}
			} else {
				ir->glitch_cnt = 0;
				apply_ir_smoothing(ir);
			}
		}
		ir->smooth_valid = 1;
		ir->error_cnt = 0;
	} else {
		if (ir->error_cnt >= ERROR_MAX_COUNT) {
			ir->smooth_valid = 0;
		} else {
			ir->smooth_valid = 1;
			ir->error_cnt++;
		}
	}
	if (ir->smooth_valid) {
		x = ir->sx;
		y = ir->sy;
		if (ir_correct_for_bounds(&x, &y, ir->aspect, ir->offset[0], ir->offset[1])) {
			ir_convert_to_vres(&x, &y, ir->aspect, ir->vres[0], ir->vres[1]);
			ir->x = x;
			ir->y = y;
			ir->valid = 1;
		} else {
			ir->valid = 0;
		}
	} else {
		ir->valid = 0;
	}
}

bool _wpad2_device_ir_step(WpadDevice *device)
{
	WPAD2_DEBUG("state = %d", device->state);
	/* My original Wiimote responds with error 0x04 if sensitivity writes are
	 * issued before the previous has been acknowledged, so we separate all the
	 * writes into different steps. */
	if (device->state == STATE_IR_IDLE) {
		if (ir_enable(device, device->ir_requested)) return true;
		device->state = STATE_READY;
	} else if (device->state == STATE_IR_ENABLING_1) {
		uint8_t buf = device->ir_requested ? 0x04 : 0x00;
		device->state = STATE_IR_ENABLING_2;
		if (_wpad2_device_send_command(device, WM_CMD_IR_2, &buf, 1)) return true;
	} else if (device->state == STATE_IR_ENABLING_2) {
		if (device->ir_requested) {
			if (ir_set_sensitivity(device)) return true;
		} else {
			device->ir_enabled = false;
			device->state = STATE_READY;
		}
	} else if (device->state >= STATE_IR_SENSITIVITY_1 &&
	           device->state < STATE_IR_SENSITIVITY_3) {
		if (ir_set_sensitivity(device)) return true;
	} else if (device->state == STATE_IR_SENSITIVITY_3) {
		if (ir_set_mode(device)) return true;
	} else if (device->state == STATE_IR_SET_MODE) {
		device->ir_enabled = true;
		device->state = STATE_READY;
	}

	return false;
}

bool _wpad2_device_ir_failed(WpadDevice *device)
{
	device->state = STATE_READY;
	return false;
}

void _wpad2_ir_sensor_bar_enable(bool enable)
{
	u32 level;

	level = IRQ_Disable();
	u32 val = (ACR_ReadReg(0xc0) & ~0x100);
	if (enable) val |= 0x100;
	ACR_WriteReg(0xc0, val);
	IRQ_Restore(level);
}
