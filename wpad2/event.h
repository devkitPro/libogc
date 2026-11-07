#ifndef WPAD2_EVENT_H
#define WPAD2_EVENT_H

#include "internals.h"

typedef struct {
	int btns;
	int ir;
	int js;
	int acc;
	int wb;
	int mp;
} WpadThresholds;

bool _wpad2_event_parse_report(const WPADData *wpad_info, const uint8_t *data,
                               const WpadThresholds *thresh, WPADData *out);

#endif /* WPAD2_EVENT_H */
