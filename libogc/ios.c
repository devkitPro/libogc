/*-------------------------------------------------------------

ios.c -- IOS control

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

#include <stdio.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "ipc.h"
#include "stm.h"
#include "es.h"
#include "ios.h"

#define IOS_HEAP_SIZE 0x1000

//#define DEBUG_IOS

#define IOS_MAX_VERSION 36
#define IOS_MIN_VERSION 3

static s32 __ios_hid = -1;

s32 __IOS_InitHeap(void)
{
	if(__ios_hid <0 ) {
		__ios_hid = iosCreateHeap(IOS_HEAP_SIZE);
		if(__ios_hid < 0) return __ios_hid;
	}
	return 0;
}

// These two functions deal with the "internal" IOS subsystems that are used by default by libogc
// Other stuff should be inited by the user and deinited by the exit callbacks. The user is also responsible
// for deiniting other stuff before an IOS reload and reiniting them after.
s32 __IOS_InitializeSubsystems(void)
{
	s32 res;
	s32 ret = 0;
#ifdef DEBUG_IOS
	printf("IOS Subsystem Init\n");
#endif
	res = __ES_Init();
	if(res < 0) {
		ret = res;
#ifdef DEBUG_IOS
		printf("ES Init failed: %d\n",ret);
#endif
	}
	res = __STM_Init();
	if(res < 0) {
		ret = res;
#ifdef DEBUG_IOS
		printf("STM Init failed: %d\n",ret);
#endif
	}
#ifdef DEBUG_IOS
	printf("IOS Subsystem Init Done: %d\n",ret);
#endif
	return ret;
}

s32 __IOS_ShutdownSubsystems(void)
{
	s32 res;
	s32 ret = 0;
#ifdef DEBUG_IOS
	printf("IOS Subsystem Close\n");
#endif
	res = __STM_Close();
	if(res < 0) ret = res;
	res = __ES_Close();
	if(res < 0) ret = res;
#ifdef DEBUG_IOS
	printf("IOS Subsystem Close Done: %d\n",ret);
#endif
	return ret;
}

s32 IOS_GetPreferredVersion()
{
	int ver = IOS_EBADVERSION;
	s32 res;
	u32 count;
	u64 *titles;
	u32 i;
	u32 a,b;
	
	res = __IOS_InitHeap();
	if(res<0) return res;
	
	res = ES_GetNumTitles(&count);
	if(res < 0) {
#ifdef DEBUG_IOS
		printf(" GetNumTitles failed: %d\n",res);
#endif
		return res;
	}
#ifdef DEBUG_IOS
	printf(" %d titles on card:\n",count);
#endif
	titles = iosAlloc(__ios_hid, sizeof(u64)*count);
	if(!titles) {
		printf(" iosAlloc titles failed\n");
		return -1;
	}
	res = ES_GetTitles(titles, count);
	if(res < 0) {
#ifdef DEBUG_IOS
		printf(" GetTitles failed: %d\n",res);
#endif
		iosFree(__ios_hid, titles);
		return res;
	}
	
	for(i=0; i<count; i++) {
		a = titles[i]>>32;
		b = titles[i]&0xFFFFFFFF;
		if(a != 1) continue;
		if(b < IOS_MIN_VERSION) continue;
		if(b > IOS_MAX_VERSION) continue;
		if(((s32)b) > ((s32)ver)) ver = b;
	}
#ifdef DEBUG_IOS
	printf(" Preferred verson: %d\n",ver);
#endif
	iosFree(__ios_hid, titles);
	return ver;
}

s32 IOS_GetVersion()
{
	u32 vercode;
	u16 version;
	DCInvalidateRange((void*)0x80003140,8);
	vercode = *((u32*)0x80003140);
	version = vercode >> 16;
	if(version == 0) return IOS_EBADVERSION;
	if(version > 0xff) return IOS_EBADVERSION;
	return version;
}

s32 IOS_GetRevision()
{
	u32 vercode;
	u16 rev;
	DCInvalidateRange((void*)0x80003140,8);
	vercode = *((u32*)0x80003140);
	rev = vercode & 0xFFFF;
	if(vercode == 0 || rev == 0) return IOS_EBADVERSION;
	return rev;
}

s32 IOS_GetRevisionMajor()
{
	s32 rev;
	rev = IOS_GetRevision();
	if(rev < 0) return rev;
	return (rev>>8)&0xFF;
}

s32 IOS_GetRevisionMinor()
{
	s32 rev;
	rev = IOS_GetRevision();
	if(rev < 0) return rev;
	return rev&0xFF;
}

s32 __IOS_LaunchNewIOS(int version)
{
	u32 numviews;
	s32 res;
	u64 titleID = 0x100000000LL;
	STACK_ALIGN(tikview,views,4,32);
#ifdef DEBUG_IOS	
	s32 oldversion;
#endif
	s32 newversion;
	
	if(version < 3 || version > 0xFF) {
		return IOS_EBADVERSION;
	}
	
#ifdef DEBUG_IOS
	oldversion = IOS_GetVersion();
	if(oldversion>0) printf("Current IOS Version: IOS%d\n",oldversion);
#endif
	
	titleID |= version;
#ifdef DEBUG_IOS
	printf("Launching IOS TitleID: %016llx\n",titleID);
#endif
	
	res = ES_GetNumTicketViews(titleID, &numviews);
	if(res < 0) {
#ifdef DEBUG_IOS
		printf(" GetNumTicketViews failed: %d\n",res);
#endif
		return res;
	}
	if(numviews > 4) {
		printf(" GetNumTicketViews too many views: %u\n",numviews);
		return IOS_ETOOMANYVIEWS;
	}
	res = ES_GetTicketViews(titleID, views, numviews);
	if(res < 0) {
#ifdef DEBUG_IOS
		printf(" GetTicketViews failed: %d\n",res);
#endif
		return res;
	}
	res = ES_LaunchTitle(titleID, &views[0]);
	if(res < 0) {
#ifdef DEBUG_IOS
		printf(" LaunchTitle failed: %d\n",res);
#endif
		return res;
	}
	__ES_Reset();
	newversion = IOS_GetVersion();
#ifdef DEBUG_IOS
	printf(" IOS Version: IOS%d %d.%d\n",newversion,IOS_GetRevisionMajor(),IOS_GetRevisionMinor());
#endif
	if(newversion != version) {
#ifdef DEBUG_IOS
		printf(" Version mismatch!\n");
#endif
		return IOS_EMISMATCH;
	}
	return version;
}

s32 __attribute__((weak)) __IOS_LoadStartupIOS()
{
	int version;
	int res;
	
	res = __ES_Init();
	if(res < 0) return res;
	version = IOS_GetPreferredVersion();
	if(version < 0) {
#ifdef DEBUG_IOS
		printf("GetPreferredVersion failed: %d\n",version);
#endif
		__ES_Close();
		return version;
	}
#ifdef DEBUG_IOS
	printf("Loading startup IOS: %d\n",version);
#endif
	res = __IOS_LaunchNewIOS(version);
	if(res < 0) return res;
	return 0;
}

s32 IOS_ReloadIOS(int version)
{
	int ret = 0;
	int res;
#ifdef DEBUG_IOS
	printf("Reloading to IOS%d\n",version);
#endif
	res = __IOS_ShutdownSubsystems();
	if(res < 0) ret = res;
	res = __ES_Init();
	if(res < 0) ret = res;
	else {
		res = __IOS_LaunchNewIOS(version);
		if(res < 0) {
			ret = res;
			__ES_Close();
		}
	}
	res = __IOS_InitializeSubsystems();
	if(res < 0) ret = res;
	return ret;
}

#endif /* defined(HW_RVL) */
