/*-------------------------------------------------------------

video_types.h -- support header

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/


#ifndef __VIDEO_TYPES_H__
#define __VIDEO_TYPES_H__

/*!
\file video_types.h
\brief support header
*/

#include <gctypes.h>

#define VI_DISPLAY_PIX_SZ           2		/*!< multiplier to get real pixel size in bytes */

/*!
 * \addtogroup vi_modetypedef VIDEO mode type definition
 * @{
 */

#define VI_INTERLACE                0		/*!< Video mode INTERLACED. */
#define VI_NON_INTERLACE            1		/*!< Video mode NON INTERLACED */	
#define VI_PROGRESSIVE              2		/*!< Video mode PROGRESSIVE. Special mode for higher quality */

/*!
 * @}
 */


/*!
 * \addtogroup vi_standardtypedef VIDEO standard type definition
 * @{
 */

#define VI_NTSC                     0		/*!< Video standard used in North America and Japan */
#define VI_PAL                      1		/*!< Video standard used in Europe */
#define VI_MPAL                     2		/*!< Video standard, similar to NTSC, used in Brazil */
#define VI_DEBUG                    3		/*!< Video standard, for debugging purpose, used in North America and Japan. Special decoder needed */
#define VI_DEBUG_PAL                4		/*!< Video standard, for debugging purpose, used in Europe. Special decoder needed */
#define VI_EURGB60                  5		/*!< RGB 60Hz, 480 lines mode (same timing and aspect ratio as NTSC) used in Europe */

/*!
 * @}
 */


#define VI_XFBMODE_SF				0
#define VI_XFBMODE_DF				1

#define VI_FIELD_ABOVE              1
#define VI_FIELD_BELOW              0

// Maximum screen space
#define VI_MAX_WIDTH_NTSC           720
#define VI_MAX_HEIGHT_NTSC          480

#define VI_MAX_WIDTH_PAL            720
#define VI_MAX_HEIGHT_PAL           574

#define VI_MAX_WIDTH_MPAL           720
#define VI_MAX_HEIGHT_MPAL          480

#define VI_MAX_WIDTH_EURGB60        VI_MAX_WIDTH_NTSC
#define VI_MAX_HEIGHT_EURGB60       VI_MAX_HEIGHT_NTSC

#define VI_TVMODE(fmt, mode)   ( ((fmt) << 2) + (mode) )

#define VI_TVMODE_NTSC_INT			VI_TVMODE(VI_NTSC,        VI_INTERLACE)
#define VI_TVMODE_NTSC_DS			VI_TVMODE(VI_NTSC,        VI_NON_INTERLACE)
#define VI_TVMODE_NTSC_PROG			VI_TVMODE(VI_NTSC,        VI_PROGRESSIVE)
#define VI_TVMODE_NTSC_PROG_DS		VI_TVMODE(VI_NTSC,        (VI_PROGRESSIVE|VI_NON_INTERLACE))

#define VI_TVMODE_PAL_INT			VI_TVMODE(VI_PAL,         VI_INTERLACE)
#define VI_TVMODE_PAL_DS			VI_TVMODE(VI_PAL,         VI_NON_INTERLACE)

#define VI_TVMODE_EURGB60_INT		VI_TVMODE(VI_EURGB60,     VI_INTERLACE)
#define VI_TVMODE_EURGB60_DS		VI_TVMODE(VI_EURGB60,     VI_NON_INTERLACE)

#define VI_TVMODE_MPAL_INT			VI_TVMODE(VI_MPAL,        VI_INTERLACE)
#define VI_TVMODE_MPAL_DS			VI_TVMODE(VI_MPAL,        VI_NON_INTERLACE)

#define VI_TVMODE_DEBUG_INT			VI_TVMODE(VI_DEBUG,       VI_INTERLACE)

#define VI_TVMODE_DEBUG_PAL_INT		VI_TVMODE(VI_DEBUG_PAL,   VI_INTERLACE)
#define VI_TVMODE_DEBUG_PAL_DS		VI_TVMODE(VI_DEBUG_PAL,   VI_NON_INTERLACE)

#endif
