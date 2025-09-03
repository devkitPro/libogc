/*-------------------------------------------------------------

stm.c - System and miscellaneous hardware control functions

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
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

#include <string.h>
#include "ipc.h"
#include "system.h"
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "stm.h"

//#define DEBUG_STM
#ifdef DEBUG_STM
#include <stdio.h>
#define STM_printf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS)
#else
#define STM_printf(...) while (0) {}
#endif

#define IOCTL_STM_EVENTHOOK			0x1000
#define IOCTL_STM_GET_IDLEMODE		0x3001
#define IOCTL_STM_RELEASE_EH		0x3002
#define IOCTL_STM_HOTRESET			0x2001
#define IOCTL_STM_HOTRESET_FOR_PD	0x2002
#define IOCTL_STM_SHUTDOWN			0x2003
#define IOCTL_STM_IDLE				0x2004
#define IOCTL_STM_WAKEUP			0x2005
#define IOCTL_STM_VIDIMMING			0x5001
#define IOCTL_STM_LEDFLASH			0x6001
#define IOCTL_STM_LEDMODE			0x6002
#define IOCTL_STM_READVER			0x7001
#define IOCTL_STM_READDDRREG		0x4001
#define IOCTL_STM_READDDRREG2		0x4002

static s32 __stm_eh_fd = -1;
static s32 __stm_imm_fd = -1;
static u32 __stm_vdinuse = 0;
static u32 __stm_initialized= 0;
static u32 __stm_ehregistered= 0;
static u32 __stm_ehclear= 0;

static u32 __stm_ehbufin[0x08] ATTRIBUTE_ALIGN(32) = {0,0,0,0,0,0,0,0};
static u32 __stm_ehbufout[0x08] ATTRIBUTE_ALIGN(32) = {0,0,0,0,0,0,0,0};

static char __stm_eh_fs[] ATTRIBUTE_ALIGN(32) = "/dev/stm/eventhook";
static char __stm_imm_fs[] ATTRIBUTE_ALIGN(32) = "/dev/stm/immediate";

s32 __STM_SetEventHook(void);
s32 __STM_ReleaseEventHook(void);
static s32 __STMEventHandler(s32 result,void *usrdata);

stmcallback __stm_eventcb = NULL;

static vu16* const _viReg = (u16*)0xCC002000;

s32 __STM_Init(void)
{
	if(__stm_initialized==1) return 1;

	STM_printf("STM init");

	__stm_vdinuse = 0;
	__stm_imm_fd = IOS_Open(__stm_imm_fs,0);
	if(__stm_imm_fd<0) return 0;

	__stm_eh_fd = IOS_Open(__stm_eh_fs,0);
	if(__stm_eh_fd<0) return 0;
	
	STM_printf("STM FDs: %d, %d\n",__stm_imm_fd, __stm_eh_fd);
	
	__stm_initialized = 1;
	__STM_SetEventHook();
	return 1;
}

s32 __STM_Close(void)
{
	s32 res;
	s32 ret = 0;
	__STM_ReleaseEventHook();
	
	if(__stm_imm_fd >= 0) {
		res = IOS_Close(__stm_imm_fd);
		if(res < 0) ret = res;
		__stm_imm_fd = -1;
	}
	if(__stm_eh_fd >= 0) {
		res = IOS_Close(__stm_eh_fd);
		if(res < 0) ret = res;
		__stm_eh_fd = -1;
	}
	__stm_initialized = 0;
	return ret;
}

s32 __STM_SendCommand(s32 ioctl, const void *inbuf, u32 inlen, void *outbuf, u32 outlen) {
	if (__stm_initialized == 0)
		return STM_ENOTINIT;

	return IOS_Ioctl(__stm_imm_fd, ioctl, (void *)inbuf, inlen, outbuf, outlen);
}

s32 __STM_SetEventHook(void)
{
	s32 ret;
	u32 level;

	if(__stm_initialized==0) return STM_ENOTINIT;
	
	__stm_ehclear = 0;
	
	_CPU_ISR_Disable(level);
	ret = IOS_IoctlAsync(__stm_eh_fd,IOCTL_STM_EVENTHOOK,__stm_ehbufin,0x20,__stm_ehbufout,0x20,__STMEventHandler,NULL);
	if(ret<0) __stm_ehregistered = 0;
	else __stm_ehregistered = 1;
	_CPU_ISR_Restore(level);

	return ret;
}

s32 __STM_ReleaseEventHook(void)
{
	s32 ret;

	if(__stm_initialized==0) return STM_ENOTINIT;
	if(__stm_ehregistered==0) return STM_ENOHANDLER;

	__stm_ehclear = 1;
	
	ret = __STM_SendCommand(IOCTL_STM_RELEASE_EH,NULL,0,NULL,0);
	if(ret>=0) __stm_ehregistered = 0;

	return ret;
}

static s32 __STMEventHandler(s32 result,void *usrdata)
{
	__stm_ehregistered = 0;
	
	if(result < 0) { // shouldn't happen
		return result;
	}
	
	STM_printf("STM Event: %08x\n",__stm_ehbufout[0]);

	if(__stm_ehclear) { //release
		return 0;
	}
	
	if(__stm_eventcb) {
		__stm_eventcb(__stm_ehbufout[0]);
	}
	
	__STM_SetEventHook();
	
	return 0;
}

stmcallback STM_RegisterEventHandler(stmcallback newhandler)
{
	stmcallback old;
	old = __stm_eventcb;
	__stm_eventcb = newhandler;
	return old;
}

__attribute__((noreturn))
static void WaitForImpendingDoom(void) {
	u32 level;
	_CPU_ISR_Disable(level);
	ICFlashInvalidate();
	ppchalt();
}

s32 STM_ShutdownToStandby(void)
{
	int res;
	
	_viReg[1] = 0;
	if(__stm_initialized==0) {
		STM_printf("STM notinited\n");
		return STM_ENOTINIT;
	}
	u32 config = 0;
	res = __STM_SendCommand(IOCTL_STM_SHUTDOWN, &config, sizeof(u32), NULL, 0);
	if(res<0) {
		STM_printf("STM STBY failed: %d\n",res);
	}

	WaitForImpendingDoom();
	return res;
}

// idea: rename this to STM_ShutdownToIdleEx and give it some bitflags
s32 STM_ShutdownToIdle(void)
{
	int res;
	u32 config;
	
	_viReg[1] = 0;
	if(__stm_initialized==0) {
		STM_printf("STM notinited\n");
		return STM_ENOTINIT;
	}
	switch(SYS_GetHollywoodRevision()) {
		case 0:
		case 1:
		case 2:
			config = 0xFCA08280;
			break;
		default:
			config = 0xFCE082C0;
			break;
	}
	res= __STM_SendCommand(IOCTL_STM_IDLE, &config, sizeof(u32), NULL, 0);
	if(res<0) {
		STM_printf("STM IDLE failed: %d\n",res);
	}

	WaitForImpendingDoom();
	return res;
}

s32 STM_SetLedMode(u32 mode)
{
	int res;
	if(__stm_initialized==0) {
		STM_printf("STM notinited\n");
		return STM_ENOTINIT;
	}
	res= __STM_SendCommand(IOCTL_STM_LEDMODE, &mode, sizeof(u32), NULL, 0);
	if(res<0) {
		STM_printf("STM LEDMODE failed: %d\n",res);
	}
	return res;
}

s32 STM_RebootSystem(void)
{
	int res;
	
	_viReg[1] = 0;
	if(__stm_initialized==0) {
		STM_printf("STM notinited\n");
		return STM_ENOTINIT;
	}
	res= __STM_SendCommand(IOCTL_STM_HOTRESET, NULL, 0, NULL, 0);
	if(res<0) {
		STM_printf("STM HRST failed: %d\n",res);
	}

	WaitForImpendingDoom();
	return res;
}

struct STM_LedFlashConfig
{
	u32            : 8;
	u32 flags      : 8;
	u32 priority   : 8;
	u32 pattern_id : 8;

	u16 patterns[STM_MAX_LED_PATTERNS];
};

// capitalization of LED is debatable. libogc currently has STM_SetLedMode, meanwhile this function is internally called ISTM_StartLEDFlashLoop
s32 STM_StartLedFlashLoop(u8 pattern_id, u8 priority, u8 flags, const u16* patterns, u32 num_patterns)
{
	s32 ret;
	struct STM_LedFlashConfig config = {};
	if ((flags & STM_LEDFLASH_USER) && patterns != NULL) {
		if (num_patterns > STM_MAX_LED_PATTERNS)
			num_patterns = STM_MAX_LED_PATTERNS;

		memcpy(config.patterns, patterns, sizeof(u16[num_patterns]));
	} else {
		num_patterns = 0;
		flags &= ~STM_LEDFLASH_USER;
	}

	config.pattern_id = pattern_id;
	config.priority = priority;
	config.flags = flags;

	ret = __STM_SendCommand(IOCTL_STM_LEDFLASH, &config, offsetof(struct STM_LedFlashConfig, patterns[num_patterns]), NULL, 0);
	if (ret < 0) {
		STM_printf("STM LEDFLASH failed: %d\n", ret);
	}

	return ret;
}

// This is where __stm_vdinuse would come into play
// https://github.com/koopthekoopa/wii-ipl/blob/main/libs/RVL_SDK/src/os/OSStateTM.c#L176
// I guess it's to prevent the function from being called 0ms after it was called the first time. Not sure why.
s32 STM_VIDimming(bool enable, u32 luma, u32 chroma)
{
	s32 ret;
	u32 inbuf[8];
	u32 outbuf[8];

	inbuf[0] = (enable << 7) | (luma & 0x7) << 3 | (chroma & 0x7);
	inbuf[1] = 0;
	inbuf[2] = 0;

	inbuf[4] = 0; // keep nothing
	inbuf[5] = ~0; // keep everything
	inbuf[6] = ~0; // <official software> uses 0xFFFF0000 here, but, what is the register even for....

	ret = __STM_SendCommand(IOCTL_STM_VIDIMMING, inbuf, sizeof inbuf, outbuf, sizeof outbuf);
	if (ret < 0) {
		STM_printf("STM VIDIM failed: %d\n", ret);
	}

	return ret;
}

#endif /* defined(HW_RVL) */
