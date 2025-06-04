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

const devoptab_t dotab_stdout = {
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

static const u32 RGB8_to_YCbCr(u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2)
{
	if (r1 < 16) r1 = 16;
	if (g1 < 16) g1 = 16;
	if (b1 < 16) b1 = 16;
	if (r2 < 16) r2 = 16;
	if (g2 < 16) g2 = 16;
	if (b2 < 16) b2 = 16;

	if (r1 > 240) r1 = 240;
	if (g1 > 240) g1 = 240;
	if (b1 > 240) b1 = 240;
	if (r2 > 240) r2 = 240;
	if (g2 > 240) g2 = 240;
	if (b2 > 240) b2 = 240;

	u8 Y1 = ( 77 * r1 + 150 * g1 + 29 * b1) / 256;
  u8 Y2 = ( 77 * r2 + 150 * g2 + 29 * b2) / 256;
  u8 Cb = (112 * (b1 + b2) -  74 * (g1 + g2) - 38 * (r1 + r2)) / 512 + 128;
  u8 Cr = (112 * (r1 + r2) - 94 * (g1 + g2) - 18 * (b1 + b2)) / 512 + 128;

  return Y1 << 24 | Cb << 16 | Y2 << 8 | Cr;
}

static const u32 single_RGB8_to_YCbCr(const u8 r, const u8 g, const u8 b)
{
    return RGB8_to_YCbCr(r,g,b,r,g,b);
}

//set up the palette for color printing
static const u32 colorTable[] = {
	0x10801080, // black
	0x317031b1, // red
	0x51605157, // green
	0x734f7387, // yellow
	0x1cb11c79, // blue
	0x3ea03ea9, // magenta
	0x5e905e4f, // cyan
	0xc080c080, // white

	0x10801080, // bright black
	0x535f53e2, // bright red
	0x9340932e, // bright green
	0xd61ed68f, // bright yellow
	0x29e22971, // bright blue
	0x6cc06cd2, // bright magenta
	0xaca1ac1e, // bright cyan
	0xf080f080, // bright white

	0x10801080, // faint black
	0x2377239c, // faint red
	0x356e3569, // faint green
	0x48644884, // faint yellow
	0x179c177c, // faint blue
	0x2a922a97, // faint magenta
	0x3c893c64, // faint cyan
	0x70807080, // faint white
};

static const u8 colorCube[] = {
	0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff,
};

static const u8 grayScale[] = {
	0x08, 0x12, 0x1c, 0x26, 0x30, 0x3a, 0x44, 0x4e,
	0x58, 0x62, 0x6c, 0x76, 0x80, 0x8a, 0x94, 0x9e,
	0xa8, 0xb2, 0xbc, 0xc6, 0xd0, 0xda, 0xe4, 0xee,
};

static bool do_xfb_copy = false;
static struct _console_data_s stdcon;
static struct _console_data_s *currentConsole = NULL;
static void *_console_buffer = NULL;

static s32 __gecko_status = -1;
static u32 __gecko_safe = 0;

extern u8 console_font_8x16[];

static u32 __console_get_offset()
{
	if(!currentConsole)
		return 0;
	
	return _console_buffer != NULL 
		? 0 
		: (currentConsole->target_y * currentConsole->tgt_stride) + (currentConsole->target_x * VI_DISPLAY_PIX_SZ);
}

void __console_vipostcb(u32 retraceCnt)
{
	u8 *fb,*ptr;

	do_xfb_copy = true;
	ptr = currentConsole->destbuffer;
	fb = (u8*)((u32)VIDEO_GetCurrentFramebuffer() + (currentConsole->target_y * currentConsole->tgt_stride) + (currentConsole->target_x * VI_DISPLAY_PIX_SZ));

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < currentConsole->con_yres; ycnt++)
	{
		memcpy(fb, ptr, currentConsole->con_xres * VI_DISPLAY_PIX_SZ);
		ptr+= currentConsole->con_stride;
		fb+= currentConsole->tgt_stride;
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
	unsigned int nextline;
	unsigned int fgcolor, bgcolor;

	if(do_xfb_copy==true) return;
	if(!currentConsole) return;
	con = currentConsole;

	ptr = (unsigned int*)(con->destbuffer + ( con->con_stride * con->cursorY * FONT_YSIZE ) + ((con->cursorX * FONT_XSIZE / 2) * 4) 
						+ __console_get_offset());
	pbits = &con->font[c * FONT_YSIZE];
	nextline = con->con_stride/4 - 4;

	fgcolor = currentConsole->fg;
	bgcolor = currentConsole->bg;

	if (!(currentConsole->flags & CONSOLE_FG_CUSTOM)) {
		if (currentConsole->flags & CONSOLE_COLOR_BOLD) {
			fgcolor = colorTable[fgcolor + 8];
		} else if (currentConsole->flags & CONSOLE_COLOR_FAINT) {
			fgcolor = colorTable[fgcolor + 16];
		} else {
			fgcolor = colorTable[fgcolor];
		}
	}

	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		bgcolor = colorTable[bgcolor];
	}

	if (currentConsole->flags & CONSOLE_COLOR_REVERSE) {
		unsigned int tmp = fgcolor;
		fgcolor = bgcolor;
		bgcolor = tmp;
	}

	for (ay = 0; ay < FONT_YSIZE; ay++)
	{
		/* hard coded loop unrolling ! */
		/* this depends on FONT_XSIZE = 8*/
#if FONT_XSIZE == 8
		bits = *pbits++;

		if ( (currentConsole->flags & CONSOLE_CROSSED_OUT) && (ay == 7 || ay == 8)) bits = 0xff;
		if ( (currentConsole->flags & CONSOLE_UNDERLINE) && (ay == 14 || ay == 15)) bits = 0xff;
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

	if( !(con = currentConsole) ) return;

  unsigned int bgcolor = con->bg;
	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		bgcolor = colorTable[bgcolor];
	}

	//add the given row & column to the offset & assign the pointer
	const u32 offset = __console_get_offset() + (line * con->con_stride * FONT_YSIZE) + (from * FONT_XSIZE * VI_DISPLAY_PIX_SZ);
	p = (u8*)((u32)con->destbuffer + offset);

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < FONT_YSIZE; ycnt++)
	{
		for(u32 xcnt = 0; xcnt < line_width; xcnt += 4)
			*(u32*)((u32)p + xcnt) = bgcolor;

		p += con->con_stride;
	}
}

