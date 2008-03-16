#ifndef __CONSOLE_H__
#define __CONSOLE_H__


#define FONT_XSIZE		8
#define FONT_YSIZE		16
#define FONT_XFACTOR	1
#define FONT_YFACTOR	1
#define FONT_XGAP			0
#define FONT_YGAP			0
#define TAB_SIZE			4

typedef struct _console_data_s {
	unsigned char *destbuffer;
	unsigned char *font;
	int con_xres,con_yres,con_stride;
	int tgt_stride;
	int cursor_x,cursor_y;
	int saved_x,saved_y;
	int target_x,target_y;
	int border_left,border_right,border_top,border_bottom;

	unsigned int foreground,background;
} console_data_s;

extern int __console_write(struct _reent *r,int fd,const char *ptr,int len);

//extern const devoptab_t dotab_stdout;

#endif
