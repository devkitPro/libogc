#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "iosupp.h"

typedef struct _console_data_s {
	unsigned char *framebuffer;
	unsigned char *font;
	int xres,yres,stride;
	int cursor_x,cursor_y;
	int border_left,border_right,border_top,border_bottom;
	int scrolled_lines;

	unsigned int foreground,background;
} console_data_s;

extern const devoptab_t dotab_stdout;

#endif