static void __console_clear(void)
{
	console_data_s *con;
	u8* p;

	if( !(con = currentConsole) ) return;

	unsigned int bgcolor = con->bg;
	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		bgcolor = colorTable[bgcolor];
	}

	//get console pointer
	p = (u8*)((u32)con->destbuffer + __console_get_offset());

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < con->con_yres; ycnt++)
	{
		for(u32 xcnt = 0; xcnt < con->con_xres * VI_DISPLAY_PIX_SZ; xcnt+=4)
			*(u32*)((u32)p + xcnt) = bgcolor;

		p += con->con_stride;
	}

	con->cursorY = 0;
	con->cursorX = 0;
	con->prevCursorX = 0;
	con->prevCursorY = 0;
}

static void __console_clear_from_cursor(void) {
	console_data_s *con;
	int cur_row;
	
  if( !(con = currentConsole) ) return;
	cur_row = con->cursorY;
	
  __console_clear_line( cur_row, con->cursorX, con->con_cols );
  
  while( cur_row++ < con->con_rows )
    __console_clear_line( cur_row, 0, con->con_cols );
  
}
static void __console_clear_to_cursor(void) {
	console_data_s *con;
	int cur_row;
	
  if( !(con = currentConsole) ) return;
	cur_row = con->cursorY;
	
  __console_clear_line( cur_row, 0, con->cursorX );
  
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
	con->cursorY = 0;
	con->cursorX = 0;
	con->prevCursorX = 0;
	con->prevCursorY = 0;

	con->font = console_font_8x16;

	con->fg = 7;
	con->bg = 0;

	con->flags = 0;
  con->tabSize = 4;

	currentConsole = con;

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
	con->cursorY = 0;
	con->cursorX = 0;
	con->prevCursorX = 0;
	con->prevCursorY = 0;

	con->font = console_font_8x16;

	con->fg = 7;
	con->bg = 0;

	con->flags = 0;
  con->tabSize = 4;

	currentConsole = con;

	__console_clear();

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;

	VIDEO_SetPostRetraceCallback(__console_vipostcb);

	_CPU_ISR_Restore(level);

	setvbuf(stdout, NULL , _IONBF, 0);
	setvbuf(stderr, NULL , _IONBF, 0);
}

