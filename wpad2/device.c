/*-------------------------------------------------------------

Copyright (C) 2008-2026
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)
Zarithya
Alberto Mardegan (mardy)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include "device.h"

#include "classic.h"
#include "ir.h"
#include "motion_plus.h"
#include "nunchuk.h"
#include "speaker.h"

#include "bt-embedded/services/hid.h"

#include <stdlib.h>
#include <string.h>

WpadDevice _wpad2_devices[WPAD2_MAX_DEVICES];

static Wpad2DeviceCalibrationCb s_device_calibration_cb;
static Wpad2DeviceExpCalibrationCb s_device_exp_calibration_cb;
static Wpad2DeviceExpDisconnectedCb s_device_exp_disconnected_cb;
static Wpad2DeviceReadyCb s_device_ready_cb;
static Wpad2DeviceStatusCb s_device_status_cb;
static Wpad2DeviceReportCb s_device_report_cb;
static Wpad2DeviceTimerHandlerCb s_timer_handler_cb;

static bool device_io_write(WpadDevice *device, uint8_t *data, int len)
{
	if (!device->hid_intr) return false;

	BteBufferWriter writer;
	bool ok = bte_l2cap_create_message(device->hid_intr, &writer, len + 1);
	if (!ok) return false;

	uint8_t *buf = bte_buffer_writer_ptr_n(&writer, len + 1);
	buf[0] = BTE_HID_TRANS_DATA | BTE_HID_REP_TYPE_OUTPUT;
	memcpy(buf + 1, data, len);
	int rc = bte_l2cap_send_message(device->hid_intr,
	                                bte_buffer_writer_end(&writer));
	return rc >= 0;
}

static bool device_send_command(WpadDevice *device, uint8_t *data, int len)
{
	if (device->rumble) data[1] |= 0x01;
	return device_io_write(device, data, len);
}

bool _wpad2_device_send_command(WpadDevice *device, uint8_t report_type,
                                uint8_t *data, int len)
{
	/* TODO: optimize this, avoid memcpy */
	uint8_t cmd[48];
	cmd[0] = report_type;
	memcpy(cmd + 1, data, len);
	if (report_type != WM_CMD_READ_DATA &&
	    report_type != WM_CMD_CTRL_STATUS &&
	    report_type != WM_CMD_REPORT_TYPE) {
		cmd[1] |= 0x02; /* ACK requested */
	}
	WPAD2_DEBUG("Pushing command: %02x %02x", cmd[0], cmd[1]);
	return device_send_command(device, cmd, len + 1);
}

bool _wpad2_device_request_status(WpadDevice *device)
{
	uint8_t buf = 0;
	return _wpad2_device_send_command(device, WM_CMD_CTRL_STATUS, &buf, 1);
}

static void event_data_read_completed(WpadDevice *device)
{
	if (device->last_read_data) {
		free(device->last_read_data);
		device->last_read_data = NULL;
	}
	/* This marks the read as completed */
	device->last_read_size = 0;
}

static inline bool has_pending_read(WpadDevice *device)
{
	return device->last_read_size != 0;
}

bool _wpad2_device_read_data(WpadDevice *device, uint32_t offset, uint16_t size)
{
	uint8_t msg[1 + 4 + 2];

	WPAD2_DEBUG("offset %08x, size %d, pending read = %d", offset, size, has_pending_read(device));

	/* Only one read at a time! */
	if (has_pending_read(device)) return false;

	msg[0] = WM_CMD_READ_DATA;
	write_be32(offset, msg + 1);
	write_be16(size, msg + 1 + 4);
	bool ok = device_send_command(device, msg, sizeof(msg));
	if (ok) {
		device->last_read_offset = offset;
		device->last_read_size = size;
		device->last_read_cursor = 0;
		device->last_read_data = size > 16 ? malloc(size) : NULL;
	}
	return ok;
}

bool _wpad2_device_write_data(WpadDevice *device, uint32_t offset,
                              const void *data, uint8_t size)
{
	uint8_t msg[1 + 4 + 1 + 16];

	if (size > 16) return false;

	WPAD2_DEBUG("pending writes %d, offset %08x, size %d", device->num_pending_writes, offset, size);
	msg[0] = WM_CMD_WRITE_DATA;
	uint8_t *ptr = msg + 1;
	write_be32(offset, ptr);
	ptr += 4;
	ptr[0] = size;
	ptr++;
	memcpy(ptr, data, size);
	ptr += size;
	memset(ptr, 0, 16 - size);
	if (!device_send_command(device, msg, sizeof(msg))) return false;

	device->num_pending_writes++;
	return true;
}

bool _wpad2_device_stream_data(WpadDevice *device, const void *data, uint8_t size)
{
	uint8_t msg[1 + 1 + 20];

	if (size > 20) return false;

	WPAD2_DEBUG("size %d", size);
	msg[0] = WM_CMD_STREAM_DATA;
	msg[1] = size << 3;
	uint8_t *ptr = msg + 2;
	memcpy(ptr, data, size);
	ptr += size;
	memset(ptr, 0, 20 - size);
	if (!device_send_command(device, msg, sizeof(msg))) return false;

	return true;
}

static bool device_set_report_type(WpadDevice *device)
{
	uint8_t buf[2];
	buf[0] = device->continuous ? 0x04 : 0x00;

	bool motion = device->accel_requested || device->ir_requested;
	bool exp = device->exp_attached;
	bool ir = device->ir_requested;

	if (motion && ir && exp) buf[1] = WM_RPT_BTN_ACC_IR_EXP;
	else if (motion && exp)  buf[1] = WM_RPT_BTN_ACC_EXP;
	else if (motion && ir)   buf[1] = WM_RPT_BTN_ACC_IR;
	else if (ir && exp)      buf[1] = WM_RPT_BTN_IR_EXP;
	else if (ir)             buf[1] = WM_RPT_BTN_ACC_IR;
	else if (exp)            buf[1] = WM_RPT_BTN_EXP;
	else if (motion)         buf[1] = WM_RPT_BTN_ACC;
	else                     buf[1] = WM_RPT_BTN;

	if (buf[1] == device->report_type) return false;

	WPAD2_DEBUG("Setting report type: 0x%02x on device %d", buf[1], device->unid);
	device->report_type = buf[1];

	return _wpad2_device_send_command(device, WM_CMD_REPORT_TYPE, buf, 2);
}

bool _wpad2_device_set_leds(WpadDevice *device, int leds)
{
	uint8_t buf;
	leds &= 0xf0;
	buf = leds;
	return _wpad2_device_send_command(device, WM_CMD_LED, &buf, 1);
}

void _wpad2_device_expansion_ready(WpadDevice *device)
{
	WPAD2_DEBUG("wiimote %d, expansion %d", _wpad2_device_get_slot(device), device->exp_type);
	device->state = STATE_READY;
	_wpad2_device_step(device);
}

static bool device_expansion_failed(WpadDevice *device)
{
	WPAD2_WARNING("Expansion handshake failed for wiimote %d",
	              _wpad2_device_get_slot(device));
	device->exp_type = EXP_FAILED;
	return false;
}

static bool device_expansion_disable(WpadDevice *device)
{
	if (device->exp_type == EXP_NONE) return false;

	device->state = STATE_READY;
	device->exp_type = EXP_NONE;

	s_device_exp_disconnected_cb(device);
	return false;
}

/* Returns true if a step was performed */
static bool device_expansion_step(WpadDevice *device)
{
	uint8_t val;

	WPAD2_DEBUG("state: %d", device->state);
	if (!device->exp_attached) {
		return device_expansion_disable(device);
	}

	switch (device->state) {
	case STATE_EXP_ATTACHED:
		device->state = STATE_EXP_ENABLE_1;
		val = 0x55;
		_wpad2_device_write_data(device, WM_EXP_MEM_ENABLE1, &val, 1);
		break;
	case STATE_EXP_ENABLE_1:
		device->state = STATE_EXP_ENABLE_2;
		val = 0x0;
		_wpad2_device_write_data(device, WM_EXP_MEM_ENABLE2, &val, 1);
		break;
	case STATE_EXP_ENABLE_2:
		if (_wpad2_device_read_data(device, WM_EXP_ID, 6)) {
			device->state = STATE_EXP_IDENTIFICATION;
		}
		break;
	case STATE_EXP_IDENTIFICATION:
		if (_wpad2_device_read_data(device, WM_EXP_MEM_CALIBR,
		                            EXP_CALIBRATION_DATA_LEN * 2)) {
			device->state = STATE_EXP_READ_CALIBRATION;
		}
		break;
	case STATE_EXP_READ_CALIBRATION:
		_wpad2_device_expansion_ready(device);
		break;
	default:
		return false;
	}
	return true;
}

