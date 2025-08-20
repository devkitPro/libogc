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
static void *_console_buffer = NULL;

static s32 __gecko_status = -1;
static u32 __gecko_safe = 0;

extern u8 console_font_8x16[];

PrintConsole defaultConsole =
{
	//Font:
	{
		(u8*)console_font_8x16, //font gfx
		0, //first ascii character in the set
		256 //number of characters in the font set
	},
	(u16*)NULL,
	0,0,	//cursorX cursorY
	0,0,	//prevcursorX prevcursorY
	80,		//console width
	30,		//console height
	0,		//window x
	0,		//window y
	80,		//window width
	30,		//window height
	3,		//tab size
	7,		// foreground color
	0,		// background color
	0,		// flags
	0,		//print callback
	false	//console initialized
};

PrintConsole currentCopy;

PrintConsole* currentConsole = &currentCopy;

static void *__console_offset_by_pixels(void *ptr, s32 dy_pixels, u32 stride_bytes, s32 dx_pixels)
{
	const s32 dy_bytes = dy_pixels * stride_bytes;
	const s32 dx_bytes = dx_pixels * VI_DISPLAY_PIX_SZ;
	return (u8 *)ptr + dy_bytes + dx_bytes;
}

// cursor_y and cursor_x are 1-indexed tile offsets (to start of tile), window offsets are a type of cursor
// window/console dimensions are a count of tiles, but 0-indexed
static void *__console_offset_by_cursor(void *ptr, u32 cursor_y, u32 stride_bytes, u32 cursor_x)
{
	return __console_offset_by_pixels(ptr, (cursor_y - 1) * FONT_YSIZE, stride_bytes, (cursor_x - 1) * FONT_XSIZE);
}

static void *__console_get_buffer_start_ptr()
{
	if(!currentConsole)
		return NULL;

	// using CON_InitEx automatically copies the console buffer to the framebuffer via __console_vipostcb
	// this copy may be to an offset, which is where target_y/target_x/tgt_stride come into play in this case
	// detected by _console_buffer being non-NULL (and by construction == currentConsole->destbuffer)

	// using CON_Init uses the provided pointer directly
	// then target_y/target_x/tgt_stride must be applied here when writing pixels to the console buffer

	// writing characters/pixels to the console buffer via stdio/etc does not involve those values otherwise
	// only console dimensions (con_xres/con_yres/con_stride) and cursor/window values (cursorX/cursorY/con_cols/con_rows/...)

	return _console_buffer != NULL 
		? _console_buffer // same as currentConsole->destbuffer, don't need to load from a struct access
		: __console_offset_by_pixels(currentConsole->destbuffer, currentConsole->target_y, currentConsole->tgt_stride, currentConsole->target_x);
}

static void *__console_get_window_start_ptr()
{
	PrintConsole *con;
	if( !(con = currentConsole) )
		return NULL;

	return __console_offset_by_cursor(__console_get_buffer_start_ptr(), con->windowY, con->con_stride, con->windowX);
}

static void *__console_get_cursor_start_ptr()
{
	PrintConsole *con;
	if( !(con = currentConsole) )
		return NULL;

	/*
	void *ptr = __console_get_buffer_start_ptr();
	ptr = __console_offset_by_cursor(ptr, con->windowY, con->con_stride, con->windowX);
	ptr = __console_offset_by_cursor(ptr, con->cursorY, con->con_stride, con->cursorX);
	return ptr;
	*/
	// equivalent but less calls. -1 for the x/y offsets avoids mistakenly using 2-indexed values
	return __console_offset_by_cursor(__console_get_buffer_start_ptr(), con->windowY + con->cursorY - 1, con->con_stride, con->windowX + con->cursorX - 1);
}

void __console_vipostcb(u32 retraceCnt)
{
	u8 *fb,*ptr;

	do_xfb_copy = true;
	ptr = currentConsole->destbuffer;
	fb = __console_offset_by_pixels(VIDEO_GetCurrentFramebuffer(), currentConsole->target_y, currentConsole->tgt_stride, currentConsole->target_x);

	// Copies 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < currentConsole->con_yres; ycnt++)
	{
		memcpy(fb, ptr, currentConsole->con_xres * VI_DISPLAY_PIX_SZ);
		ptr += currentConsole->con_stride;
		fb += currentConsole->tgt_stride;
	}

	do_xfb_copy = false;
}

