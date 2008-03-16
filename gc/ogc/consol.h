#ifndef __CONSOL_H__
#define __CONSOL_H__

/*!
 * \file consol.h
 * \brief Console subsystem
 *
 */

/* macros to support old function names */
#define console_init     CON_Init

#ifdef __cplusplus
	extern "C" {
#endif

/*!
 * \fn CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
 * \brief Initialises the console subsystem with given parameters
 *
 * \param[in] framebuffer pointer to the framebuffer used for drawing the characters
 * \param[in] xstart,ystart start position of the console output in pixel
 * \param[in] xres,yres size of the console in pixel
 * \param[in] stride size of one line of the framebuffer in bytes
 *
 * \return none
 */
void CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride);

/*!
 * \fn CON_GetMetrics(int *cols, int *rows)
 * \brief retrieve the columns and rows of the current console
 *
 * \param[out] cols,rows number of columns and rows of the current console
 *
 * \return none
 */
void CON_GetMetrics(int *cols, int *rows);

#ifdef __cplusplus
	}
#endif

#endif