static bool device_expansion_calibrate(
	WpadDevice *device, const uint8_t *data, uint16_t len)
{
	if (len < EXP_CALIBRATION_DATA_LEN * 2) {
		return device_expansion_failed(device);
	}

	if (data[0] == 0xff) {
		/* This calibration data is invalid, let's try the backup copy */
		data += EXP_CALIBRATION_DATA_LEN;
	}

	if (data[0] == 0xff) {
		return device_expansion_failed(device);
	}

	/* Send the calibration data to the client */
	s_device_exp_calibration_cb(device, (const WpadDeviceExpCalibrationData *)data);

	return false;
}

void _wpad2_device_set_data_format(WpadDevice *device, uint8_t format)
{
	switch (format) {
	case WPAD_FMT_BTNS:
		device->accel_requested = false;
		device->ir_requested = false;
		break;
	case WPAD_FMT_BTNS_ACC:
		device->accel_requested = true;
		device->ir_requested = false;
		break;
	case WPAD_FMT_BTNS_ACC_IR:
		device->accel_requested = true;
		device->ir_requested = true;
		break;
	default:
		return;
	}

	device->continuous = device->accel_requested;
	if (device->initialized) _wpad2_device_step(device);
}

static bool device_handshake_completed(WpadDevice *device)
{
	device->state = STATE_HANDSHAKE_COMPLETE;
	WPAD2_DEBUG("state: %d", device->state);

	int8_t chan = device->unid;
	bool active = _wpad2_device_set_leds(device, WIIMOTE_LED_1 << (chan % 4));

	device->initialized = true;
	s_device_ready_cb(device);
	return active;
}

static bool device_handshake_read_calibration(WpadDevice *device)
{
	device->state = STATE_HANDSHAKE_READ_CALIBRATION;
	return _wpad2_device_read_data(device, WM_MEM_OFFSET_CALIBRATION, 8);
}

static bool device_handshake_calibrated(WpadDevice *device,
                                        const uint8_t *data, uint16_t len)
{
	if (len != 8) return false;
	s_device_calibration_cb(device, (const WpadDeviceCalibrationData *)data);

	return device_handshake_completed(device);
}

static bool device_handshake_step(WpadDevice *device)
{
	if (device->initialized) return false;

	bool active = false;
	if (device->state == STATE_HANDSHAKE_LEDS_OFF) {
		if (device->exp_attached) {
			device->state = STATE_EXP_FIRST;
			active = device_expansion_step(device);
		}
	} else if (device->state >= STATE_EXP_FIRST &&
			   device->state <= STATE_EXP_LAST) {
		active = device_expansion_step(device);
	}

	if (active) return true;

	if (device->state != STATE_HANDSHAKE_READ_CALIBRATION) {
		if (device_handshake_read_calibration(device)) return true;
	}

	return false;
}

bool _wpad2_device_step(WpadDevice *device)
{
	if (has_pending_read(device) || device->num_pending_writes > 0) {
		WPAD2_DEBUG("state = %d, pending read %d, pending writes = %d",
		            device->state, has_pending_read(device), device->num_pending_writes);
		/* let the pending operations complete first */
		return true;
	}

	WPAD2_DEBUG("state = %d", device->state);

	if (device_handshake_step(device)) return true;

	/* First, complete any flow currently in progress */
	if (device->state >= STATE_EXP_FIRST && device->state <= STATE_EXP_LAST) {
		if (device_expansion_step(device)) return true;
	} else if (device->state >= STATE_IR_FIRST && device->state <= STATE_IR_LAST) {
		if (_wpad2_device_ir_step(device)) return true;
	} else if (device->state >= STATE_MP_FIRST && device->state <= STATE_MP_LAST) {
		if (_wpad2_device_motion_plus_step(device)) return true;
	} else if (device->state >= STATE_SPEAKER_FIRST &&
	           device->state <= STATE_SPEAKER_LAST) {
		if (_wpad2_device_speaker_step(device)) return true;
	}

	if (device->ir_enabled != device->ir_requested) {
		device->state = STATE_IR_FIRST;
		if (_wpad2_device_ir_step(device)) return true;
	}

	if (device->speaker_enabled != device->speaker_requested) {
		device->state = STATE_SPEAKER_FIRST;
		if (_wpad2_device_speaker_step(device)) return true;
	}

	bool exp_setup = device->exp_type != EXP_NONE;
	if (device->exp_attached != exp_setup) {
		device->state = STATE_EXP_FIRST;
		if (device_expansion_step(device)) return true;
	}

	if (!device->motion_plus_probed ||
	    (device->motion_plus_available &&
	     device->motion_plus_enabled != device->motion_plus_requested)) {
		device->state = STATE_MP_FIRST;
		if (_wpad2_device_motion_plus_step(device)) return true;
	}

	device->state = STATE_READY;

	return device_set_report_type(device);
}

bool _wpad2_device_step_failed(WpadDevice *device)
{
	WPAD2_DEBUG("");
	bool active = false;
	if (device->state >= STATE_EXP_FIRST &&
	    device->state <= STATE_EXP_LAST) {
		device_expansion_failed(device);
	} else if (device->state >= STATE_IR_FIRST &&
	           device->state <= STATE_IR_LAST) {
		active = _wpad2_device_ir_failed(device);
	} else if (device->state >= STATE_MP_FIRST &&
	           device->state <= STATE_MP_LAST) {
		active = _wpad2_device_motion_plus_failed(device);
	}

	if (!active) {
		/* Forget about pending writes */
		device->num_pending_writes = 0;
		active = _wpad2_device_request_status(device);
	}

	return active;
}

static void event_status(WpadDevice *device, const uint8_t *msg, uint16_t len)
{
	if (len < 6) return;

	/* TODO; should we send en event for the buttons? */

	bool critical = false;
	bool attachment = false;
	bool speaker = false;
	bool ir = false;
	if (msg[2] & WM_CTRL_STATUS_BYTE1_BATTERY_CRITICAL) critical = true;
	if (msg[2] & WM_CTRL_STATUS_BYTE1_ATTACHMENT) attachment = true;
	if (msg[2] & WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED) speaker = true;
	if (msg[2] & WM_CTRL_STATUS_BYTE1_IR_ENABLED) ir = true;

	WPAD2_DEBUG("Status event, critical = %d, exp = %d, speaker = %d, ir = %d",
	            critical, attachment, speaker, ir);
	uint8_t battery_level = msg[5];

	if (battery_level != device->battery_level ||
	    critical != device->battery_critical ||
		speaker != device->speaker_enabled) {
		device->battery_level = battery_level;
		device->battery_critical = critical;
		device->speaker_enabled = speaker;
		s_device_status_cb(device);
	}
	device->ir_enabled = ir;
	device->exp_attached = attachment;
	/* Reset the report mode, so that it will be recomputed */
	device->report_type = 0;

	_wpad2_device_step(device);
}

static void parse_extension_type(WpadDevice *device, const uint8_t *data)
{
	uint32_t id = read_be32(data + 2);
	switch (id) {
	case EXP_ID_CODE_NUNCHUK:
		device->exp_type = EXP_NUNCHUK;
		break;
	case EXP_ID_CODE_GUITAR:
		device->exp_type = EXP_GUITAR_HERO_3;
		break;
	case EXP_ID_CODE_WIIBOARD:
		device->exp_type = EXP_WII_BOARD;
		break;
	case EXP_ID_CODE_CLASSIC_CONTROLLER:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING2:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING3:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC2:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC3:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC4:
	case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC5:
		device->exp_type = EXP_CLASSIC;
		device->exp_subtype = data[0] == 0 ? CLASSIC_TYPE_ORIG : CLASSIC_TYPE_PRO;
		break;
	case EXP_ID_CODE_CLASSIC_WIIU_PRO:
		device->exp_type = EXP_CLASSIC;
		device->exp_subtype = CLASSIC_TYPE_WIIU;
		break;
	case EXP_ID_CODE_MOTION_PLUS:
		device->exp_type = EXP_MOTION_PLUS;
		break;
	default:
		device->exp_type = EXP_NONE;
		break;
	}
}

static void event_data_read(WpadDevice *device,
                            const uint8_t *msg, uint16_t len)
{
	if (len < 6) return;

	/* TODO; should we send an event for the buttons? */

	uint8_t err = msg[2] & 0x0f;
	WPAD2_DEBUG("len %d, err = %d, requested offset %08x req size %d",
	            len, err, device->last_read_offset, device->last_read_size);
	if (err) {
		event_data_read_completed(device);
		_wpad2_device_step_failed(device);
		return;
	}

	uint16_t size = (msg[2] >> 4) + 1;
	const uint8_t *data = msg + 5;
	if (device->last_read_data) {
		memcpy(device->last_read_data + device->last_read_cursor, data, size);
		device->last_read_cursor += size;
		if (device->last_read_cursor < device->last_read_size) {
			/* We expect more read events. We'll handle the read when all the
			 * data has arrived. */
			return;
		}
		data = device->last_read_data;
		size = device->last_read_size;
	}

	uint32_t offset = device->last_read_offset;
	bool active = false;
	if (offset == WM_EXP_MEM_CALIBR) {
		active = device_expansion_calibrate(device, data, size);
		event_data_read_completed(device);
	} else if (offset == WM_MEM_OFFSET_CALIBRATION) {
		active = device_handshake_calibrated(device, data, size);
		event_data_read_completed(device);
	} else if (offset == WM_EXP_ID) {
		parse_extension_type(device, data);
		event_data_read_completed(device);
		if (device->state < STATE_HANDSHAKE_COMPLETE) {
			/* Give precedence to complete the wiimote initialization */
			active = device_handshake_read_calibration(device);
		} else if (device->exp_type != EXP_MOTION_PLUS) {
			active = device_expansion_step(device);
		}
	} else if (offset == WM_EXP_MOTION_PLUS_MODE) {
		active = _wpad2_device_motion_plus_read_mode_cb(device, data, size);
		event_data_read_completed(device);
	} else {
		event_data_read_completed(device);
	}

	if (!active) {
		_wpad2_device_step(device);
	}
}

