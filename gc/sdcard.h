#ifndef __SDCARD_H__
#define __SDCARD_H__

#include <gctypes.h>

#define SDCARD_ERROR_READY				 0
#define SDCARD_ERROR_BUSY				-1
#define SDCARD_ERROR_NOCARD				-3
#define SDCARD_ERROR_IOERROR			-5
#define SDCARD_ERROR_FATALERROR			-128

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s32 SDCARD_Reset(s32 chn);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
