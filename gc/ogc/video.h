#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <gctypes.h>
#include "gx_struct.h"

/*+----------------------------------------------------------------------------------------------+*/
#define VIDEO_640X480_NTSC_YUV16     (0)  ///< Helper define. Can be used with VIDEO_Init()
#define VIDEO_640X480_PAL50_YUV16    (1)  ///< Helper define. Can be used with VIDEO_Init()
#define VIDEO_640X480_PAL60_YUV16    (2)  ///< Helper define. Can be used with VIDEO_Init()
#define VIDEO_AUTODETECT			 (3)  ///< Helper define. Can be used with VIDEO_Init()
/*+----------------------------------------------------------------------------------------------+*/
#define VIDEO_FRAMEBUFFER_1          (1)  ///< Helper define. Can be used with VIDEO_SetFrameBuffer()
#define VIDEO_FRAMEBUFFER_2          (2)  ///< Helper define. Can be used with VIDEO_SetFrameBuffer()
#define VIDEO_FRAMEBUFFER_BOTH       (0)  ///< Helper define. Can be used with VIDEO_SetFrameBuffer()
/*+----------------------------------------------------------------------------------------------+*/
#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */
/*+----------------------------------------------------------------------------------------------+*/
extern GXRModeObj TVPal524IntAa;
extern GXRModeObj TVPal528IntDf;
extern GXRModeObj TVPal574IntDfScale;

typedef void (*VIRetraceCallback)(u32);

void VIDEO_Init(u32 VideoMode);
void VIDEO_SetFrameBuffer(u32 which, void *fb);
void VIDEO_SetBlack(bool btrue);
void VIDEO_ClearFrameBuffer(u32 FrameBufferAddr,u32 nSize,u32 color);
void VIDEO_WaitVSync(void);
void VIDEO_SetNextFramebuffer(void *fb);
u32 VIDEO_GetCurrentMode();
u32 VIDEO_GetYCbCr(u8 r,u8 g,u8 b);
VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback);
VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback);
/*+----------------------------------------------------------------------------------------------+*/

#ifdef __cplusplus
   }
#endif /* __cplusplus */
/*+----------------------------------------------------------------------------------------------+*/

#endif