static struct
{
	union
	{
		struct
		{
			int movement;
		} directional;
		struct
		{
			int y;
			int x;
		} absolute;
		struct
		{
			int type;
		} clear;
		struct
		{
			int args[3];
			int flags;
			u32 fg;
			u32 bg;
		} color;
		int rawBuf[5];
	};
	int argIdx;
	bool hasArg[3];
	enum ESC_STATE
	{
		ESC_NONE,
		ESC_START,
		ESC_BUILDING_UNKNOWN,
		ESC_BUILDING_FORMAT_UNKNOWN,
		ESC_BUILDING_FORMAT_FG,
		ESC_BUILDING_FORMAT_BG,
		ESC_BUILDING_FORMAT_FG_NONRGB,
		ESC_BUILDING_FORMAT_BG_NONRGB,
		ESC_BUILDING_FORMAT_FG_RGB,
		ESC_BUILDING_FORMAT_BG_RGB,
	} state;
} escapeSeq;


static void consoleClearLine(int mode) {

	switch(mode) {
	case 0:
		 __console_clear_line( currentConsole->cursorY, currentConsole->cursorX, currentConsole->con_cols );
		break;
	case 1:
		__console_clear_line( currentConsole->cursorY, 0, currentConsole->cursorX );
		break;
	case 2:
		__console_clear_line( currentConsole->cursorY, 0, currentConsole->con_cols);
		break;
	}
}

static void consoleCls(int mode) {

	switch(mode) {
	case 0:
		__console_clear_from_cursor();
		break;
	case 1:
		__console_clear_to_cursor();
		break;
	case 2:
	case 3:
		__console_clear();
		break;
	}
}

static inline void consolePosition(int x, int y) {
	// invalid position
	if(x < 0 || y < 0)
		return;

	// 1-based, but we'll take a 0
	if(x < 1)
		x = 1;
	if(y < 1)
		y = 1;

	// clip to console edge
	if(x > currentConsole->con_cols)
		x = currentConsole->con_cols;
	if(y > currentConsole->con_rows)
		y = currentConsole->con_rows;

	// 1-based adjustment
	currentConsole->cursorX = x - 1;
	currentConsole->cursorY = y - 1;
}

