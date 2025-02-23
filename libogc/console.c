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

#include "console_internal.h"
#include "consol.h"
#include "usbgecko.h"

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
	NULL,		// device statvfs_r
	NULL,		// device ftrunctate_r
	NULL,		// device fsync_r
	NULL,		// deviceData;
};

//color table
static const unsigned int color_table[] =
{
  0x10801080,		// 30 normal black
  0x316D31B8,		// 31 normal red
  0x515B5151,		// 32 normal green
  0x71487189,		// 33 normal yellow
  0x1DB81D77,		// 34 normal blue
  0x3DA53DAF,		// 35 normal magenta
  0x5D935D48,		// 36 normal cyan
  0xB580B580,		// 37 normal white
  0x7E807E80,		// 30 bright black
  0x515A51F0,		// 31 bright red
  0x91369122,		// 32 bright green
  0xD210D292,		// 33 bright yellow
  0x29F0296E,		// 34 bright blue
  0x6ACA6ADE,		// 35 bright magenta
  0xAAA6AA10,		// 36 bright cyan
  0xEB80EB80,		// 37 bright white
};

static bool do_xfb_copy = false;
static struct _console_data_s stdcon;
static struct _console_data_s *curr_con = NULL;
static void *_console_buffer = NULL;

static s32 __gecko_status = -1;
static u32 __gecko_safe = 0;

extern u8 console_font_8x16[];

static u32 __console_get_offset()
{
	if(!curr_con) 
		return 0;
	
	return _console_buffer != NULL 
		? 0 
		: (curr_con->target_y * curr_con->tgt_stride) + (curr_con->target_x * VI_DISPLAY_PIX_SZ);
}

void __console_vipostcb(u32 retraceCnt)
{
	u8 *fb,*ptr;

	do_xfb_copy = true;
	ptr = curr_con->destbuffer;
	fb = (u8*)((u32)VIDEO_GetCurrentFramebuffer() + (curr_con->target_y * curr_con->tgt_stride) + (curr_con->target_x * VI_DISPLAY_PIX_SZ));

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < curr_con->con_yres; ycnt++)
	{
		memcpy(fb, ptr, curr_con->con_xres * VI_DISPLAY_PIX_SZ);
		ptr+= curr_con->con_stride;
		fb+= curr_con->tgt_stride;
	}

	do_xfb_copy = false;
}

static void __console_drawc(int c)
{
	console_data_s *con;
	int ay;
	unsigned int *ptr;
	unsigned char *pbits;
	unsigned char bits;
	unsigned int color;
	unsigned int fgcolor, bgcolor;
	unsigned int nextline;

	if(do_xfb_copy==true) return;
	if(!curr_con) return;
	con = curr_con;

	ptr = (unsigned int*)(con->destbuffer + ( con->con_stride * con->cursor_row * FONT_YSIZE ) + ((con->cursor_col * FONT_XSIZE / 2) * 4) 
						+ __console_get_offset());
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

static void __console_clear_line(int line, int from, int to ) 
{
	console_data_s *con;
	u8* p;
	// Each character is FONT_XSIZE * VI_DISPLAY_PIX_SZ wide
	const u32 line_width = (to - from) * (FONT_XSIZE * VI_DISPLAY_PIX_SZ);

	if( !(con = curr_con) ) return;

	//add the given row & column to the offset & assign the pointer
	const u32 offset = __console_get_offset() + (line * con->con_stride * FONT_YSIZE) + (from * FONT_XSIZE * VI_DISPLAY_PIX_SZ);
	p = (u8*)((u32)con->destbuffer + offset);

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < FONT_YSIZE; ycnt++)
	{
		for(u32 xcnt = 0; xcnt < line_width; xcnt += 4)
			*(u32*)((u32)p + xcnt) = con->background;

		p += con->con_stride;
	}
}

