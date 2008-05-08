/*-------------------------------------------------------------

conf.h -- SYSCONF support

Copyright (C) 2008
Hector Martin (marcan)

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

#ifndef __CONF_H__
#define __CONF_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <gcutil.h>

#define CONF_EBADFILE	-0x6001
#define CONF_ENOENT		-0x6002
#define CONF_ETOOBIG	-0x6003
#define CONF_ENOTINIT	-0x6004
#define CONF_ENOTIMPL	-0x6005
#define CONF_EBADVALUE	-0x6006
#define CONF_ENOMEM		-0x6007

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

enum {
	CONF_BIGARRAY = 1,
	CONF_SMALLARRAY,
	CONF_BYTE,
	CONF_SHORT,
	CONF_LONG,
	CONF_BOOL = 7
};

typedef struct _conf_pad_device conf_pad_device;

struct _conf_pad_device {
	u8 bdaddr[6];
	char name[0x40];
} ATTRIBUTE_PACKED;

s32 CONF_Init(void);
s32 CONF_GetLength(const char *name);
s32 CONF_GetType(const char *name);
s32 CONF_Get(const char *name, void *buffer, u32 length);
s32 CONF_GetShutdownMode(void);
s32 CONF_GetIdleLedMode(void);
s32 CONF_GetProgressiveScan(void);
s32 CONF_GetEuRGB60(void);
s32 CONF_GetIRSensitivity(void);
s32 CONF_GetSensorBarPosition(void);
s32 CONF_GetPadSpeakerVolume(void);
s32 CONF_GetPadMotorMode(void);
s32 CONF_GetSoundMode(void);
s32 CONF_GetLanguage(void);
s32 CONF_GetCounterBias(void);
s32 CONF_GetScreenSaverMode(void);
s32 CONF_GetDisplayOffsetH(s8 *offset);
s32 CONF_GetPadDevices(conf_pad_device *devs, int count);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif
