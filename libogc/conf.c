/*-------------------------------------------------------------

conf.c -- SYSCONF support

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

#if defined(HW_RVL)

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "ipc.h"
#include "asm.h"
#include "processor.h"
#include "conf.h"


static int __conf_inited = 0;
static u8 __conf_buffer[0x4000] ATTRIBUTE_ALIGN(32);

static const char __conf_file[] ATTRIBUTE_ALIGN(32) = "/shared2/sys/SYSCONF";

s32 CONF_Init(void)
{
	int fd;
	int ret;
	
	if(__conf_inited) return 0;
	
	fd = IOS_Open(__conf_file,1);
	if(fd < 0) return fd;
	
	memset(__conf_buffer,0,0x4000);
	
	ret = IOS_Read(fd, __conf_buffer, 0x4000);
	IOS_Close(fd);
	if(ret != 0x4000) return CONF_EBADFILE;
		
	if(memcmp(__conf_buffer, "SCv0", 4)) return CONF_EBADFILE;
	__conf_inited = 1;
	return 0;
}

u8 *__CONF_Find(const char *name)
{
	u16 count;
	u16 *offset;
	int nlen = strlen(name);
	count = *((u16*)(&__conf_buffer[4]));
	offset = (u16*)&__conf_buffer[6];
	
	while(count--) {
		if((nlen == ((__conf_buffer[*offset]&0x0F)+1)) && !memcmp(name, &__conf_buffer[*offset+1], nlen))
			return &__conf_buffer[*offset];
		offset++;
	}
	return NULL;
}

s32 CONF_GetLength(const char *name)
{
	u8 *entry;

	if(!__conf_inited) return CONF_ENOTINIT;
	
	entry = __CONF_Find(name);
	if(!entry) return CONF_ENOENT;
	
	switch(*entry>>5) {
		case 1:
			return *((u16*)&entry[strlen(name)+1]) + 1;
		case 2:
			return entry[strlen(name)+1] + 1;
		case 3:
			return 1;
		case 4:
			return 2;
		case 5:
			return 4;
		case 7:
			return 1;
		default:
			return CONF_ENOTIMPL;	
	}
}

int CONF_GetType(const char *name) 
{
	u8 *entry;
	if(!__conf_inited) return CONF_ENOTINIT;
	
	entry = __CONF_Find(name);
	if(!entry) return CONF_ENOENT;
	
	return *entry>>5;
}

s32 CONF_Get(const char *name, void *buffer, u32 length)
{
	u8 *entry;
	s32 len;
	if(!__conf_inited) return CONF_ENOTINIT;
	
	entry = __CONF_Find(name);
	if(!entry) return CONF_ENOENT;
	
	len = CONF_GetLength(name);
	if(len<0) return len;
	if(len>length) return CONF_ETOOBIG;
	
	switch(*entry>>5) {
		case CONF_BIGARRAY:
			memcpy(buffer, &entry[strlen(name)+3], len);
			break;
		case CONF_SMALLARRAY:
			memcpy(buffer, &entry[strlen(name)+2], len);
			break;
		case CONF_BYTE:
		case CONF_SHORT:
		case CONF_LONG:
		case CONF_BOOL:
			memset(buffer, 0, length);
			memcpy(buffer, &entry[strlen(name)+1], len);
			break;
		default:
			return CONF_ENOTIMPL;
	}
	return len;
}

s32 CONF_GetShutdownMode(void) 
{
	u8 idleconf[2] = {0,0};
	int res;
	
	res = CONF_Get("IPL.IDL", idleconf, 2);
	if(res<0) return res;
	if(res!=2) return CONF_EBADVALUE;
	return idleconf[0];
}

s32 CONF_GetIdleLedMode(void) 
{
	int res;
	u8 idleconf[2] = {0,0};
	res = CONF_Get("IPL.IDL", idleconf, 2);
	if(res<0) return res;
	if(res!=2) return CONF_EBADVALUE;
	return idleconf[1];
}

s32 CONF_GetProgressiveScan(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("IPL.PGS", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetEuRGB60(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("IPL.E60", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetIRSensitivity(void) 
{
	int res;
	u32 val = 0;
	res = CONF_Get("BT.SENS", &val, 4);
	if(res<0) return res;
	if(res!=4) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetSensorBarPosition(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("BT.BAR", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetPadSpeakerVolume(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("BT.SPKV", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetPadMotorMode(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("BT.MOT", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetSoundMode(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("IPL.SND", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetLanguage(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("IPL.LNG", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetCounterBias(void) 
{
	int res;
	u32 val = 0;
	res = CONF_Get("IPL.CB", &val, 4);
	if(res<0) return res;
	if(res!=4) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetScreenSaverMode(void) 
{
	int res;
	u8 val = 0;
	res = CONF_Get("IPL.SSV", &val, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return val;
}

s32 CONF_GetDisplayOffsetH(s8 *offset) 
{
	int res;
	res = CONF_Get("IPL.SSV", offset, 1);
	if(res<0) return res;
	if(res!=1) return CONF_EBADVALUE;
	return 0;
}

s32 CONF_GetPadDevices(conf_pad_device *devs, int count) 
{
	int res;
	u8 buf[0x461];
	
	res = CONF_Get("BT.DINF", buf, 0x461);
	if(res < 0) return res;
	if((res < 1) || (buf[0] > 0x10)) return CONF_EBADVALUE;

	if(count && devs) {
		memset(devs,0,count*sizeof(conf_pad_device));
		if(count < buf[0])
			memcpy(devs,&buf[1],count*sizeof(conf_pad_device));
		else
			memcpy(devs,&buf[1],buf[0]*sizeof(conf_pad_device));
	}

	res = buf[0];
	return res;
}

#endif
