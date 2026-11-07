#ifndef WPAD2_DEVICE_H
#define WPAD2_DEVICE_H

#include "internals.h"

#define WPAD2_DEFAULT_SMOOTH_ALPHA 0.3f

#define EXP_FAILED 0xff

typedef struct {
	uint8_t data[8];
} WpadDeviceCalibrationData;

typedef struct {
	uint8_t data[EXP_CALIBRATION_DATA_LEN];
} WpadDeviceExpCalibrationData;

typedef struct {
	int len;
	int offset;
	uint8_t samples[];
} WpadSoundInfo;

typedef enum {
	STATE_HANDSHAKE_LEDS_OFF,
	STATE_HANDSHAKE_READ_CALIBRATION,
	STATE_HANDSHAKE_COMPLETE,

	STATE_EXP_FIRST = 10,
	STATE_EXP_ATTACHED = STATE_EXP_FIRST,
	/* These next two steps are for disabling encryption */
	STATE_EXP_ENABLE_1,
	STATE_EXP_ENABLE_2,
	STATE_EXP_IDENTIFICATION,
	STATE_EXP_READ_CALIBRATION,
	STATE_EXP_READY,
	STATE_EXP_LAST = STATE_EXP_READY,

	/* IR states */
	STATE_IR_FIRST = 20,
	STATE_IR_IDLE = STATE_IR_FIRST,
	STATE_IR_ENABLING_1,
	STATE_IR_ENABLING_2,
	STATE_IR_SENSITIVITY_1,
	STATE_IR_SENSITIVITY_2,
	STATE_IR_SENSITIVITY_3,
	STATE_IR_SET_MODE,
	STATE_IR_LAST = STATE_IR_SET_MODE,

	/* Motion Plus states: */
	STATE_MP_FIRST = 30,
	STATE_MP_PROBE = STATE_MP_FIRST,
	STATE_MP_INITIALIZING,
	STATE_MP_ENABLING,
	STATE_MP_IDENTIFICATION,
	STATE_MP_DISABLING_1,
	STATE_MP_DISABLING_2,
	STATE_MP_LAST = STATE_MP_DISABLING_2,

	STATE_SPEAKER_FIRST = 40,
	STATE_SPEAKER_ENABLING_1,
	STATE_SPEAKER_ENABLING_2,
	STATE_SPEAKER_ENABLING_3,
	STATE_SPEAKER_ENABLING_4,
	STATE_SPEAKER_ENABLING_5,
	STATE_SPEAKER_ENABLING_6,
	STATE_SPEAKER_DISABLING,
	STATE_SPEAKER_LAST = STATE_SPEAKER_DISABLING,

	STATE_READY = 50,
} WpadDeviceState;

typedef struct wpad2_device_t WpadDevice;

struct wpad2_device_t {
	BteBdAddr address;
	BteL2cap *hid_ctrl;
	BteL2cap *hid_intr;
	int unid; /* TODO: figure out if needed */
	WpadDeviceState state;
	uint8_t battery_level;
	uint8_t speaker_volume;
	bool initialized : 1;
	bool exp_attached : 1;
	bool continuous : 1;
	bool rumble : 1;
	bool battery_critical : 1;
	bool ir_enabled : 1;
	bool ir_requested : 1;
	bool accel_enabled : 1;
	bool accel_requested : 1;
	bool speaker_enabled : 1;
	bool speaker_requested : 1;
	bool motion_plus_probed : 1;
	bool motion_plus_available : 1;
	bool motion_plus_enabled : 1;
	bool motion_plus_requested : 1;
	unsigned ir_sensor_level : 3;
	uint8_t exp_type;
	uint8_t exp_subtype;
	uint8_t num_pending_writes;
	uint8_t report_type;
	uint32_t last_read_offset;
	uint16_t last_read_size;
	uint16_t last_read_cursor;
	uint8_t *last_read_data; /* NULL if data is less than 16 bytes */
	WpadSoundInfo *sound;
};

extern WpadDevice _wpad2_devices[WPAD2_MAX_DEVICES];

static inline WpadDevice *_wpad2_device_get(int slot)
{
	return &_wpad2_devices[slot];
}

static inline int _wpad2_device_get_slot(const WpadDevice *device)
{
	return device ? (device - _wpad2_devices) : -1;
}

bool _wpad2_device_send_command(WpadDevice *device, uint8_t report_type,
                                uint8_t *data, int len);
bool _wpad2_device_read_data(WpadDevice *device, uint32_t offset, uint16_t size);
bool _wpad2_device_write_data(WpadDevice *device, uint32_t offset,
                              const void *data, uint8_t size);
bool _wpad2_device_stream_data(WpadDevice *device, const void *data, uint8_t size);
bool _wpad2_device_request_status(WpadDevice *device);
bool _wpad2_device_step(WpadDevice *device);
bool _wpad2_device_step_failed(WpadDevice *device);

/* Initialization-related functions */
void _wpad2_device_expansion_ready(WpadDevice *device);

/* Event reporting */
void _wpad2_device_event(WpadDevice *device,
                         const uint8_t *report, uint16_t len);

typedef void (*Wpad2DeviceCalibrationCb)(
	WpadDevice *device, const WpadDeviceCalibrationData *data);
void _wpad2_device_set_calibration_cb(Wpad2DeviceCalibrationCb callback);

typedef void (*Wpad2DeviceExpCalibrationCb)(
	WpadDevice *device, const WpadDeviceExpCalibrationData *data);
void _wpad2_device_set_exp_calibration_cb(Wpad2DeviceExpCalibrationCb callback);

typedef void (*Wpad2DeviceExpDisconnectedCb)(WpadDevice *device);
void _wpad2_device_set_exp_disconnected_cb(
	Wpad2DeviceExpDisconnectedCb callback);

typedef void (*Wpad2DeviceReadyCb)(WpadDevice *device);
void _wpad2_device_set_ready_cb(Wpad2DeviceReadyCb callback);

typedef void (*Wpad2DeviceStatusCb)(WpadDevice *device);
void _wpad2_device_set_status_cb(Wpad2DeviceStatusCb callback);

typedef void (*Wpad2DeviceReportCb)(
	WpadDevice *device, const uint8_t *data, uint8_t len);
void _wpad2_device_set_report_cb(Wpad2DeviceReportCb callback);

typedef void (*Wpad2DeviceTimerHandlerCb)(WpadDevice *device, uint32_t period_us);
void _wpad2_device_set_timer_handler_cb(Wpad2DeviceTimerHandlerCb callback);
void _wpad2_device_timer_event(uint64_t now);

bool _wpad2_device_set_leds(WpadDevice *device, int leds);
void _wpad2_device_set_data_format(WpadDevice *device, uint8_t format);
void _wpad2_device_set_rumble(WpadDevice *device, bool enable);
void _wpad2_device_set_speaker(WpadDevice *device, bool enable);
void _wpad2_device_set_timer(WpadDevice *device, uint32_t period_us);

#endif /* WPAD2_DEVICE_H */
