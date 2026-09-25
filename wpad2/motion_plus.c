#include "motion_plus.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

bool _wpad2_device_motion_plus_read_mode_cb(WpadDevice *device,
                                            const uint8_t *data, uint16_t len)
{
	WPAD2_DEBUG("got byte %02x", data[1]);
	device->motion_plus_probed = true;
	if (data[1] != 0x05) {
		device->motion_plus_available = false;
		device->motion_plus_enabled = false;
		return false;
	}
	uint8_t val = 0x55;
	if (!_wpad2_device_write_data(device, WM_EXP_MOTION_PLUS_ENABLE, &val, 1))
		return false;

	device->state = STATE_MP_INITIALIZING;
	return true;
}

static void probe_motion_plus(WpadDevice *device)
{
	device->state = STATE_MP_PROBE;
	_wpad2_device_read_data(device, WM_EXP_MOTION_PLUS_MODE, 2);
}

static bool motion_plus_check(WpadDevice *device)
{
	device->state = STATE_MP_IDENTIFICATION;
	return _wpad2_device_read_data(device, WM_EXP_ID, 6);
}

static bool set_clear1(WpadDevice *device)
{
	ubyte val = 0x00;
	return _wpad2_device_write_data(device, WM_EXP_MEM_ENABLE1, &val, 1);
}

bool _wpad2_device_motion_plus_step(WpadDevice *device)
{
	WPAD2_DEBUG("state = %d", device->state);

	if (!device->motion_plus_probed) {
		probe_motion_plus(device);
		return true;
	}

	bool active = false;
	if (device->state == STATE_MP_INITIALIZING) {
		device->motion_plus_available = true;
	} else if (device->state == STATE_MP_ENABLING) {
		active = motion_plus_check(device);
	} else if (device->state == STATE_MP_IDENTIFICATION) {
		device->motion_plus_enabled = true;
		/* Now that the motion plus is enabled, continue its setup as an
		 * ordinary expansion. Issue a status request, and this will report
		 * that the expansion is attached. */
		device->state = STATE_EXP_IDENTIFICATION;
		active = _wpad2_device_request_status(device);
	} else if (device->state == STATE_MP_DISABLING_1) {
		active = set_clear1(device);
	} else if (device->state == STATE_MP_DISABLING_2) {
		device->motion_plus_enabled = false;
	}

	if (active) return true;

	if (device->motion_plus_enabled == device->motion_plus_requested)
		return false;

	uint8_t val;

	if (device->motion_plus_requested) {
		val = 0x04;
		active = _wpad2_device_write_data(device, WM_EXP_MOTION_PLUS_MODE, &val, 1);
		device->state = STATE_MP_ENABLING;
	} else {
		val = 0x55;
		active = _wpad2_device_write_data(device, WM_EXP_MEM_ENABLE1, &val, 1);
		device->state = STATE_MP_DISABLING_1;
	}
	return active;
}

bool _wpad2_device_motion_plus_failed(WpadDevice *device)
{
	WPAD2_DEBUG("state = %d", device->state);

	if (device->state == STATE_MP_PROBE) {
		device->motion_plus_probed = true;
		device->motion_plus_available = false;
	}

	device->state = STATE_READY;
	return false;
}

void _wpad2_device_motion_plus_enable(WpadDevice *device, bool enable)
{
	device->motion_plus_requested = enable;
	if (device->initialized) _wpad2_device_step(device);
}

void _wpad2_motion_plus_disconnected(struct motion_plus_t *mp)
{
	WPAD2_DEBUG("Motion plus disconnected");
	memset(mp, 0, sizeof(struct motion_plus_t));
}

void _wpad2_motion_plus_event(struct motion_plus_t *mp, const uint8_t *msg)
{
	mp->rx = ((msg[5] & 0xFC) << 6) | msg[2]; /* Pitch */
	mp->ry = ((msg[4] & 0xFC) << 6) | msg[1]; /* Roll */
	mp->rz = ((msg[3] & 0xFC) << 6) | msg[0]; /* Yaw */

	mp->ext = msg[4] & 0x1;
	mp->status = (msg[3] & 0x3) | ((msg[4] & 0x2) << 1); /* roll, yaw, pitch */
}