static void __console_drawc(int c)
{
	PrintConsole *con;
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
	c -= con->font.asciiOffset;
	if (c<0 || c>con->font.numChars) return;

	ptr = __console_get_cursor_start_ptr();

	pbits = &con->font.gfx[c * FONT_YSIZE];
	// con_stride is in bytes, but we increment ptr which isn't a byte pointer
	// and the work in the loop already increments ptr 4 times before needing to go to the next row
	nextline = con->con_stride/sizeof(*ptr) - 4;

	fgcolor = currentConsole->fg;
	bgcolor = currentConsole->bg;

	if (!(currentConsole->flags & CONSOLE_FG_CUSTOM)) {
		if (currentConsole->flags & (CONSOLE_COLOR_BOLD | CONSOLE_COLOR_FG_BRIGHT)) {
			fgcolor += 8;
		} else if (currentConsole->flags & CONSOLE_COLOR_FAINT) {
			fgcolor += 16;
		}
		fgcolor = colorTable[fgcolor];
	}

	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		if (currentConsole->flags & CONSOLE_COLOR_BG_BRIGHT) bgcolor +=8;
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

/*
line, from, to are 1-indexed tile offsets (to starts of tiles)
to convert from 'an amount of tiles' to 'the end of the final tile' (aka 'the start of the past-the-end tile')
simply add 1 to the 'amount of tiles' value
Examples:
a cursor points to (the start of) tile 3, how many tiles to fill to clear until (the start of) tile 7 ?
-> fill tile 3, 4, 5, 6 (= 4 tiles = 7 - 3)
a cursor points to (the start of) tile 2 in an area with 5 tiles, how many tiles to fill to clear until the end ?
-> fill tile 2, 3, 4, 5 (= 4 tiles = 5 - 2 + 1, because '5' here is an amount, not an offset)
*/
static void __console_clear_line(int line, int from, int to)
{
	PrintConsole *con;
	u8 *p;
	
	if( !(con = currentConsole) ) return;

	// Each character is FONT_XSIZE * VI_DISPLAY_PIX_SZ bytes wide
	const u32 line_width = (to - from) * FONT_XSIZE * VI_DISPLAY_PIX_SZ;

	unsigned int bgcolor = con->bg;

	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		bgcolor = colorTable[bgcolor];
	}

	p = __console_get_window_start_ptr();
	p = __console_offset_by_cursor(p, line, con->con_stride, from);

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < FONT_YSIZE; ycnt++)
	{
		for(u32 xcnt = 0; xcnt < line_width; xcnt += sizeof(u32))
			*(u32*)(p + xcnt) = bgcolor;

		p += con->con_stride;
	}
}

static void __console_clear(void)
{
	PrintConsole *con;
	u8 *p;

	if( !(con = currentConsole) ) return;

	// Window lines add up to con->windowHeight * FONT_YSIZE pixel lines
	const u32 view_height = con->windowHeight * FONT_YSIZE;
	// Each window line is con->windowWidth * FONT_XSIZE * VI_DISPLAY_PIX_SZ bytes wide
	const u32 line_width = con->windowWidth * FONT_XSIZE * VI_DISPLAY_PIX_SZ;

	unsigned int bgcolor = con->bg;
	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		bgcolor = colorTable[bgcolor];
	}

	//get console pointer
	p = __console_get_window_start_ptr();

	// Clears 1 line of pixels at a time
	for(u16 ycnt = 0; ycnt < view_height; ycnt++)
	{
		for(u32 xcnt = 0; xcnt < line_width; xcnt += sizeof(u32))
			*(u32*)(p + xcnt) = bgcolor;

		p += con->con_stride;
	}

	con->cursorY = 1;
	con->cursorX = 1;
	con->prevCursorX = 1;
	con->prevCursorY = 1;
}

static void __console_clear_from_cursor(void) {
	PrintConsole *con;
	int cur_row;
	
	if( !(con = currentConsole) ) return;

	cur_row = con->cursorY;
	
	__console_clear_line( cur_row, con->cursorX, con->windowWidth + 1 );

	while( cur_row++ < con->windowHeight )
		__console_clear_line( cur_row, 1, con->windowWidth + 1 );
}

static void __console_clear_to_cursor(void) {
	PrintConsole *con;
	int cur_row;
	
	if( !(con = currentConsole) ) return;
	cur_row = con->cursorY;

	__console_clear_line( cur_row, 1, con->cursorX );

	while( --cur_row )
		__console_clear_line( cur_row, 1, con->windowWidth + 1 );
}

