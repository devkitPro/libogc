#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "color.h"

#include "console.h"

#define FONT_XSIZE			8
#define FONT_YSIZE			16
#define FONT_XFACTOR		1
#define FONT_YFACTOR		1
#define FONT_XGAP			2
#define FONT_YGAP			0

typedef struct _console_data_s {
	unsigned char *framebuffer;
	unsigned char *font;
	int xres,yres,stride;
	int cursor_x,cursor_y;
	int border_left,border_right,border_top,border_bottom;
	int scrolled_lines;

	unsigned int foreground,background;
} console_data_s;

int con_open(struct _reent *r,const char *path,int flags,int mode);
int con_close(struct _reent *r,int fd);
int con_write(struct _reent *r,int fd,const char *ptr,int len);
int con_read(struct _reent *r,int fd,char *ptr,int len);

const devoptab_t dotab_stdout = {"stdout",con_open,con_close,con_write,con_read,NULL};

static struct _console_data_s stdcon;
static struct _console_data_s *curr_con = NULL;

extern u8 console_font_8x8[];
extern u8 console_font_8x16[];

static void __console_drawc(console_data_s *con,int xpos,int ypos,int c)
{
	xpos >>= 1;
	int ax, ay;
	unsigned int *ptr = (unsigned int*)(con->framebuffer + con->stride * ypos + xpos * 4);
	for (ay = 0; ay < FONT_YSIZE; ay++)
#if FONT_XFACTOR == 2
		for (ax = 0; ax < 8; ax++)
		{
			unsigned int color;
			if ((con->font[c * FONT_YSIZE + ay] << ax) & 0x80)
				color = con->foreground;
			else
				color = con->background;
#if FONT_YFACTOR == 2
				// pixel doubling: we write u32
			ptr[ay * 2 * con->stride/4 + ax] = color;
				// line doubling
			ptr[(ay * 2 +1) * con->stride/4 + ax] = color;
#else
			ptr[ay * con->stride/4 + ax] = color;
#endif
		}
#else
		for (ax = 0; ax < 4; ax ++)
		{
			unsigned int color[2];
			int bits = (con->font[c * FONT_YSIZE + ay] << (ax*2));
			if (bits & 0x80)
				color[0] = con->foreground;
			else
				color[0] = con->background;
			if (bits & 0x40)
				color[1] = con->foreground;
			else
				color[1] = con->background;
			ptr[ay * con->stride/4 + ax] = (color[0] & 0xFFFF00FF) | (color[1] & 0x0000FF00);
		}
#endif
}

void __console_init(console_data_s *con,void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;

	_CPU_ISR_Disable(level);
	
	con->framebuffer = framebuffer;
	con->xres = xres;
	con->yres = yres;
	con->border_left = xstart;
	con->border_top  = ystart;
	con->border_right = con->xres;
	con->border_bottom = con->yres;
	con->stride = stride;
	con->cursor_x = xstart;
	con->cursor_y = ystart;
	
	con->font = console_font_8x16;
	
	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;
	
	con->scrolled_lines = 0;
	
	unsigned int c = (con->xres*con->yres)/2;
	unsigned int *p = (unsigned int*)con->framebuffer;
	while(c--)
		*p++ = con->background;

	curr_con = con;

	_CPU_ISR_Restore(level);
}

void console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	__console_init(&stdcon,framebuffer,xstart,ystart,xres,yres,stride);
}


int console_putc(console_data_s *con,int c)
{
	if(!con) return -1;

	switch(c) 
	{
		case '\n':
			con->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
			con->cursor_x = con->border_left;
			break;
		default:
			__console_drawc(con,con->cursor_x,con->cursor_y,c);
			con->cursor_x += FONT_XSIZE*FONT_XFACTOR+FONT_XGAP;
			if((con->cursor_x+FONT_XSIZE*FONT_XFACTOR)>con->border_right) {
				con->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
				con->cursor_x = con->border_left;
			}
	}

	if((con->cursor_y+FONT_YSIZE*FONT_YFACTOR)>=con->border_bottom) {
		memcpy(con->framebuffer,
			con->framebuffer+con->stride*(FONT_YSIZE*FONT_YFACTOR+FONT_YGAP),
			con->stride*con->yres-FONT_YSIZE);

		unsigned int cnt = (con->stride * (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP))/4;
		unsigned int *ptr = (unsigned int*)(con->framebuffer + con->stride * (con->yres - FONT_YSIZE));
		while(cnt--)
			*ptr++ = con->background;
		con->cursor_y -= FONT_YSIZE * FONT_YFACTOR + FONT_YGAP;
	}
	
	return 1;
}

int con_open(struct _reent *r,const char *path,int flags,int mode)
{
	if(!curr_con) return -1;
	return 0;
}

int con_close(struct _reent *r,int fd)
{
	return 0;
}

int con_write(struct _reent *r,int fd,const char *ptr,int len)
{
	int i, count = 0;
	char *tmp = (char*)ptr;

	if(!curr_con) return -1;
	if(!tmp || len<=0) return -1;

	i = 0;
	while(*tmp!='\0' && i<len) {
		count += console_putc(curr_con,*tmp++);
		i++;
	}

	return count;
}

int con_read(struct _reent *r,int fd,char *ptr,int len)
{
	return -1;
}
