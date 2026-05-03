#ifndef WPAD2_WORKER_H
#define WPAD2_WORKER_H

#include "device.h"

typedef uint8_t WpadEventType;
enum {
	WPAD2_EVENT_TYPE_BT_INITIALIZED,
	WPAD2_EVENT_TYPE_CONNECTED,
	WPAD2_EVENT_TYPE_CALIBRATION,
	WPAD2_EVENT_TYPE_EXP_CONNECTED,
	WPAD2_EVENT_TYPE_READY,
	WPAD2_EVENT_TYPE_REPORT,
	WPAD2_EVENT_TYPE_STATUS,
	WPAD2_EVENT_TYPE_HOST_SYNC_BTN,
	WPAD2_EVENT_TYPE_EXP_DISCONNECTED,
	WPAD2_EVENT_TYPE_DISCONNECTED,
};

typedef struct {
	uint8_t channel;
	WpadEventType type;
	union {
		struct {
			const uint8_t *data;
			uint8_t len;
		} report;
		const WpadDeviceCalibrationData *calibration_data;
		struct {
			uint8_t exp_type;
			uint8_t exp_subtype;
			const WpadDeviceExpCalibrationData *calibration_data;
		} exp_connected;
		uint8_t disconnection_reason;
		bool host_sync_button_held;
		struct {
			uint8_t battery_level;
			bool battery_critical;
			bool speaker_enabled;
		} status;
	} u;
} WpadEvent;

typedef void (*WpadEventCb)(const WpadEvent *event);

void _wpad2_worker_start();
void _wpad2_worker_on_event(WpadEventCb callback);
void _wpad2_worker_set_search_active(bool active);
void _wpad2_worker_set_format(uint8_t channel, uint8_t format);
void _wpad2_worker_set_motion_plus(uint8_t channel, bool enable);
void _wpad2_worker_set_rumble(uint8_t channel, bool enable);
void _wpad2_worker_set_speaker(uint8_t channel, bool enable);
void _wpad2_worker_disconnect(uint8_t channel);
void _wpad2_worker_stop();
void _wpad2_worker_wipe_saved_controllers();
void _wpad2_worker_start_pairing();
void _wpad2_worker_play_sound(uint8_t channel, void *buffer, int len);

#endif /* WPAD2_WORKER_H */
