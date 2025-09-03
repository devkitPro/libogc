#ifndef __LIBOGC_CONSOLE_H__
#define __LIBOGC_CONSOLE_H__

/**
 * @file console.h
 * @brief ogc stdio support.
 *
 * Provides stdio integration for printing to the framebuffer as well as debug print
 * functionality provided by stderr.
 *
 */

#include "gx_struct.h"

/* macros to support old function names */
#define console_init     CON_Init
#define SYS_ConsoleInit  CON_InitEx

#define CONSOLE_ESC(x) "\x1b[" #x
#define CONSOLE_RESET   CONSOLE_ESC(0m)

#define CONSOLE_BLACK   CONSOLE_ESC(30m)
#define CONSOLE_RED     CONSOLE_ESC(31;1m)
#define CONSOLE_GREEN   CONSOLE_ESC(32;1m)
#define CONSOLE_YELLOW  CONSOLE_ESC(33;1m)
#define CONSOLE_BLUE    CONSOLE_ESC(34;1m)
#define CONSOLE_MAGENTA CONSOLE_ESC(35;1m)
#define CONSOLE_CYAN    CONSOLE_ESC(36;1m)
#define CONSOLE_WHITE   CONSOLE_ESC(37;1m)

#define CONSOLE_BG_BLACK   CONSOLE_ESC(40m)
#define CONSOLE_BG_RED     CONSOLE_ESC(41;1m)
#define CONSOLE_BG_GREEN   CONSOLE_ESC(42;1m)
#define CONSOLE_BG_YELLOW  CONSOLE_ESC(43;1m)
#define CONSOLE_BG_BLUE    CONSOLE_ESC(44;1m)
#define CONSOLE_BG_MAGENTA CONSOLE_ESC(45;1m)
#define CONSOLE_BG_CYAN    CONSOLE_ESC(46;1m)
#define CONSOLE_BG_WHITE   CONSOLE_ESC(47;1m)

#define CONSOLE_COLOR_BLACK   0
#define CONSOLE_COLOR_RED     1
#define CONSOLE_COLOR_GREEN   2
#define CONSOLE_COLOR_YELLOW  3
#define CONSOLE_COLOR_BLUE    4
#define CONSOLE_COLOR_MAGENTA 5
#define CONSOLE_COLOR_CYAN    6
#define CONSOLE_COLOR_WHITE   7

#ifdef __cplusplus
	extern "C" {
#endif

/*!
 * \fn CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
 * \brief Initializes the console subsystem with given parameters
 *
 * \param[in] framebuffer pointer to the framebuffer used for drawing the characters
 * \param[in] xstart,ystart start position of the console output in pixels
 * \param[in] xres,yres size of the console in pixels. Truncated to a multiple of font size if not already one
 * \param[in] stride size of one line of the framebuffer in bytes
 *
 * \return none
 */
void CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride);

/*!
 * \fn s32 CON_InitEx(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight)
 * \brief Initialize stdout console
 * \param[in] rmode pointer to the video/render mode configuration
 * \param[in] conXOrigin starting pixel in X direction of the console output on the external framebuffer
 * \param[in] conYOrigin starting pixel in Y direction of the console output on the external framebuffer
 * \param[in] conWidth width of the console output 'window' to be drawn in pixels. Truncated to a multiple of font width if not already one
 * \param[in] conHeight height of the console output 'window' to be drawn in pixels. Truncated to a multiple of font height if not already one
 *
 * \return 0 on success, <0 on error
 */
s32 CON_InitEx(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight);

/*!
 * \fn CON_GetMetrics(int *cols, int *rows)
 * \brief retrieve the columns and rows of the current console
 *
 * \param[out] cols,rows number of columns and rows of the current console in tiles
 *
 * \return none
 */
void CON_GetMetrics(int *cols, int *rows);

/*!
 * \fn CON_GetPosition(int *col, int *row)
 * \brief retrieve the current cursor position of the current console
 *
 * \param[out] col,row current cursor position in tiles, 1-indexed
 *
 * \return none
 */
void CON_GetPosition(int *cols, int *rows);

/*!
 * \fn CON_EnableGecko(int channel, int safe)
 * \brief Enable or disable the USB gecko console.
 *
 * \param[in] channel EXI channel, or -1 to disable the gecko console
 * \param[in] safe If true, use safe mode (wait for peer)
 *
 * \return none
 */
void CON_EnableGecko(int channel,int safe);

/// A callback for printing a character.
typedef bool(*ConsolePrint)(void* con, int c);