static void consoleHandleColorEsc(int code)
{
	switch (escapeSeq.state)
	{
		case ESC_BUILDING_FORMAT_UNKNOWN:
			switch (code)
			{
				case 0: // reset
					escapeSeq.color.flags = 0;
					escapeSeq.color.bg    = 0;
					escapeSeq.color.fg    = 7;
					break;

				case 1: // bold
					escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
					escapeSeq.color.flags |= CONSOLE_COLOR_BOLD;
					SYS_Report("set bold\n");
					break;

				case 2: // faint
					escapeSeq.color.flags &= ~CONSOLE_COLOR_BOLD;
					escapeSeq.color.flags |= CONSOLE_COLOR_FAINT;
					break;

				case 3: // italic
					escapeSeq.color.flags |= CONSOLE_ITALIC;
					break;

				case 4: // underline
					escapeSeq.color.flags |= CONSOLE_UNDERLINE;
					break;

				case 5: // blink slow
					escapeSeq.color.flags &= ~CONSOLE_BLINK_FAST;
					escapeSeq.color.flags |= CONSOLE_BLINK_SLOW;
					break;

				case 6: // blink fast
					escapeSeq.color.flags &= ~CONSOLE_BLINK_SLOW;
					escapeSeq.color.flags |= CONSOLE_BLINK_FAST;
					break;

				case 7: // reverse video
					escapeSeq.color.flags |= CONSOLE_COLOR_REVERSE;
					break;

				case 8: // conceal
					escapeSeq.color.flags |= CONSOLE_CONCEAL;
					break;

				case 9: // crossed-out
					escapeSeq.color.flags |= CONSOLE_CROSSED_OUT;
					break;

				case 21: // bold off
					escapeSeq.color.flags &= ~CONSOLE_COLOR_BOLD;
					break;

				case 22: // normal color
					escapeSeq.color.flags &= ~CONSOLE_COLOR_BOLD;
					escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
					break;

				case 23: // italic off
					escapeSeq.color.flags &= ~CONSOLE_ITALIC;
					break;

				case 24: // underline off
					escapeSeq.color.flags &= ~CONSOLE_UNDERLINE;
					break;

				case 25: // blink off
					escapeSeq.color.flags &= ~CONSOLE_BLINK_SLOW;
					escapeSeq.color.flags &= ~CONSOLE_BLINK_FAST;
					break;

				case 27: // reverse off
					escapeSeq.color.flags &= ~CONSOLE_COLOR_REVERSE;
					break;

				case 29: // crossed-out off
					escapeSeq.color.flags &= ~CONSOLE_CROSSED_OUT;
					break;

				case 30 ... 37: // writing color
					escapeSeq.color.flags &= ~CONSOLE_FG_CUSTOM;
					escapeSeq.color.fg     = code - 30;
					break;

				case 38: // custom foreground color
					escapeSeq.state = ESC_BUILDING_FORMAT_FG;
					break;

				case 39: // reset foreground color
					escapeSeq.color.flags &= ~CONSOLE_FG_CUSTOM;
					escapeSeq.color.fg     = 7;
					break;

				case 40 ... 47: // screen color
					escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
					escapeSeq.color.bg = code - 40;
					break;

				case 48: // custom background color
					escapeSeq.state = ESC_BUILDING_FORMAT_BG;
					break;

				case 49: // reset background color
					escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
					escapeSeq.color.fg = 0;
					break;
			}
		break;
		case ESC_BUILDING_FORMAT_FG:
			if (escapeSeq.color.args[0] == 5)
				escapeSeq.state = ESC_BUILDING_FORMAT_FG_NONRGB;
			else if (escapeSeq.color.args[0] == 2)
				escapeSeq.state = ESC_BUILDING_FORMAT_FG_RGB;
			else
				escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_BG:
			if (escapeSeq.color.args[0] == 5)
				escapeSeq.state = ESC_BUILDING_FORMAT_BG_NONRGB;
			else if (escapeSeq.color.args[0] == 2)
				escapeSeq.state = ESC_BUILDING_FORMAT_BG_RGB;
			else
				escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_FG_NONRGB:
			if (code <= 15) {
				escapeSeq.color.fg  = code;
				escapeSeq.color.flags &= ~CONSOLE_FG_CUSTOM;
			} else if (code <= 231) {
				code -= 16;
				unsigned int r = code / 36;
				unsigned int g = (code - r * 36) / 6;
				unsigned int b = code - r * 36 - g * 6;

				escapeSeq.color.fg  = single_RGB8_to_YCbCr (colorCube[r], colorCube[g], colorCube[b]);
				escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
			} else if (code <= 255) {
				code -= 232;

				escapeSeq.color.fg  = single_RGB8_to_YCbCr (grayScale[code], grayScale[code], grayScale[code]);
				escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
			}
			escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_BG_NONRGB:
			if (code <= 15) {
				escapeSeq.color.bg  = code;
				escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
			} else if (code <= 231) {
				code -= 16;
				unsigned int r = code / 36;
				unsigned int g = (code - r * 36) / 6;
				unsigned int b = code - r * 36 - g * 6;

				escapeSeq.color.bg  = single_RGB8_to_YCbCr (colorCube[r], colorCube[g], colorCube[b]);
				escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
			} else if (code <= 255) {
				code -= 232;

				escapeSeq.color.bg  = single_RGB8_to_YCbCr (grayScale[code], grayScale[code], grayScale[code]);
				escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
			}
			escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_FG_RGB:
			escapeSeq.color.fg = single_RGB8_to_YCbCr((unsigned int)escapeSeq.color.args[0], (unsigned int)escapeSeq.color.args[1], (unsigned int)escapeSeq.color.args[2]);
			escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
			escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_BG_RGB:
			escapeSeq.color.bg = single_RGB8_to_YCbCr((unsigned int)escapeSeq.color.args[0], (unsigned int)escapeSeq.color.args[1], (unsigned int)escapeSeq.color.args[2]);
			escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
			escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			break;
		default:
			break;
	}
	escapeSeq.argIdx = 0;
}

