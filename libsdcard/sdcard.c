#include <stdlib.h>
#include <stdio.h>

#include "sdcard.h"

struct sd_cmd {
	union {
		struct {
			unsigned start : 1;
			unsigned host  : 1;
			unsigned cmd   : 6;
		};
		u8 ccmd;
	} cmd;
};
