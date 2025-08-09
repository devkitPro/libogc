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
#include "irq.h"
#include "processor.h"
#include "cache.h"
#include "stm.h"

#define DEBUG_STM

#ifdef DEBUG_STM
#include <stdio.h>
#define STM_printf(fmt, ...)	printf("%s():%i: "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define STM_printf(fmt, ...)	while (0) {};
#endif

#define IOCTL_STM_EVENTHOOK			0x1000
#define IOCTL_STM_GETSTATE			0x3001
#define IOCTL_STM_RELEASE_EH		0x3002
#define IOCTL_STM_HOTRESET			0x2001
#define IOCTL_STM_HOTRESET_FOR_PD	0x2002
#define IOCTL_STM_SHUTDOWN			0x2003
#define IOCTL_STM_IDLEMODE			0x2004
#define IOCTL_STM_WAKEUP			0x2005
#define IOCTL_STM_VIDIMMING			0x5001
#define IOCTL_STM_LEDFLASH			0x6001
#define IOCTL_STM_LEDMODE			0x6002
#define IOCTL_STM_READVER			0x7001
#define IOCTL_STM_READDDRREG		0x4001
#define IOCTL_STM_READDDRREG2		0x4002

struct eventhook {
	s32 fd;
	u32 event_code;
	stmcallback callback;
	u32 state; /* 0: unset, 1: set, 2: release */
};

static struct eventhook __stm_eventhook;

static s32 __STMEventCallback(s32 result,void *usrdata);
s32 __STM_SetEventHook(void);
s32 __STM_ReleaseEventHook(void);

static vu16* const _viReg = (u16*)0xCC002000;

s32 __STM_Init(void)
{
	return 0;
}

s32 __STM_Close(void)
{
	__STM_ReleaseEventHook();
	return 0;
}

s32 __STM_SetEventHook(void)
{
	s32 ret;

	if (__stm_eventhook.state == 1)
		return 0;

	if (__stm_eventhook.fd < 0) {
		__stm_eventhook.fd = ret = IOS_Open("/dev/stm/eventhook", 0);
		if (ret < 0) {
			STM_printf("Failed to open STM eventhook (%i)\n", ret);
			return ret;
		}
	}

	u32 level = IRQ_Disable();
	{
		ret = IOS_IoctlAsync(__stm_eventhook.fd, IOCTL_STM_EVENTHOOK, NULL, 0, &__stm_eventhook.event_code, sizeof(u32), __STMEventCallback, &__stm_eventhook);
		if (ret == 0)
			__stm_eventhook.state = 1;
	}
	IRQ_Restore(level);

	return ret;
}

s32 __STM_ReleaseEventHook(void)
{
	s32 ret = STM_ENOHANDLER, fd;

	STM_printf("Release\n");

	if (__stm_eventhook.state == 1) {
		ret = fd = IOS_Open("/dev/stm/immediate", 0);
		if (ret >= 0) {
			__stm_eventhook.state = 2;
			ret = IOS_Ioctl(fd, IOCTL_STM_RELEASE_EH, NULL, 0, NULL, 0);
			IOS_Close(fd);
		} else {
			STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
		}
	}
	else if (__stm_eventhook.fd >= 0) {
		ret = IOS_Close(__stm_eventhook.fd);
		__stm_eventhook.fd = -1;
	}


	return ret;
}

static s32 __STMEventCallback(s32 result, void *usrdata)
{
	__stm_eventhook.state = 0;

	if (__stm_eventhook.state == 2) {
		/* Release. event_code will probably be 0. */
		result = IOS_Close(__stm_eventhook.fd);
		__stm_eventhook.fd = -1;
	}

	else if (result < 0) {
		STM_printf("STM eventhook error (%i), was it already registered?\n", result);
	}

	else {
		STM_printf("STM eventhook handler, event=%08x\n", __stm_eventhook.event_code);

		if (__stm_eventhook.callback)
			__stm_eventhook.callback(__stm_eventhook.event_code);

		__STM_SetEventHook();
	}
	
	return result;
}

stmcallback STM_RegisterEventHandler(stmcallback new)
{
	stmcallback old;
	old = __stm_eventhook.callback;
	__stm_eventhook.callback = new;
	__STM_SetEventHook();
	return old;
}

__attribute__((noreturn))
static void WaitForImpendingDoom(void) {
	IRQ_Disable();
	ICFlashInvalidate();
	ppchalt();
}

s32 STM_ShutdownToStandby(void)
{
	s32 ret, fd;
	u32 config = 0;

	_viReg[1] = 0;
	ret = fd = IOS_Open("/dev/stm/immediate", 0);
	if (ret >= 0) {
		ret = IOS_Ioctl(fd, IOCTL_STM_SHUTDOWN, &config, sizeof(u32), NULL, 0);
		if (ret == 0) {
			WaitForImpendingDoom();
		} else {
			STM_printf("STM Shutdown failed (%i). Why? It does not even check input size...\n", ret);
		}

		/* Should we hang regardless? The Wii menu does, pulling OSHalt if STM was not ready by the time <function> was called */
		IOS_Close(fd);
	} else {
		STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
	}

	return ret;
}

