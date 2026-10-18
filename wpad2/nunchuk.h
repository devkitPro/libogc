#ifndef WPAD2_NUNCHUK_H
#define WPAD2_NUNCHUK_H

#include "device.h"

void _wpad2_nunchuk_calibrate(struct nunchuk_t *nc,
                              const WpadDeviceExpCalibrationData *cd);
void _wpad2_nunchuk_event(struct nunchuk_t *nc, const uint8_t *msg);

#endif /* WPAD2_NUNCHUK_H */