static void __set_default_console(void *destbuffer,int xstart,int ystart, int tgt_stride, int con_xres,int con_yres,int con_stride)
{
	PrintConsole *con = &defaultConsole;

	con->destbuffer = destbuffer;
	con->target_x = xstart;
	con->target_y = ystart;
	con->con_xres = con_xres;
	con->con_yres = con_yres;
	con->tgt_stride = tgt_stride;
	con->con_stride = con_stride;
	con->con_cols = con->windowWidth = con_xres / FONT_XSIZE;
	con->con_rows = con->windowHeight = con_yres / FONT_YSIZE;
	con->windowX = 1;
	con->windowY = 1;
	con->cursorY = 1;
	con->cursorX = 1;
	con->prevCursorX = 1;
	con->prevCursorY = 1;

	con->font.gfx = console_font_8x16;
	con->font.asciiOffset = 0;
	con->font.numChars = 256;

	con->fg = CONSOLE_COLOR_WHITE;
	con->bg = CONSOLE_COLOR_BLACK;

	con->flags = 0;
	con->tabSize = 3;

	currentConsole = con;

	__console_clear();

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;

	setvbuf(stdout, NULL , _IONBF, 0);
	setvbuf(stderr, NULL , _IONBF, 0);

}

void __console_init(void *destbuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;

	_CPU_ISR_Disable(level);

	__set_default_console(destbuffer, xstart, ystart, stride, xres, yres, stride);

	_CPU_ISR_Restore(level);

}

void __console_init_ex(void *destbuffer,int tgt_xstart,int tgt_ystart,int tgt_stride,int con_xres,int con_yres,int con_stride)
{
	unsigned int level;

	_CPU_ISR_Disable(level);

	__set_default_console(destbuffer, tgt_xstart, tgt_ystart, tgt_stride, con_xres, con_yres, con_stride);

	VIDEO_SetPostRetraceCallback(__console_vipostcb);

	_CPU_ISR_Restore(level);

}