static void event_ack(WpadDevice *device, const uint8_t *msg, uint16_t len)
{
	if (len < 4) return;

	uint8_t error = msg[3];
	uint8_t report_type = msg[2];
	WPAD2_DEBUG("reporty type %02x, error %d", report_type, error);

	if (report_type == WM_CMD_WRITE_DATA) {
		if (device->num_pending_writes > 0)
			device->num_pending_writes--;
	} else if (report_type == WM_CMD_STREAM_DATA) {
		if (error != 0) _wpad2_speaker_play(device, NULL);
	}

	if (error == 0) {
		_wpad2_device_step(device);
	} else {
		_wpad2_device_step_failed(device);
	}
}

void _wpad2_device_event(WpadDevice *device,
                         const uint8_t *report, uint16_t len)
{
	uint8_t event = report[0];
	const uint8_t *msg = report + 1;

	switch (event) {
	case WM_RPT_CTRL_STATUS:
		event_status(device, msg, len - 1);
		break;
	case WM_RPT_READ:
		event_data_read(device, msg, len - 1);
		break;
	case WM_RPT_ACK:
		event_ack(device, msg, len - 1);
		break;
	case WM_RPT_BTN:
	case WM_RPT_BTN_ACC:
	case WM_RPT_BTN_ACC_IR:
	case WM_RPT_BTN_EXP:
	case WM_RPT_BTN_ACC_EXP:
	case WM_RPT_BTN_IR_EXP:
	case WM_RPT_BTN_ACC_IR_EXP:
		s_device_report_cb(device, report, len);
		break;
	default:
		WPAD2_DEBUG("Event: %02x, length %d", event, len);
	}
}

void _wpad2_device_set_calibration_cb(Wpad2DeviceCalibrationCb callback)
{
	s_device_calibration_cb = callback;
}

void _wpad2_device_set_exp_calibration_cb(Wpad2DeviceExpCalibrationCb callback)
{
	s_device_exp_calibration_cb = callback;
}

void _wpad2_device_set_exp_disconnected_cb(
	Wpad2DeviceExpDisconnectedCb callback)
{
	s_device_exp_disconnected_cb = callback;
}

void _wpad2_device_set_ready_cb(Wpad2DeviceReadyCb callback)
{
	s_device_ready_cb = callback;
}

void _wpad2_device_set_status_cb(Wpad2DeviceStatusCb callback)
{
	s_device_status_cb = callback;
}

void _wpad2_device_set_report_cb(Wpad2DeviceReportCb callback)
{
	s_device_report_cb = callback;
}

void _wpad2_device_set_timer_handler_cb(Wpad2DeviceTimerHandlerCb callback)
{
	s_timer_handler_cb = callback;
}

void _wpad2_device_timer_event(uint64_t now)
{
	for (int i = 0; i < WPAD2_MAX_DEVICES; i++) {
		WpadDevice *device = _wpad2_device_get(i);
		if (device->sound) {
			_wpad2_speaker_play_part(device);
		}
	}
}

void _wpad2_device_set_rumble(WpadDevice *device, bool enable)
{
	if (enable != device->rumble) {
		WPAD2_DEBUG("enable: %d", enable);
		device->rumble = enable;
		_wpad2_device_request_status(device);
	}
}

void _wpad2_device_set_speaker(WpadDevice *device, bool enable)
{
	WPAD2_DEBUG("enable: %d", enable);
	device->speaker_requested = enable;
	_wpad2_device_step(device);
}

void _wpad2_device_set_timer(WpadDevice *device, uint32_t period_us)
{
	s_timer_handler_cb(device, period_us);
}