static void consoleColorStateShift(void)
{
	switch (escapeSeq.state)
	{
		case ESC_BUILDING_UNKNOWN:
			escapeSeq.state = ESC_BUILDING_FORMAT_UNKNOWN;
			if (escapeSeq.hasArg[0])
				consoleHandleColorEsc(escapeSeq.color.args[0]);
			if (escapeSeq.hasArg[1])
				consoleHandleColorEsc(escapeSeq.color.args[1]);
			escapeSeq.argIdx = 0;
			escapeSeq.hasArg[0] = escapeSeq.hasArg[1] = false;
			break;
		case ESC_BUILDING_FORMAT_BG:
		case ESC_BUILDING_FORMAT_FG:
		case ESC_BUILDING_FORMAT_FG_NONRGB:
		case ESC_BUILDING_FORMAT_BG_NONRGB:
			consoleHandleColorEsc(escapeSeq.color.args[0]);
			escapeSeq.argIdx = 0;
			escapeSeq.hasArg[0] = escapeSeq.hasArg[1] = false;
			break;
		case ESC_BUILDING_FORMAT_FG_RGB:
		case ESC_BUILDING_FORMAT_BG_RGB:
			if (escapeSeq.argIdx < 3)
				escapeSeq.argIdx++;
			else
				consoleHandleColorEsc(0); // Nothing passed here because three RGB items
			break;
		default:
			break;
	}
}

static void consoleColorApply(void)
{
	currentConsole->bg = escapeSeq.color.bg;
	currentConsole->fg = escapeSeq.color.fg;
	currentConsole->flags = escapeSeq.color.flags;
	SYS_Report("fg: %x, bg: %x, flags: %08x\n", currentConsole->fg, currentConsole->bg, currentConsole->flags);
}

void newRow()
{

	currentConsole->cursorY++;
	/* if bottom border reached, scroll */
	if( currentConsole->cursorY >= currentConsole->con_rows)
	{
		const u8* console = (u8*)((u32)currentConsole->destbuffer + __console_get_offset());
		const u32 last_console_row = currentConsole->con_rows == 0
			? 0
			: (currentConsole->con_rows-1);

		//copy lines upwards
		u8* ptr = (u8*)console;
		for(u32 ycnt = 0; ycnt < (last_console_row * FONT_YSIZE); ycnt++)
		{
			memmove(ptr, ptr + currentConsole->con_stride * FONT_YSIZE, currentConsole->con_cols * FONT_XSIZE * VI_DISPLAY_PIX_SZ);
			ptr+= currentConsole->con_stride;
		}

		//clear last line
		__console_clear_line(last_console_row, 0, currentConsole->con_cols);
		currentConsole->cursorY = currentConsole->con_rows - 1;
	}
}

void consolePrintChar(int c)
{
	if (c==0) return;

	if(currentConsole->cursorX  >= currentConsole->con_cols) {
		currentConsole->cursorX  = 0;
		newRow();
	}

	switch(c) {
		/*
		The only special characters we will handle are tab (\t), carriage return (\r), line feed (\n)
		and backspace (\b).
		Carriage return & line feed will function the same: go to next line and put cursor at the beginning.
		For everything else, use VT sequences.

		Reason: VT sequences are more specific to the task of cursor placement.
		The special escape sequences \b \f & \v are archaic and non-portable.
		*/
		case 8:
			currentConsole->cursorX--;

			if(currentConsole->cursorX < 0) {
				if(currentConsole->cursorY > 0) {
					currentConsole->cursorX = currentConsole->con_cols - 1;
					currentConsole->cursorY--;
				} else {
					currentConsole->cursorX = 0;
				}
			}

			__console_drawc(' ');
			break;

		case 9:
			currentConsole->cursorX  += currentConsole->tabSize - ((currentConsole->cursorX)%(currentConsole->tabSize));
			break;
		case 10:
			newRow();
		case 13:
			currentConsole->cursorX  = 0;
			break;
		default:
			__console_drawc(c);
			++currentConsole->cursorX ;
			break;
	}
}

