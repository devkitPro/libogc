#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

// haxhaxhax
#define _LANGUAGE_ASSEMBLY
#include <ogc/machine/asm.h>
#undef _LANGUAGE_ASSEMBLY

#include "stubasm.h"

#include <ogc/lwp_threads.h>
#include <ogc/machine/processor.h>

static context_storage di_ctx;

#define IOS_HEAP_SIZE 0x1000

void __IPC_Reinitialize(void);

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;

//#define DEBUG_DVD_STUB

#ifdef DEBUG_DVD_STUB
#define dprintf printf
#else
#define dprintf(...)
#endif

static void dumpregs(void)
{
	dprintf(" MSR:     %08x\n",mfmsr());
	dprintf(" SPRGx:   %08x %08x %08x %08x\n", mfspr(SPRG0), mfspr(SPRG1), mfspr(SPRG2), mfspr(SPRG3));
	dprintf(" HID0124: %08x %08x %08x %08x\n", mfspr(HID0), mfspr(HID1), mfspr(HID2), mfspr(HID4));
	dprintf(" L2CR:    %08x\n", mfspr(L2CR));
	dprintf(" WPAR:    %08x\n", mfspr(WPAR));
	dprintf(" PMCx:    %08x %08x %08x %08x\n", mfspr(PMC1), mfspr(PMC2), mfspr(PMC3), mfspr(PMC4));
	dprintf(" MMCRx:   %08x %08x\n", mfspr(MMCR0), mfspr(MMCR1));
/*
	dprintf(" PI Regs: %08x %08x %08x %08x\n", _piReg[0], _piReg[1], _piReg[2], _piReg[3]);
	dprintf("          %08x %08x %08x %08x\n", _piReg[4], _piReg[5], _piReg[6], _piReg[7]);
	dprintf("          %08x %08x %08x %08x\n", _piReg[8], _piReg[9], _piReg[10], _piReg[11]);
	*/
	/*
	dprintf(" MI Regs: %04x %04x %04x %04x %04x %04x %04x %04x\n", _memReg[0], _memReg[1], _memReg[2], _memReg[3], _memReg[4], _memReg[5], _memReg[6], _memReg[7]);
	dprintf("          %04x %04x %04x %04x %04x %04x %04x %04x\n", _memReg[8], _memReg[9], _memReg[10], _memReg[11], _memReg[12], _memReg[13], _memReg[14], _memReg[15]);
	dprintf("          %04x %04x %04x %04x %04x %04x %04x %04x\n", _memReg[16], _memReg[17], _memReg[18], _memReg[19], _memReg[20], _memReg[21], _memReg[22], _memReg[23]);
	dprintf("          %04x %04x %04x %04x %04x %04x %04x %04x\n", _memReg[24], _memReg[25], _memReg[26], _memReg[27], _memReg[28], _memReg[29], _memReg[30], _memReg[31]);
	dprintf("          %04x %04x %04x %04x %04x %04x %04x %04x\n", _memReg[32], _memReg[33], _memReg[34], _memReg[35], _memReg[36], _memReg[37], _memReg[38], _memReg[39]);
	dprintf("          %04x %04x %04x %04x %04x %04x %04x %04x\n", _memReg[40], _memReg[41], _memReg[42], _memReg[43], _memReg[44], _memReg[45], _memReg[46], _memReg[47]);
	*/
}

static register_storage di_regs;

static void __distub_saveregs(void)
{
	int i;
	di_regs.timebase = gettime();
	for(i=1;i<6;i++)
		di_regs.piReg[i] = _piReg[i];
}

static void __distub_restregs(void)
{
	int i;
	for(i=1;i<6;i++)
		_piReg[i] = di_regs.piReg[i];
	//i = _piReg[0]; //clear all interrupts
	settime(di_regs.timebase);
}

u32 __di_check_ahbprot(void) {
	s32 res;
	u64 title_id;
	u32 tmd_size;
	STACK_ALIGN(u32, tmdbuf, 1024, 32);

	res = __ES_Init();

	if (res < 0) {
		dprintf("ES failed to initialize\n");
		return res;
	}

	res = ES_GetTitleID(&title_id);
	if (res < 0) {
		dprintf("ES_GetTitleID() failed: %d\n", res);
		return res;
	}

	res  = ES_GetStoredTMDSize(title_id, &tmd_size);
	if (res < 0) {
		dprintf("ES_GetStoredTMDSize() failed: %d\n", res);
		return res;
	}

	if (tmd_size > 4096) {
		dprintf("TMD too big: %d\n", tmd_size);
		return -EINVAL;
	}

	res = ES_GetStoredTMD(title_id, tmdbuf, tmd_size);
	if (res < 0) {
		dprintf("ES_GetStoredTMD() failed: %d\n", res);
		return -EINVAL;
	}

	if ((tmdbuf[0x76] & 3) == 3) {
		dprintf("ahbprot flags are set!\n");
		return 1;
	}

	dprintf("ahbprot flags are not set!\n");
	return 0;
}

