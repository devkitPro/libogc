#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "color.h"
#include "cache.h"
#include "video.h"
#include "system.h"

#include "console.h"
#include "consol.h"

#include <stdio.h>
#include <sys/iosupport.h>

//---------------------------------------------------------------------------------
const devoptab_t dotab_stdout = {
//---------------------------------------------------------------------------------
	"stdout",	// device name
	0,			// size of file structure
	NULL,		// device open
	NULL,		// device close
	__console_write,	// device write
	NULL,		// device read
	NULL,		// device seek
	NULL,		// device fstat
	NULL,		// device stat
	NULL,		// device link
	NULL,		// device unlink
	NULL,		// device chdir
	NULL,		// device rename
	NULL,		// device mkdir
	0,			// dirStateSize
	NULL,		// device diropen_r
	NULL,		// device dirreset_r
	NULL,		// device dirnext_r
	NULL,		// device dirclose_r
	NULL		// device statvfs_r
};

//color table
const unsigned int color_table[] =
{
  0x00800080,		// 30 normal black
  0x246A24BE,		// 31 normal red
  0x4856484B,		// 32 normal green
  0x6D416D8A,		// 33 normal yellow
  0x0DBE0D75,		// 34 normal blue
  0x32A932B4,		// 35 normal magenta
  0x56955641,		// 36 normal cyan
  0xC580C580,		// 37 normal white
  0x7B807B80,		// 30 bright black
  0x4C544CFF,		// 31 bright red
  0x95299512,		// 32 bright green
  0xE200E294,		// 33 bright yellow
  0x1CFF1C6B,		// 34 bright blue
  0x69D669ED,		// 35 bright magenta
  0xB2ABB200,		// 36 bright cyan
  0xFF80FF80,		// 37 bright white
};

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
	int ay;
	unsigned int *ptr;
	unsigned char *pbits;
	unsigned char bits;
	unsigned int color;
	unsigned int fgcolor, bgcolor;
	unsigned int nextline;

	if(do_xfb_copy==TRUE) return;

	ptr = (unsigned int*)(con->destbuffer + con->con_stride * ypos + (xpos / 2) * 4);
	pbits = &con->font[c * FONT_YSIZE];
	nextline = con->con_stride/4 - 4;
	fgcolor = con->foreground;
	bgcolor = con->background;

	for (ay = 0; ay < FONT_YSIZE; ay++)
	{
		/* hard coded loop unrolling ! */
		/* this depends on FONT_XSIZE = 8*/
#if FONT_XSIZE == 8
		bits = *pbits++;

		/* bits 1 & 2 */
		if ( bits & 0x80)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x40)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* bits 3 & 4 */
		if ( bits & 0x20)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x10)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* bits 5 & 6 */
		if ( bits & 0x08)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x04)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* bits 7 & 8 */
		if ( bits & 0x02)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x01)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* next line */
		ptr += nextline;
#else
#endif
	}
}

void __console_clear(void)
{
	console_data_s *con;
	unsigned int c;
	unsigned int *p;

	if(!curr_con) return;
	con = curr_con;

	c = (con->con_xres*con->con_yres)/2;
	p = (unsigned int*)con->destbuffer;
	while(c--)
		*p++ = con->background;
}

void __console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;
	console_data_s *con = &stdcon;

	_CPU_ISR_Disable(level);

	con->destbuffer = framebuffer;
	con->con_xres = xres;
	con->con_yres = yres;
	con->border_left = xstart;
	con->border_top  = ystart;
	con->border_right = con->con_xres;
	con->border_bottom = con->con_yres;
	con->con_stride = con->tgt_stride = stride;
	con->target_x = con->cursor_x = xstart;
	con->target_y = con->cursor_y = ystart;
	con->saved_x = con->border_left;
	con->saved_y = con->border_top;

	con->font = console_font_8x16;

	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;

	curr_con = con;

	__console_clear();

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
	//setvbuf(stdout, NULL , _IONBF, 0);

	_CPU_ISR_Restore(level);
}