static void consoleClearLine(int mode) {

	switch(mode) {
	case 0: // cursor to end of line
		 __console_clear_line( currentConsole->cursorY, currentConsole->cursorX, currentConsole->windowWidth + 1 );
		break;
	case 1: // beginning of line to cursor
		__console_clear_line( currentConsole->cursorY, 1, currentConsole->cursorX );
		break;
	case 2: // entire line
		__console_clear_line( currentConsole->cursorY, 1, currentConsole->windowWidth + 1);
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
	if(x > currentConsole->windowWidth)
		x = currentConsole->windowWidth;
	if(y > currentConsole->windowHeight)
		y = currentConsole->windowHeight;

	currentConsole->cursorX = x;
	currentConsole->cursorY = y;
}

#define _ANSI_MAXARGS 16

static struct
{
	struct
	{
		int flags;
		u32 fg;
		u32 bg;
	} color;
	int argIdx;
	int args[_ANSI_MAXARGS];
	int colorArgCount;
	unsigned int colorArgs[3];
	bool hasArg;
	enum
	{
		ESC_NONE,
		ESC_START,
		ESC_BUILDING_UNKNOWN,
		ESC_BUILDING_FORMAT_FG,
		ESC_BUILDING_FORMAT_BG,
		ESC_BUILDING_FORMAT_FG_NONRGB,
		ESC_BUILDING_FORMAT_BG_NONRGB,
		ESC_BUILDING_FORMAT_FG_RGB,
		ESC_BUILDING_FORMAT_BG_RGB,
	} state;
} escapeSeq;

static void consoleSetColorState(int code)
{
	switch(code)
	{
	case 0: // reset
		escapeSeq.color.flags = 0;
		escapeSeq.color.bg    = 0;
		escapeSeq.color.fg    = 7;
		break;

	case 1: // bold
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		escapeSeq.color.flags |= CONSOLE_COLOR_BOLD;
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
		escapeSeq.colorArgCount = 0;
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
		escapeSeq.colorArgCount = 0;
		break;
	case 49: // reset background color
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.bg = 0;
		break;
	case 90 ... 97: // bright foreground
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		escapeSeq.color.flags |= CONSOLE_COLOR_FG_BRIGHT;
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.fg = code - 90;
		break;
	case 100 ... 107: // bright background
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		escapeSeq.color.flags |= CONSOLE_COLOR_BG_BRIGHT;
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.bg = code - 100;
		break;
	}
}

static void consoleHandleColorEsc(int argCount)
{
	escapeSeq.color.bg = currentConsole->bg;
	escapeSeq.color.fg = currentConsole->fg;
	escapeSeq.color.flags = currentConsole->flags;

	for (int arg = 0; arg < argCount; arg++)
	{
		int code = escapeSeq.args[arg];
		switch (escapeSeq.state)
		{
		case ESC_BUILDING_UNKNOWN:
			consoleSetColorState(code);
			break;
		case ESC_BUILDING_FORMAT_FG:
			if (code == 5)
				escapeSeq.state = ESC_BUILDING_FORMAT_FG_NONRGB;
			else if (code == 2)
				escapeSeq.state = ESC_BUILDING_FORMAT_FG_RGB;
			else
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_BG:
			if (code == 5)
				escapeSeq.state = ESC_BUILDING_FORMAT_BG_NONRGB;
			else if (code == 2)
				escapeSeq.state = ESC_BUILDING_FORMAT_BG_RGB;
			else
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
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
			escapeSeq.state = ESC_BUILDING_UNKNOWN;
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
			escapeSeq.state = ESC_BUILDING_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_FG_RGB:
			escapeSeq.colorArgs[escapeSeq.colorArgCount++] = code;
			if(escapeSeq.colorArgCount == 3)
			{
				escapeSeq.color.fg = single_RGB8_to_YCbCr(escapeSeq.colorArgs[0], escapeSeq.colorArgs[1], escapeSeq.colorArgs[2]);
				escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			}
			break;
		case ESC_BUILDING_FORMAT_BG_RGB:
			escapeSeq.colorArgs[escapeSeq.colorArgCount++] = code;
			if(escapeSeq.colorArgCount == 3)
			{
				escapeSeq.color.bg = single_RGB8_to_YCbCr(escapeSeq.colorArgs[0], escapeSeq.colorArgs[1], escapeSeq.colorArgs[2]);
				escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			}
		default:
			break;
		}
	}
	escapeSeq.argIdx = 0;

	currentConsole->bg = escapeSeq.color.bg;
	currentConsole->fg = escapeSeq.color.fg;
	currentConsole->flags = escapeSeq.color.flags;

}

void newRow()
{
	currentConsole->cursorY++;
	// if bottom border reached, scroll
	if( currentConsole->cursorY > currentConsole->windowHeight)
	{
		u8* ptr = __console_get_window_start_ptr();

		// Each window line is currentConsole->windowWidth * FONT_XSIZE * VI_DISPLAY_PIX_SZ bytes wide
		const u32 line_width = currentConsole->windowWidth * FONT_XSIZE * VI_DISPLAY_PIX_SZ;
		
		// scrolling copies all rows except the very first one (that gets overwritten)
		const u32 scroll_height = (currentConsole->windowHeight - 1) * FONT_YSIZE;
		// skip 1 tile row's worth of bytes
		const u32 row_bytes = currentConsole->con_stride * FONT_YSIZE;

		for(u32 ycnt = 0; ycnt < scroll_height; ycnt++)
		{
			memmove(ptr, ptr + row_bytes, line_width);
			ptr += currentConsole->con_stride;
		}

		// clear last line
		__console_clear_line(currentConsole->windowHeight, 1, currentConsole->windowWidth + 1);
		currentConsole->cursorY = currentConsole->windowHeight;
	}
}

void consolePrintChar(int c)
{
	int tabspaces;

	if (c==0) return;

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

			if(currentConsole->cursorX < 1) {
				if(currentConsole->cursorY > 1) {
					currentConsole->cursorX = currentConsole->windowWidth;
					currentConsole->cursorY--;
				} else {
					currentConsole->cursorX = 1;
				}
			}

			__console_drawc(' ');
			break;

		case 9:
			tabspaces = currentConsole->tabSize - ((currentConsole->cursorX - 1) % currentConsole->tabSize);
			if (currentConsole->cursorX + tabspaces > currentConsole->windowWidth)
				tabspaces = currentConsole->windowWidth - currentConsole->cursorX;
			for(int i=0; i < tabspaces; i++) consolePrintChar(' ');
			break;
		case 10:
			newRow();
		case 13:
			currentConsole->cursorX = 1;
			break;
		default:
			if(currentConsole->cursorX > currentConsole->windowWidth) {
				currentConsole->cursorX = 1;
				newRow();
			}
			__console_drawc(c);
			++currentConsole->cursorX;
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
				escapeSeq.hasArg = false;
				memset(escapeSeq.args, 0, sizeof(escapeSeq.args));
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
				escapeSeq.hasArg = true;
				escapeSeq.args[escapeSeq.argIdx] = escapeSeq.args[escapeSeq.argIdx] * 10 + (chr - '0');
				break;
			case ';':
				if (escapeSeq.hasArg) {
					if (escapeSeq.argIdx < _ANSI_MAXARGS) {
						escapeSeq.argIdx++;
					}
				}
				escapeSeq.hasArg = false;
				break;
			//---------------------------------------			// Cursor directional movement
			//---------------------------------------
			case 'A':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorY  =  currentConsole->cursorY - escapeSeq.args[0];
				if (currentConsole->cursorY < 1)
					currentConsole->cursorY = 1;
				escapeSeq.state = ESC_NONE;
				break;
			case 'B':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorY  =  currentConsole->cursorY + escapeSeq.args[0];
				if (currentConsole->cursorY > currentConsole->windowHeight)
					currentConsole->cursorY = currentConsole->windowHeight;
				escapeSeq.state = ESC_NONE;
				break;
			case 'C':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorX  =  currentConsole->cursorX  + escapeSeq.args[0];
				if (currentConsole->cursorX > currentConsole->windowWidth)
					currentConsole->cursorX = currentConsole->windowWidth;
				escapeSeq.state = ESC_NONE;
				break;
			case 'D':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorX  =  currentConsole->cursorX  - escapeSeq.args[0];
				if (currentConsole->cursorX < 1)
					currentConsole->cursorX = 1;
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Cursor position movement
			//---------------------------------------
			case 'H':
			case 'f':
				consolePosition(escapeSeq.args[1], escapeSeq.args[0]);
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Screen clear
			//---------------------------------------
			case 'J':
				if (escapeSeq.argIdx == 0 && !escapeSeq.hasArg) {
					escapeSeq.args[0] = 0;
				}
				consoleCls(escapeSeq.args[0]);
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Line clear
			//---------------------------------------
			case 'K':
				if (escapeSeq.argIdx == 0 && !escapeSeq.hasArg) {
					escapeSeq.args[0] = 0;
				}
				consoleClearLine(escapeSeq.args[0]);
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Save cursor position
			//---------------------------------------
			case 's':
				currentConsole->prevCursorX = currentConsole->cursorX ;
				currentConsole->prevCursorY = currentConsole->cursorY ;
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Load cursor position
			//---------------------------------------
			case 'u':
				currentConsole->cursorX = currentConsole->prevCursorX ;
				currentConsole->cursorY = currentConsole->prevCursorY ;
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Color scan codes
			//---------------------------------------
			case 'm':
				if (escapeSeq.argIdx == 0 && !escapeSeq.hasArg) escapeSeq.args[escapeSeq.argIdx++] = 0;
				if (escapeSeq.hasArg) escapeSeq.argIdx++;
				consoleHandleColorEsc(escapeSeq.argIdx);
				escapeSeq.state = ESC_NONE;
				break;
			default:
				// some sort of unsupported escape; just gloss over it
				escapeSeq.state = ESC_NONE;
				break;
			}
		default:
			break;
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

void consoleClear(void)
{
	if(!currentConsole)
		return;

	__console_clear();
}

PrintConsole* consoleGetDefault(void){return &defaultConsole;}

PrintConsole* consoleInit(PrintConsole* console)
{

	static bool firstConsoleInit = true;

	if(firstConsoleInit) {

		GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
		void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		console_init(xfb,0,0,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
		VIDEO_Configure(rmode);
		VIDEO_SetNextFramebuffer(xfb);
		VIDEO_SetBlack(FALSE);
		VIDEO_Flush();
		VIDEO_WaitVSync();
		if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

		devoptab_list[STD_OUT] = &dotab_stdout;
		devoptab_list[STD_ERR] = &dotab_stdout;

		setvbuf(stdout, NULL , _IONBF, 0);
		setvbuf(stderr, NULL , _IONBF, 0);

		firstConsoleInit = false;
	}

	if(console) {
		currentConsole = console;
	} else {
		console = currentConsole;
	}

	*currentConsole = defaultConsole;

	console->consoleInitialised = 1;

	consoleCls(2);

	return currentConsole;

}

PrintConsole *consoleSelect(PrintConsole* console)
{
	PrintConsole *tmp = currentConsole;
	currentConsole = console;
	return tmp;
}

void consoleSetFont(PrintConsole* console, ConsoleFont* font)
{
	if(!console) console = currentConsole;

	console->font = *font;
}

void consoleSetWindow(PrintConsole* console, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	if(!console) console = currentConsole;

	// co-ords are 1-based but accept 0
	if (x == 0) x = 1;
	if (y == 0) y = 1;

	if (x >= console->con_cols) return;
	if (y >= console->con_rows) return;

	if (x + width > console->con_cols + 1) width = console->con_cols + 1 - x;
	if (y + height > console->con_rows + 1) height = console->con_rows + 1 - y;

	console->windowWidth = width;
	console->windowHeight = height;
	console->windowX = x;
	console->windowY = y;

	console->cursorX = 1;
	console->cursorY = 1;
}