static void __console_clear(void)
{
	console_data_s *con;
	u8* p;

	if( !(con = curr_con) ) return;

	//get console pointer
	p = (u8*)((u32)con->destbuffer + __console_get_offset());

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < con->con_yres; ycnt++)
	{
		for(u32 xcnt = 0; xcnt < con->con_xres * VI_DISPLAY_PIX_SZ; xcnt+=4)
			*(u32*)((u32)p + xcnt) = con->background;

		p += con->con_stride;
	}

	con->cursor_row = 0;
	con->cursor_col = 0;
	con->saved_row = 0;
	con->saved_col = 0;
}
static void __console_clear_from_cursor(void) {
	console_data_s *con;
	int cur_row;
	
  if( !(con = curr_con) ) return;
	cur_row = con->cursor_row;
	
  __console_clear_line( cur_row, con->cursor_col, con->con_cols );
  
  while( cur_row++ < con->con_rows )
    __console_clear_line( cur_row, 0, con->con_cols );
  
}
static void __console_clear_to_cursor(void) {
	console_data_s *con;
	int cur_row;
	
  if( !(con = curr_con) ) return;
	cur_row = con->cursor_row;
	
  __console_clear_line( cur_row, 0, con->cursor_col );
  
  while( cur_row-- )
    __console_clear_line( cur_row, 0, con->con_cols );
}

void __console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;
	console_data_s *con = &stdcon;

	_CPU_ISR_Disable(level);

	con->destbuffer = framebuffer;
	con->con_xres = xres;
	con->con_yres = yres;
	con->con_cols = xres / FONT_XSIZE;
	con->con_rows = yres / FONT_YSIZE;
	con->con_stride = con->tgt_stride = stride;
	con->target_x = xstart;
	con->target_y = ystart;
	con->cursor_row = 0;
	con->cursor_col = 0;
	con->saved_row = 0;
	con->saved_col = 0;

	con->font = console_font_8x16;

	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;

	curr_con = con;

	__console_clear();

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
	_CPU_ISR_Restore(level);

	setvbuf(stdout, NULL , _IONBF, 0);
	setvbuf(stderr, NULL , _IONBF, 0);
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
	con->con_cols = con_xres / FONT_XSIZE;
	con->con_rows = con_yres / FONT_YSIZE;
	con->cursor_row = 0;
	con->cursor_col = 0;
	con->saved_row = 0;
	con->saved_col = 0;

	con->font = console_font_8x16;

	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;

	curr_con = con;

	__console_clear();

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;

	VIDEO_SetPostRetraceCallback(__console_vipostcb);

	_CPU_ISR_Restore(level);

	setvbuf(stdout, NULL , _IONBF, 0);
	setvbuf(stderr, NULL , _IONBF, 0);
}

static int __console_parse_escsequence(const char *pchr)
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
			curr_con->cursor_row -= parameters[0];
			if(curr_con->cursor_row < 0) curr_con->cursor_row = 0;
			break;
		}
		case 'B':
		{
			curr_con->cursor_row += parameters[0];
			if(curr_con->cursor_row >= curr_con->con_rows) curr_con->cursor_row = curr_con->con_rows - 1;
			break;
		}
		case 'C':
		{
			curr_con->cursor_col += parameters[0];
			if(curr_con->cursor_col >= curr_con->con_cols) curr_con->cursor_col = curr_con->con_cols - 1;
			break;
		}
		case 'D':
		{
			curr_con->cursor_col -= parameters[0];
			if(curr_con->cursor_col < 0) curr_con->cursor_col = 0;
			break;
		}
		/////////////////////////////////////////
		// Cursor position movement
		/////////////////////////////////////////
		case 'H':
		case 'f':
		{
			curr_con->cursor_col = parameters[1];
			curr_con->cursor_row = parameters[0];
			if(curr_con->cursor_row >= curr_con->con_rows) curr_con->cursor_row = curr_con->con_rows - 1;
			if(curr_con->cursor_col >= curr_con->con_cols) curr_con->cursor_col = curr_con->con_cols - 1;
			break;
		}
		/////////////////////////////////////////
		// Screen clear
		/////////////////////////////////////////
		case 'J':
		{
			if( parameters[0] == 0 )
        __console_clear_from_cursor();
			if( parameters[0] == 1 )
        __console_clear_to_cursor();
			if( parameters[0] == 2 )
        __console_clear();
        
			break;
		}
		/////////////////////////////////////////
		// Line clear
		/////////////////////////////////////////
		case 'K':
		{
			if( parameters[0] == 0 )
        __console_clear_line( curr_con->cursor_row, curr_con->cursor_col, curr_con->con_cols );
			if( parameters[0] == 1 )
        __console_clear_line( curr_con->cursor_row, 0, curr_con->cursor_col );
			if( parameters[0] == 2 )
        __console_clear_line( curr_con->cursor_row, 0, curr_con->con_cols);
        
			break;
		}
		/////////////////////////////////////////
		// Save cursor position
		/////////////////////////////////////////
		case 's':
		{
			con->saved_col = con->cursor_col;
			con->saved_row = con->cursor_row;
			break;
		}
		/////////////////////////////////////////
		// Load cursor position
		/////////////////////////////////////////
		case 'u':
			con->cursor_col = con->saved_col;
			con->cursor_row = con->saved_row;
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