void __console_init_ex(void *conbuffer,int tgt_xstart,int tgt_ystart,int tgt_stride,int con_xres,int con_yres,int con_stride)
{
	unsigned int level;
	console_data_s *con = &stdcon;

	_CPU_ISR_Disable(level);

	con->destbuffer = conbuffer;
	con->target_x = tgt_xstart;
	con->target_y = tgt_ystart;
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
	con->saved_x = con->border_left;
	con->saved_y = con->border_top;

	con->font = console_font_8x16;

	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;

	curr_con = con;

	__console_clear();

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
	//setvbuf(stdout, NULL , _IONBF, 0);

	VIDEO_SetPreRetraceCallback(__console_viprecb);

	_CPU_ISR_Restore(level);
}

int __console_parse_escsequence(char *pchr)
{
	char chr;
	console_data_s *con;
	int i;
	int parameters[3];
	int para;

	if(!curr_con) return -1;
	con = curr_con;

	/* set default value */
	para = 0;
	parameters[0] = 0;
	parameters[1] = 0;
	parameters[2] = 0;

	/* scan parameters */
	i = 0;
	chr = *pchr;
	while( (para < 3) && (chr >= '0') && (chr <= '9') )
	{
		while( (chr >= '0') && (chr <= '9') )
		{
			/* parse parameter */
			parameters[para] *= 10;
			parameters[para] += chr - '0';
			pchr++;
			i++;
			chr = *pchr;
		}
		para++;

		if( *pchr == ';' )
		{
		  /* skip parameter delimiter */
		  pchr++;
			i++;
		}
		chr = *pchr;
	}

	/* get final character */
	chr = *pchr++;
	i++;
	switch(chr)
	{
		/////////////////////////////////////////
		// Cursor directional movement
		/////////////////////////////////////////
		case 'A':
		{
			curr_con->cursor_y -= parameters[0] * (FONT_YSIZE*FONT_YFACTOR+FONT_YGAP);
			if(curr_con->cursor_y < curr_con->border_top) curr_con->cursor_y = curr_con->border_top;
			break;
		}
		case 'B':
		{
			curr_con->cursor_y += parameters[0] * (FONT_YSIZE*FONT_YFACTOR+FONT_YGAP);
			if(curr_con->cursor_y >= curr_con->border_bottom) curr_con->cursor_y = curr_con->border_bottom - (FONT_YSIZE*FONT_YFACTOR+FONT_YGAP);
			break;
		}
		case 'C':
		{
			curr_con->cursor_x += parameters[0] * (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP);
			if(curr_con->cursor_x >= curr_con->border_right) curr_con->cursor_x = curr_con->border_right - (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP);
			break;
		}
		case 'D':
		{
			curr_con->cursor_x -= parameters[0] * (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP);
			if(curr_con->cursor_x < curr_con->border_left) curr_con->cursor_x = curr_con->border_left;
			break;
		}
		/////////////////////////////////////////
		// Cursor position movement
		/////////////////////////////////////////
		case 'H':
		case 'f':
		{
			curr_con->cursor_x = curr_con->border_left + parameters[1] * (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP);
			curr_con->cursor_y = curr_con->border_top + parameters[0] * (FONT_YSIZE*FONT_YFACTOR+FONT_YGAP);
			break;
		}
		/////////////////////////////////////////
		// Screen clear
		/////////////////////////////////////////
		case 'J':
		{
			/* different erase modes not yet supported, just clear all */
			__console_clear();
			break;
		}
		/////////////////////////////////////////
		// Line clear
		/////////////////////////////////////////
		case 'K':
		{
			break;
		}
		/////////////////////////////////////////
		// Save cursor position
		/////////////////////////////////////////
		case 's':
		{
			con->saved_x = con->cursor_x;
			con->saved_y = con->cursor_y;
			break;
		}
		/////////////////////////////////////////
		// Load cursor position
		/////////////////////////////////////////
		case 'u':
			con->cursor_x = con->saved_x;
			con->cursor_y = con->saved_y;
			break;
		/////////////////////////////////////////
		// SGR Select Graphic Rendition
		/////////////////////////////////////////
		case 'm':
		{
			// handle 30-37,39 for foreground color changes
			if( (parameters[0] >= 30) && (parameters[0] <= 39) )
			{
				parameters[0] -= 30;

				//39 is the reset code
				if(parameters[0] == 9){
				    parameters[0] = 15;
				}
				else if(parameters[0] > 7){
					parameters[0] = 7;
				}

				if(parameters[1] == 1)
				{
					// Intensity: Bold makes color bright
					parameters[0] += 8;
				}
				con->foreground = color_table[parameters[0]];
			}
			// handle 40-47 for background color changes
			else if( (parameters[0] >= 40) && (parameters[0] <= 47) )
			{
				parameters[0] -= 40;

				if(parameters[1] == 1)
				{
					// Intensity: Bold makes color bright
					parameters[0] += 8;
				}
				con->background = color_table[parameters[0]];
			}
		  break;
		}
	}

	return(i);
}

