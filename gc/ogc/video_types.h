#ifndef __VIDEO_TYPES_H__
#define __VIDEO_TYPES_H__

#include <gctypes.h>

#define VI_DISPLAY_PIX_SZ           2

#define VI_INTERLACE                0
#define VI_NON_INTERLACE            1
#define VI_PROGRESSIVE              2

#define VI_NTSC                     0
#define VI_PAL                      1
#define VI_MPAL                     2
#define VI_DEBUG                    3
#define VI_DEBUG_PAL                4
#define VI_EURGB60                  5

#define VI_TVMODE(fmt, mode)   ( ((fmt) << 2) + (mode) )

#define VI_TVMODE_NTSC_INT			VI_TVMODE(VI_NTSC,        VI_INTERLACE)
#define VI_TVMODE_NTSC_DS			VI_TVMODE(VI_NTSC,        VI_NON_INTERLACE)
#define VI_TVMODE_NTSC_PROG			VI_TVMODE(VI_NTSC,        VI_PROGRESSIVE)

#define VI_TVMODE_PAL_INT			VI_TVMODE(VI_PAL,         VI_INTERLACE)
#define VI_TVMODE_PAL_DS			VI_TVMODE(VI_PAL,         VI_NON_INTERLACE)

#define VI_TVMODE_EURGB60_INT		VI_TVMODE(VI_EURGB60,     VI_INTERLACE)
#define VI_TVMODE_EURGB60_DS		VI_TVMODE(VI_EURGB60,     VI_NON_INTERLACE)

#define VI_TVMODE_MPAL_INT			VI_TVMODE(VI_MPAL,        VI_INTERLACE)
#define VI_TVMODE_MPAL_DS			VI_TVMODE(VI_MPAL,        VI_NON_INTERLACE)

#define VI_TVMODE_DEBUG_INT			VI_TVMODE(VI_DEBUG,       VI_INTERLACE)

#define VI_TVMODE_DEBUG_PAL_INT		VI_TVMODE(VI_DEBUG_PAL,   VI_INTERLACE)
#define VI_TVMODE_DEBUG_PAL_DS		VI_TVMODE(VI_DEBUG_PAL,   VI_NON_INTERLACE)

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

#endif