int __console_write(struct _reent *r,void *fd,const char *ptr,size_t len)
{
	size_t i = 0;
	const char *tmp = ptr;
	console_data_s *con;
	char chr;

	if(__gecko_status>=0) {
		if(__gecko_safe)
			usb_sendbuffer_safe(__gecko_status,ptr,len);
		else
			usb_sendbuffer(__gecko_status,ptr,len);
	}

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
					con->cursor_row++;
					con->cursor_col = 0;
					break;
				case '\r':
					con->cursor_col = 0;
					break;
				case '\b':
					con->cursor_col--;
					if(con->cursor_col < 0)
					{
						con->cursor_col = 0;
					}
					break;
				case '\f':
					con->cursor_row++;
					break;
				case '\t':
					if(con->cursor_col%TAB_SIZE) con->cursor_col += (con->cursor_col%TAB_SIZE);
					else con->cursor_col += TAB_SIZE;
					break;
				default:
					__console_drawc(chr);
					con->cursor_col++;

					if( con->cursor_col >= con->con_cols)
					{
						/* if right border reached wrap around */
						con->cursor_row++;
						con->cursor_col = 0;
					}
			}
		}

		/* if bottom border reached, scroll */
		if( con->cursor_row >= con->con_rows)
		{
			const u8* console = (u8*)((u32)con->destbuffer + __console_get_offset());
			const u32 last_console_row = curr_con->con_rows == 0
				? 0
				: (curr_con->con_rows-1);

			//copy lines upwards
			u8* ptr = (u8*)console;
			for(u32 ycnt = 0; ycnt < (last_console_row * FONT_YSIZE); ycnt++)
			{
				memmove(ptr, ptr + curr_con->con_stride * FONT_YSIZE, curr_con->con_cols * FONT_XSIZE * VI_DISPLAY_PIX_SZ);
				ptr+= curr_con->con_stride;
			}
			
			//clear last line
			__console_clear_line(last_console_row, 0, con->con_cols);
			con->cursor_row--;
		}
	}

	return i;
}

void CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	__console_init(framebuffer,xstart,ystart,xres,yres,stride);
}

s32 CON_InitEx(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight)
{
	VIDEO_SetPostRetraceCallback(NULL);
	if(_console_buffer)
		free(_console_buffer);
	
	_console_buffer = malloc(conWidth*conHeight*VI_DISPLAY_PIX_SZ);
	if(!_console_buffer) return -1;

	__console_init_ex(_console_buffer,conXOrigin,conYOrigin,rmode->fbWidth*VI_DISPLAY_PIX_SZ,conWidth,conHeight,conWidth*VI_DISPLAY_PIX_SZ);

	return 0;
}

void CON_GetMetrics(int *cols, int *rows)
{
	if(curr_con) {
		*cols = curr_con->con_cols;
		*rows = curr_con->con_rows;
	}
}

void CON_GetPosition(int *col, int *row)
{
	if(curr_con) {
		*col = curr_con->cursor_col;
		*row = curr_con->cursor_row;
	}
}

void CON_EnableGecko(int channel,int safe)
{
	if(channel && (channel>1 || !usb_isgeckoalive(channel))) channel = -1;

	__gecko_status = channel;
	__gecko_safe = safe;

	if(__gecko_status!=-1) {
		devoptab_list[STD_OUT] = &dotab_stdout;
		devoptab_list[STD_ERR] = &dotab_stdout;

		// line buffered output for threaded apps when only using the usbgecko
		if(!curr_con) {
			setvbuf(stdout, NULL, _IOLBF, 0);
			setvbuf(stderr, NULL, _IOLBF, 0);
		}
	}
}

