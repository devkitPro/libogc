#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "color.h"
#include "cache.h"
#include "video.h"
#include "system.h"

#include "console.h"

#define FONT_XSIZE			8
#define FONT_YSIZE			16
#define FONT_XFACTOR		1
#define FONT_YFACTOR		1
#define FONT_XGAP			2
#define FONT_YGAP			0

int con_open(struct _reent *r,const char *path,int flags,int mode);
int con_write(struct _reent *r,int fd,const char *ptr,int len);
int con_read(struct _reent *r,int fd,char *ptr,int len);
int con_close(struct _reent *r,int fd);

const devoptab_t dotab_stdout = {"stdout",con_open,con_close,con_write,con_read,NULL,NULL};

static u32 do_xfb_copy = FALSE;
static struct _console_data_s stdcon;
static struct _console_data_s *curr_con = NULL;

extern u8 console_font_8x8[];
extern u8 console_font_8x16[];

extern void *__VIDEO_GetNextFramebuffer();

static void __console_viprecb(u32 retraceCnt)
{
	u32 ycnt,xcnt;
	u8 *fb,*ptr;

	do_xfb_copy = TRUE;
		
	ptr = curr_con->destbuffer;
	fb = (u8*)MEM_K0_TO_K1(__VIDEO_GetNextFramebuffer()+(curr_con->target_y*curr_con->tgt_stride+(curr_con->target_x*4)));
	for(ycnt=0;ycnt<curr_con->con_yres;ycnt++) {
		for(xcnt=0;xcnt<curr_con->con_stride;xcnt++) fb[xcnt] = ptr[xcnt];

		fb += curr_con->tgt_stride;
		ptr += curr_con->con_stride;
	}

	do_xfb_copy = FALSE;
}

static void __console_drawc(console_data_s *con,int xpos,int ypos,int c)
{
	if(do_xfb_copy==TRUE) return;

	xpos >>= 1;
	int ax, ay;
	unsigned int *ptr = (unsigned int*)(con->destbuffer + con->con_stride * ypos + xpos * 4);
	for (ay = 0; ay < FONT_YSIZE; ay++)
	{
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
			ptr[ay * 2 * con->con_stride/4 + ax] = color;
				// line doubling
			ptr[(ay * 2 +1) * con->con_stride/4 + ax] = color;
#else
			ptr[ay * con->con_stride/4 + ax] = color;
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
			ptr[ay * con->con_stride/4 + ax] = (color[0] & 0xFFFF00FF) | (color[1] & 0x0000FF00);
		}
#endif
	}
}

void __console_init(console_data_s *con,void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;

	_CPU_ISR_Disable(level);
	
	con->destbuffer = framebuffer;
	con->con_xres = con->tgt_xres = xres;
	con->con_yres = con->tgt_yres = yres;
	con->border_left = xstart;
	con->border_top  = ystart;
	con->border_right = con->con_xres;
	con->border_bottom = con->con_yres;
	con->con_stride = con->tgt_stride = stride;
	con->target_x = con->cursor_x = xstart;
	con->target_y = con->cursor_y = ystart;
	
	con->font = console_font_8x16;
	
	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;
	
	con->scrolled_lines = 0;
	
	unsigned int c = (con->con_xres*con->con_yres)/2;
	unsigned int *p = (unsigned int*)con->destbuffer;
	while(c--)
		*p++ = con->background;

	curr_con = con;

	_CPU_ISR_Restore(level);
}

void __console_init_ex(void *conbuffer,int tgt_xstart,int tgt_ystart,int tgt_xres,int tgt_yres,int tgt_stride,int con_xres,int con_yres,int con_stride)
{
	unsigned int level;
	console_data_s *con = &stdcon;

	_CPU_ISR_Disable(level);
	
	con->destbuffer = conbuffer;
	con->target_x = tgt_xstart;
	con->target_y = tgt_ystart;
	con->tgt_xres = tgt_xres;
	con->tgt_yres = tgt_yres;
	con->con_xres = con_xres;
	con->con_yres = con_yres;
	con->tgt_stride = tgt_stride;
	con->con_stride = con_stride;
	con->cursor_x = 0;
	con->cursor_y = 0;
	con->border_left = 0;
	con->border_top  = 0;
	con->border_right = con->con_xres;
	con->border_bottom = con->con_yres;

	con->font = console_font_8x16;
	
	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;
	
	con->scrolled_lines = 0;

	unsigned int c = (con->con_xres*con->con_yres)/2;
	unsigned int *p = (unsigned int*)con->destbuffer;
	while(c--)
		*p++ = con->background;

	curr_con = con;

	VIDEO_SetPreRetraceCallback(__console_viprecb);
	
	_CPU_ISR_Restore(level);
}

void console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	__console_init(&stdcon,framebuffer,xstart,ystart,xres,yres,stride);
}

void console_setpos(int x,int y)
{
	if(curr_con) {
		curr_con->cursor_x = x;
		curr_con->cursor_y = y;
	}
}

void console_setcolor(unsigned int bgcolor,unsigned int fgcolor)
{
	if(curr_con) {
		curr_con->background = bgcolor;
		curr_con->foreground = fgcolor;
	}
}

int console_putc(console_data_s *con,int c)
{
	u32 curr_x,tabsize;

	if(!con) return -1;

	switch(c) 
	{
		case '\n':
			con->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
			con->cursor_x = con->border_left;
			break;
		case '\t':
			curr_x = con->cursor_x;
			tabsize = (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP)*4;
			if(curr_x%tabsize) con->cursor_x += (curr_x%tabsize);
			else con->cursor_x += tabsize;
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
		memcpy(con->destbuffer,
			con->destbuffer+con->con_stride*(FONT_YSIZE*FONT_YFACTOR+FONT_YGAP),
			con->con_stride*con->con_yres-FONT_YSIZE);

		unsigned int cnt = (con->con_stride * (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP))/4;
		unsigned int *ptr = (unsigned int*)(con->destbuffer + con->con_stride * (con->con_yres - FONT_YSIZE));
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

int con_close(struct _reent *r,int fd)
{
	return -1;
}