/// A font struct for the console.
typedef struct ConsoleFont
{
	u8* gfx;         ///< A pointer to the font graphics
	u16 asciiOffset; ///< Offset to the first valid character in the font table
	u16 numChars;    ///< Number of characters in the font graphics
}ConsoleFont;

/**
 * @brief Console structure used to store the state of a console render context.
 *
 * Default values from consoleGetDefault();
 * @code
 * PrintConsole defaultConsole =
 * {
 * 	//Font:
 * 	{
 * 		(u8*)default_font_bin, //font gfx
 * 		0, //first ascii character in the set
 * 		128, //number of characters in the font set
 *	},
 *	0,0, //cursorX cursorY
 *	0,0, //prevcursorX prevcursorY
 *	40, //console width
 *	30, //console height
 *	0,  //window x
 *	0,  //window y
 *	32, //window width
 *	24, //window height
 *	3, //tab size
 *	0, //font character offset
 *	0,  //print callback
 *	false //console initialized
 * };
 * @endcode
 */

typedef struct PrintConsole
{
	ConsoleFont font;        ///< Font of the console

	void *destbuffer;        ///< Framebuffer address
	int con_xres, con_yres;  ///< Console buffer width/height in pixels
	int con_stride;          ///< Size of one row in the console buffer in bytes
	int target_x, target_y;  ///< Target buffer x/y offset to start the console in pixels
	int tgt_stride;          ///< Size of one row in the target buffer in bytes

	int cursorX;             ///< Current X location of the cursor in the window in tiles, 1-indexed: 1 <= cursorX <= windowWidth, cursorX > windowWidth wraps to the next line and resets to cursorX = 1
	int cursorY;             ///< Current Y location of the cursor in the window in tiles, 1-indexed

	int prevCursorX;         ///< Internal state
	int prevCursorY;         ///< Internal state

	int con_cols;            ///< Width of the console hardware layer in characters (aka tiles)
	int con_rows;            ///< Height of the console hardware layer in characters (aka tiles)

	int windowX;             ///< Window X location in tiles, 1-indexed, 1 <= windowX < con_cols
	int windowY;             ///< Window Y location in tiles, 1-indexed
	int windowWidth;         ///< Window width in tiles, 1 <= windowWidth <= con_cols
	int windowHeight;        ///< Window height in tiles

	int tabSize;             ///< Size of a tab
	unsigned int fg;         ///< Foreground color
	unsigned int bg;         ///< Background color
	unsigned int flags;      ///< Attribute flags

	ConsolePrint PrintChar;  ///< Callback for printing a character. Should return true if it has handled rendering the graphics (else the print engine will attempt to render via tiles).

	bool consoleInitialised; ///< True if the console is initialized
} PrintConsole;

/// Console debug devices supported by libogc.
typedef enum {
	debugDevice_NULL,    ///< Swallows prints to stderr
	debugDevice_EXI,     ///< Outputs stderr debug statements using exi uart
	debugDevice_CONSOLE, ///< Directs stderr debug statements to console window
} debugDevice;

/**
 * @brief Loads the font into the console.
 * @param console Pointer to the console to update, if NULL it will update the current console.
 * @param font The font to load.
 */
void consoleSetFont(PrintConsole* console, ConsoleFont* font);

/**
 * @brief Sets the print window.
 * @param console Console to set, if NULL it will set the current console window.
 * @param x X location of the window in tiles, 1-indexed
 * @param y Y location of the window in tiles, 1-indexed
 * @param width Width of the window in tiles
 * @param height Height of the window in tiles
 */
void consoleSetWindow(PrintConsole* console, unsigned int x, unsigned int y, unsigned int width, unsigned int height);

/**
 * @brief Gets a pointer to the console with the default values.
 * This should only be used when using a single console or without changing the console that is returned, otherwise use consoleInit().
 * @return A pointer to the console with the default values.
 */
PrintConsole* consoleGetDefault(void);

/**
 * @brief Make the specified console the render target.
 * @param console A pointer to the console struct (must have been initialized with consoleInit(PrintConsole* console)).
 * @return A pointer to the previous console.
 */
PrintConsole *consoleSelect(PrintConsole* console);

/**
 * @brief Initialise the console.
 * @param console A pointer to the console data to initialize (if it's NULL, the default console will be used).
 * @return A pointer to the current console.
 */
PrintConsole* consoleInit(PrintConsole* console);

/**
 * @brief Initializes debug console output on stderr to the specified device.
 * @param device The debug device (or devices) to output debug print statements to.
 */
void consoleDebugInit(debugDevice device);

/// Clears the screen by using iprintf("\x1b[2J");
void consoleClear(void);

#ifdef __cplusplus
}
#endif
#endif