static u32 __di_find_stub(u64 *title_id) {
	s32 ios_hid, res;
	u32 count, i, tmd_view_size;
	u64 *titles;
	u16 rev_highest, rev_this;
	STACK_ALIGN(u8, tmdbuf, 1024, 32);

	*title_id = 0;
	rev_highest = 0;

	ios_hid = iosCreateHeap(IOS_HEAP_SIZE);
	if (ios_hid < 0) {
		dprintf("iosCreateHeap() failed: %d\n", ios_hid);
		return ios_hid;
	}

	res = ES_GetNumTitles(&count);
	if (res < 0) {
		iosDestroyHeap(ios_hid);
		dprintf("ES_GetNumTitles() failed: %d\n", res);
		return res;
	}

	dprintf("%u titles are installed\n", count);

	titles = iosAlloc(ios_hid, sizeof(u64) * count);
	if (!titles) {
		iosDestroyHeap(ios_hid);
		dprintf("iosAlloc() failed\n");
		return -1;
	}

	res = ES_GetTitles(titles, count);
	if (res < 0) {
		iosFree(ios_hid, titles);
		iosDestroyHeap(ios_hid);
		dprintf("ES_GetTitles() failed: %d\n", res);
		return res;
	}

	for (i = 0; i < count; i++) {
		if ((titles[i] >> 32) != 0x00010008)
			continue;

		dprintf("found hidden title 0x%llx\n", titles[i]);

		res  = ES_GetTMDViewSize(titles[i], &tmd_view_size);
		if (res < 0) {
			dprintf("ES_GetTMDViewSize() failed: %d\n", res);
			continue;
		}

		if (tmd_view_size < 90) {
			dprintf("TMD too small: %d\n", tmd_view_size);
			continue;
		}

		if (tmd_view_size > 1024) {
			dprintf("TMD too big: %d\n", tmd_view_size);
			continue;
		}

		res = ES_GetTMDView(titles[i], tmdbuf, tmd_view_size);
		if (res < 0) {
			dprintf("ES_GetTMDView() failed: %d\n", res);
			continue;
		}

		if ((tmdbuf[0x18] == 'D') && (tmdbuf[0x19] == 'V')) {
			rev_this = (tmdbuf[88] << 8) | tmdbuf[89];
			dprintf("found stub with revision 0x%x\n", rev_this);

			if (rev_this > rev_highest) {
				*title_id = titles[i];
				rev_highest = rev_this;
			}
		}
	}

	iosFree(ios_hid, titles);
	iosDestroyHeap(ios_hid);

	if (*title_id) {
		dprintf("we have a winner: 0x%llx\n", *title_id);
		return 0;
	}

	return -1;
}

s32 __DI_StubLaunch(void)
{
	u64 titleID = 0;
	static tikview views[4] ATTRIBUTE_ALIGN(32);
	u32 numviews;
	s32 res;
	u32 ints;

	res = __di_find_stub(&titleID);
	if (res < 0) {
		dprintf("stub not installed\n");
		return res;
	}

	dprintf("Stopping thread timeslice ticker\n");
	__lwp_thread_stoptimeslice();

	dprintf("Shutting down IOS subsystems\n");
	res = __IOS_ShutdownSubsystems();
	if (res < 0)
		dprintf("Shutdown failed: %d\n",res);

	dprintf("Initializing ES\n");
	__ES_Init();

	dprintf("Launching TitleID: %016llx\n",titleID);

	res = ES_GetNumTicketViews(titleID, &numviews);
	if(res < 0) {
		dprintf(" GetNumTicketViews failed: %d\n",res);
		__IOS_InitializeSubsystems();
		__lwp_thread_starttimeslice();
		return res;
	}
	if(numviews > 4) {
		dprintf(" GetNumTicketViews too many views: %u\n",numviews);
		__IOS_InitializeSubsystems();
		__lwp_thread_starttimeslice();
		return IOS_ETOOMANYVIEWS;
	}
	res = ES_GetTicketViews(titleID, views, numviews);
	if(res < 0) {
		dprintf(" GetTicketViews failed: %d\n",res);
		__IOS_InitializeSubsystems();
		__lwp_thread_starttimeslice();
		return res;
	}
	dprintf("Ready to launch channel\n");
	res = ES_LaunchTitleBackground(titleID, &views[0]);
	if(res<0) {
		dprintf("Launch failed: %d\n",res);
		__IOS_InitializeSubsystems();
		__lwp_thread_starttimeslice();
		return res;
	}

	dprintf("Channel launching in the background\n");
	dprintf("Pre-stub status:\n");
	dumpregs();
	dprintf("ISR Disable...\n");
	_CPU_ISR_Disable( ints );
	dprintf("Saving regs...\n");
	__distub_saveregs();
	dprintf("Taking the plunge...\n");
	__distub_take_plunge(&di_ctx);

	dprintf("We're back!\n");
	dprintf("Restoring regs...\n");
	__distub_restregs();
	dprintf("ISR Enable...\n");
	_CPU_ISR_Restore( ints );

	dprintf("Post-stub status:\n");
	dumpregs();

	__IPC_Reinitialize();
	__ES_Reset();

	dprintf("IPC reinitialized\n");
	sleep(1);
	dprintf("Restarting IOS subsystems\n");

	res = __IOS_InitializeSubsystems();

	dprintf("Subsystems running!\n");

	res = ES_GetNumTicketViews(titleID, &numviews);
	if(res < 0)
		dprintf(" GetNumTicketViews failed: %d\n",res);
	else
		dprintf(" GetNumTicketViews: %d\n",numviews);

	dprintf("Restarting threads timeslice ticker\n");
	__lwp_thread_starttimeslice();

	return 0;
}

