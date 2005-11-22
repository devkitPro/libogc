#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "iosupp.h"

typedef struct _console_data_s {
	unsigned char *destbuffer;
	unsigned char *font;
	int con_xres,con_yres,con_stride;
	int tgt_xres,tgt_yres,tgt_stride;
	int cursor_x,cursor_y;
	int target_x,target_y;
	int border_left,border_right,border_top,border_bottom;
	int scrolled_lines;

	unsigned int foreground,background;
} console_data_s;

extern const devoptab_t dotab_stdout;

#endif
