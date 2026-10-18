#ifndef WPAD2_IR_H
#define WPAD2_IR_H

#include "device.h"

void _wpad2_interpret_ir_data(struct ir_t *ir, struct orient_t *orient);
void _wpad2_ir_parse_basic(WPADData *out, const uint8_t *data);
void _wpad2_ir_parse_extended(WPADData *out, const uint8_t *data);
void _wpad2_ir_set_vres(WPADData *data, unsigned int x, unsigned int y);
void _wpad2_ir_set_position(WPADData *data, enum ir_position_t pos);
void _wpad2_ir_set_aspect_ratio(WPADData *data, enum aspect_t aspect);
void _wpad2_ir_sensor_bar_enable(bool enable);

bool _wpad2_device_ir_step(WpadDevice *device);
bool _wpad2_device_ir_failed(WpadDevice *device);

#endif /* WPAD2_IR_H */
