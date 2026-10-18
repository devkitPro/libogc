/**
 *	@file
 *	@brief Motion plus extension
 */

#ifndef WPAD2_MOTION_PLUS_H
#define WPAD2_MOTION_PLUS_H

#include "device.h"

void _wpad2_motion_plus_disconnected(struct motion_plus_t *mp);
void _wpad2_motion_plus_event(struct motion_plus_t *mp, const uint8_t *msg);

bool _wpad2_device_motion_plus_step(WpadDevice *device);
bool _wpad2_device_motion_plus_failed(WpadDevice *device);
void _wpad2_device_motion_plus_enable(WpadDevice *device, bool enable);

bool _wpad2_device_motion_plus_read_mode_cb(WpadDevice *device,
                                            const uint8_t *data, uint16_t len);

#endif /* WPAD2_MOTION_PLUS_H */