s32 STM_ShutdownToIdle(void)
{
	s32 ret, fd;

	u32 config = 0xFCA08280;
	if (SYS_GetHollywoodRevision() > 2)
		config |= 0x00400040;

	_viReg[1] = 0;
	ret = fd = IOS_Open("/dev/stm/immediate", 0);
	if (ret >= 0) {
		ret = IOS_Ioctl(fd, IOCTL_STM_IDLEMODE, &config, sizeof(u32), NULL, 0);
		if (ret == 0) {
			WaitForImpendingDoom();
		} else {
			STM_printf("STM IdleMode failed (%i).\n", ret);
		}
		IOS_Close(fd);
	} else {
		STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
	}

	return ret;
}

s32 STM_RebootSystem(void)
{
	s32 ret, fd;

	_viReg[1] = 0;
	ret = fd = IOS_Open("/dev/stm/immediate", 0);
	if (ret >= 0) {
		ret = IOS_Ioctl(fd, IOCTL_STM_HOTRESET, NULL, 0, NULL, 0);
		if (ret == 0) {
			WaitForImpendingDoom();
		} else {
			STM_printf("STM HotReset failed. (%i)\n", ret);
		}
		IOS_Close(fd);
	} else {
		STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
	}

	return ret;
}

s32 STM_SetLedMode(u32 mode)
{
	s32 ret, fd;

	ret = fd = IOS_Open("/dev/stm/immediate", 0);
	if (ret >= 0) {
		ret = IOS_Ioctl(fd, IOCTL_STM_LEDMODE, &mode, sizeof(u32), NULL, 0);
		if (ret < 0) {
			STM_printf("STM LEDMode failed (%i).\n", ret);
		} else if (mode == 0) {
			STM_printf("Forced led off.\n");
		}

		IOS_Close(fd);
	} else {
		STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
	}

	return ret;
}

struct STM_LEDFlashConfig
{
	u32          : 8;
	u32 flags    : 8;
	u32 priority : 8;
	u32 id       : 8;

	u16 patterns[STM_MAX_LED_PATTERNS];
};
_Static_assert(offsetof(struct STM_LEDFlashConfig, patterns) == 0x4, "?");

s32 STM_StartLEDFlashLoop(u8 id, u8 priority, u8 flags, const u16* patterns, u32 num_patterns)
{
	s32 ret, fd;

	ret = fd = IOS_Open("/dev/stm/immediate", 0);
	if (ret >= 0) {
		struct STM_LEDFlashConfig config;

		if ((flags & STM_LEDFLASH_USER) && patterns != NULL) {
			if (num_patterns >= STM_MAX_LED_PATTERNS)
				num_patterns = STM_MAX_LED_PATTERNS;

			memcpy(config.patterns, patterns, sizeof(u16[num_patterns]));
		} else {
			num_patterns = 0;
			flags &= ~STM_LEDFLASH_USER;
		}

		config.id = id;
		config.priority = priority;
		config.flags = flags;

		ret = IOS_Ioctl(fd, IOCTL_STM_LEDFLASH, &config, offsetof(struct STM_LEDFlashConfig, patterns[num_patterns]), NULL, 0);
		if (ret < 0) {
			STM_printf("STM LEDFlash failed (%i). :(\n", ret);
		}

		IOS_Close(fd);
	} else {
		STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
	}

	return ret;
}

// https://wiibrew.org/wiki//dev/stm/immediate#VIDimming
s32 STM_VIDimming(bool enable, u32 luma, u32 chroma) {
	s32 ret, fd;
	u32 inbuf[8], outbuf[8];

	inbuf[0] = enable << 7 | (luma & 0x7) << 3 | (chroma & 0x7);
	inbuf[1] = 0;
	inbuf[2] = 0;

	inbuf[4] = 0; // keep nothing
	inbuf[5] = ~0; // keep everything
	inbuf[6] = ~0; // <official software> uses 0xFFFF0000 here, but, what is the register even for....

	ret = fd = IOS_Open("/dev/stm/immediate", 0);
	if (ret >= 0) {
		ret = IOS_Ioctl(fd, IOCTL_STM_VIDIMMING, inbuf, sizeof inbuf, outbuf, sizeof outbuf);
		if (ret < 0) {
			STM_printf("STM LEDMode failed (%i).\n", ret);
		} else {
			STM_printf("0x0d80001c: %08x\n", outbuf[0]);
			STM_printf("0x0d800020: %08x\n", outbuf[1]);
			STM_printf("0x0d800028: %08x\n", outbuf[2]);
		}

		IOS_Close(fd);
	} else {
		STM_printf("Open /dev/stm/immediate failed {%i}\n", ret);
	}

	return ret;
}

#endif /* defined(HW_RVL) */