ssize_t __console_write(struct _reent *r,void *fd,const char *ptr, size_t len)
{

	char chr;

	int i, count = 0;
	char *tmp = (char*)ptr;

	if(!tmp) return -1;

	i = 0;

	if(__gecko_status>=0) {
		if(__gecko_safe)
			usb_sendbuffer_safe(__gecko_status,ptr,len);
		else
			usb_sendbuffer(__gecko_status,ptr,len);
	}

	if(!currentConsole) return -1;
	if(!tmp || len<=0) return -1;

	while(i<len) {

		chr = *(tmp++);
		i++; count++;

		switch (escapeSeq.state)
		{
			case ESC_NONE:
				if (chr == 0x1b)
					escapeSeq.state = ESC_START;
				else
					consolePrintChar(chr);
				break;
			case ESC_START:
				if (chr == '[')
				{
					escapeSeq.state = ESC_BUILDING_UNKNOWN;
					memset(escapeSeq.rawBuf, 0, sizeof(escapeSeq.rawBuf));
					memset(escapeSeq.hasArg, 0, sizeof(escapeSeq.hasArg));
					escapeSeq.color.bg = currentConsole->bg;
					escapeSeq.color.fg = currentConsole->fg;
					escapeSeq.color.flags = currentConsole->flags;
					escapeSeq.argIdx = 0;
				}
				else
				{
					consolePrintChar(0x1b);
					consolePrintChar(chr);
					escapeSeq.state = ESC_NONE;
				}
				break;
			case ESC_BUILDING_UNKNOWN:
				switch (chr)
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						escapeSeq.hasArg[escapeSeq.argIdx] = true;
						escapeSeq.rawBuf[escapeSeq.argIdx] = escapeSeq.rawBuf[escapeSeq.argIdx] * 10 + (chr - '0');
						break;
					case ';':
						if (escapeSeq.argIdx < 2)
							escapeSeq.argIdx++;
						else
							consoleColorStateShift();
						break;

					//---------------------------------------
					// Cursor directional movement
					//---------------------------------------
					case 'A':
						if (!escapeSeq.hasArg[0])
							escapeSeq.directional.movement = 1;
						currentConsole->cursorY  =  (currentConsole->cursorY  - escapeSeq.directional.movement) < 0 ? 0 : currentConsole->cursorY  - escapeSeq.directional.movement;
						escapeSeq.state = ESC_NONE;
						break;
					case 'B':
						if (!escapeSeq.hasArg[0])
							escapeSeq.directional.movement = 1;
						currentConsole->cursorY  =  (currentConsole->cursorY  + escapeSeq.directional.movement) > currentConsole->con_rows - 1 ? currentConsole->con_rows - 1 : currentConsole->cursorY  + escapeSeq.directional.movement;
						escapeSeq.state = ESC_NONE;
						break;
					case 'C':
						if (!escapeSeq.hasArg[0])
							escapeSeq.directional.movement = 1;
						currentConsole->cursorX  =  (currentConsole->cursorX  + escapeSeq.directional.movement) > currentConsole->con_cols - 1 ? currentConsole->con_cols - 1 : currentConsole->cursorX  + escapeSeq.directional.movement;
						escapeSeq.state = ESC_NONE;
						break;
					case 'D':
						if (!escapeSeq.hasArg[0])
							escapeSeq.directional.movement = 1;
						currentConsole->cursorX  =  (currentConsole->cursorX  - escapeSeq.directional.movement) < 0 ? 0 : currentConsole->cursorX  - escapeSeq.directional.movement;
						escapeSeq.state = ESC_NONE;
						break;
					//---------------------------------------
					// Cursor position movement
					//---------------------------------------
					case 'H':
					case 'f':
						consolePosition(escapeSeq.hasArg[1] ? escapeSeq.absolute.x : 1, escapeSeq.hasArg[0] ? escapeSeq.absolute.y : 1);
						escapeSeq.state = ESC_NONE;
						break;
					//---------------------------------------
					// Screen clear
					//---------------------------------------
					case 'J':
						consoleCls(escapeSeq.hasArg[0] ? escapeSeq.clear.type : 0);
						escapeSeq.state = ESC_NONE;
						break;
					//---------------------------------------
					// Line clear
					//---------------------------------------
					case 'K':
						consoleClearLine(escapeSeq.hasArg[0] ? escapeSeq.clear.type : 0);
						escapeSeq.state = ESC_NONE;
						break;
					//---------------------------------------
					// Save cursor position
					//---------------------------------------
					case 's':
						currentConsole->prevCursorX  = currentConsole->cursorX ;
						currentConsole->prevCursorY  = currentConsole->cursorY ;
						escapeSeq.state = ESC_NONE;
						break;
					//---------------------------------------
					// Load cursor position
					//---------------------------------------
					case 'u':
						currentConsole->cursorX  = currentConsole->prevCursorX ;
						currentConsole->cursorY  = currentConsole->prevCursorY ;
						escapeSeq.state = ESC_NONE;
						break;
					//---------------------------------------
					// Color scan codes
					//---------------------------------------
					case 'm':
						consoleColorStateShift();
						consoleColorApply();
						escapeSeq.state = ESC_NONE;
						break;

					default:
						// some sort of unsupported escape; just gloss over it
						escapeSeq.state = ESC_NONE;
						break;
				}
				break;
			default:
				switch (chr)
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						escapeSeq.hasArg[escapeSeq.argIdx] = true;
						escapeSeq.rawBuf[escapeSeq.argIdx] = escapeSeq.rawBuf[escapeSeq.argIdx] * 10 + (chr - '0');
						break;
					case ';':
						consoleColorStateShift();
						break;
					case 'm':
						consoleColorStateShift();
						consoleColorApply();
						escapeSeq.state = ESC_NONE;
						break;
					default:
						// some sort of unsupported escape; just gloss over it
						escapeSeq.state = ESC_NONE;
						break;
				}
			}
	}

	return count;
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
	if(currentConsole) {
		*cols = currentConsole->con_cols;
		*rows = currentConsole->con_rows;
	}
}

void CON_GetPosition(int *col, int *row)
{
	if(currentConsole) {
		*col = currentConsole->cursorX;
		*row = currentConsole->cursorY;
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
		if(!currentConsole) {
			setvbuf(stdout, NULL, _IOLBF, 0);
			setvbuf(stderr, NULL, _IOLBF, 0);
		}
	}
}

