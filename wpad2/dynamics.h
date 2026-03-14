#ifndef WPAD2_DYNAMICS_H
#define WPAD2_DYNAMICS_H

#include "internals.h"

void _wpad2_calc_data(WPADData *data, const WPADData *lstate,
                      struct accel_t *accel_calib, bool smoothed);

#endif /* WPAD2_DYNAMICS_H */