int __console_write(struct _reent *r,int fd,const char *ptr,int len)
{
	int i = 0;
	char *tmp = (char*)ptr;
	u32 curr_x,tabsize;
	console_data_s *con;
	char chr;

	if(!curr_con) return -1;
	con = curr_con;
	if(!tmp || len<=0) return -1;

	i = 0;
	while(*tmp!='\0' && i<len)
	{
		chr = *tmp++;
		i++;
		if ( (chr == 0x1b) && (*tmp == '[') )
		{
			/* escape sequence found */
			int k;

			tmp++;
			i++;
			k = __console_parse_escsequence(tmp);
			tmp += k;
			i += k;
		}
		else
		{
			switch(chr)
			{
				case '\n':
					con->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
					con->cursor_x = con->border_left;
					break;
				case '\r':
					con->cursor_x = con->border_left;
					break;
				case '\b':
					con->cursor_x -= FONT_XSIZE*FONT_XFACTOR+FONT_XGAP;
					if(con->cursor_x < con->border_left)
					{
						con->cursor_x = con->border_left;
					}
					break;
				case '\f':
					con->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
					break;
				case '\t':
					curr_x = con->cursor_x;
					tabsize = (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP)*TAB_SIZE;
					if(curr_x%tabsize) con->cursor_x += (curr_x%tabsize);
					else con->cursor_x += tabsize;
					break;
				default:
					__console_drawc(con,con->cursor_x,con->cursor_y,chr);
					con->cursor_x += FONT_XSIZE*FONT_XFACTOR+FONT_XGAP;

					if((con->cursor_x+FONT_XSIZE*FONT_XFACTOR)>con->border_right)
					{
						/* if right border reached wrap around */
						con->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
						con->cursor_x = con->border_left;
					}
			}
		}

		if((con->cursor_y+FONT_YSIZE*FONT_YFACTOR)>=con->border_bottom)
		{
			/* if bottom border reached scroll */
			memcpy(con->destbuffer,
				con->destbuffer+con->con_stride*(FONT_YSIZE*FONT_YFACTOR+FONT_YGAP),
				con->con_stride*con->con_yres-FONT_YSIZE);

			unsigned int cnt = (con->con_stride * (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP))/4;
			unsigned int *ptr = (unsigned int*)(con->destbuffer + con->con_stride * (con->con_yres - FONT_YSIZE));
			while(cnt--)
				*ptr++ = con->background;
			con->cursor_y -= FONT_YSIZE * FONT_YFACTOR + FONT_YGAP;
		}
	}

	return i;
}

void CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	__console_init(framebuffer,xstart,ystart,xres,yres,stride);
}

void CON_GetMetrics(int *cols, int *rows)
{
	if(curr_con) {
		*cols = (curr_con->border_right - curr_con->border_left) / (FONT_XSIZE*FONT_XFACTOR+FONT_XGAP);
		*rows = (curr_con->border_bottom - curr_con->border_top) / (FONT_YSIZE*FONT_YFACTOR+FONT_YGAP);
	}
}


