#ifndef __CONSOLE_INTERNAL_H__
#define __CONSOLE_INTERNAL_H__

#define CONSOLE_COLOR_BOLD    (1<<0)  ///< Bold text
#define CONSOLE_COLOR_FAINT   (1<<1)  ///< Faint text
#define CONSOLE_ITALIC        (1<<2)  ///< Italic text
#define CONSOLE_UNDERLINE     (1<<3)  ///< Underlined text
#define CONSOLE_BLINK_SLOW    (1<<4)  ///< Slow blinking text
#define CONSOLE_BLINK_FAST    (1<<5)  ///< Fast blinking text
#define CONSOLE_COLOR_REVERSE (1<<6)  ///< Reversed color text
#define CONSOLE_CONCEAL       (1<<7)  ///< Concealed text
#define CONSOLE_CROSSED_OUT   (1<<8)  ///< Crossed out text
#define CONSOLE_FG_CUSTOM     (1<<9)  ///< Foreground custom color
#define CONSOLE_BG_CUSTOM     (1<<10) ///< Background custom color

#define FONT_XSIZE		8
#define FONT_YSIZE		16
#define FONT_XFACTOR	1
#define FONT_YFACTOR	1
#define FONT_XGAP			0
#define FONT_YGAP			0
#define TAB_SIZE			4

typedef struct _console_data_s {
	void *destbuffer;
	unsigned char *font;
	int con_xres,con_yres,con_stride;
	int target_x,target_y, tgt_stride;
	int cursorX,cursorY;
	int prevCursorX,prevCursorY;
	int con_rows, con_cols;
	int tabSize;
	unsigned int fg,bg;
	unsigned int flags;
} console_data_s;

extern int __console_write(struct _reent *r,void *fd,const char *ptr,size_t len);
extern void __console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride);

//extern const devoptab_t dotab_stdout;

#endif
