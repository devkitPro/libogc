
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

context_storage di_ctx;

#define DVD_TITLEID 0x0001000844564458LL
//#define DVD_TITLEID 0x1000148415858LL

void __IPC_Reinitialize(void);
extern void __exi_init();

static vu16* const _viReg = (u16*)0xCC002000;
static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;
static vu16* const _dspReg = (u16*)0xCC005000;

//#define DEBUG_DVD_STUB

#ifdef DEBUG_DVD_STUB
#define dprintf printf
#else
#define dprintf(...)
#endif

void dumpregs(void)
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

register_storage di_regs;

void __distub_saveregs(void)
{
	int i;
	di_regs.timebase = gettime();
	for(i=1;i<6;i++)
		di_regs.piReg[i] = _piReg[i];
}

void __distub_restregs(void)
{
	int i;
	for(i=1;i<6;i++)
		_piReg[i] = di_regs.piReg[i];
	//i = _piReg[0]; //clear all interrupts
	settime(di_regs.timebase);
}


s32 __DI_StubLaunch(void)
{
	u64 titleID = DVD_TITLEID;
	static tikview views[4] ATTRIBUTE_ALIGN(32);
	u32 numviews;
	s32 res;
	
	u32 ints;
	
	dprintf("Stopping thread timeslice ticker\n");
	__lwp_thread_stoptimeslice();

	dprintf("Shutting down IOS subsystems\n");
	res = __IOS_ShutdownSubsystems();
	if(res < 0) {
		dprintf("Shutdown failed: %d\n",res);
	}
	dprintf("Initializing ES\n");
	res = __ES_Init();
	if(res < 0) {
		dprintf("ES init failed: %d\n",res);
		return res;
	}
	
	dprintf("Launching TitleID: %016llx\n",titleID);
	
	res = ES_GetNumTicketViews(titleID, &numviews);
	if(res < 0) {
		dprintf(" GetNumTicketViews failed: %d\n",res);
		return res;
	}
	if(numviews > 4) {
		dprintf(" GetNumTicketViews too many views: %u\n",numviews);
		return IOS_ETOOMANYVIEWS;
	}
	res = ES_GetTicketViews(titleID, views, numviews);
	if(res < 0) {
		dprintf(" GetTicketViews failed: %d\n",res);
		return res;
	}
	dprintf("Ready to launch channel\n");
	res = ES_LaunchTitleBackground(titleID, &views[0]);
	if(res<0) {
		dprintf("Launch failed: %d\n",res);
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
	if(res < 0) {
		dprintf(" GetNumTicketViews failed: %d\n",res);
		return res;
	}
	dprintf(" GetNumTicketViews: %d",numviews);

	dprintf("Restarting threads timeslice ticker\n");
	__lwp_thread_starttimeslice();

	return 0;
}


s32 __DI_LoadStub(void)
{
	int ret = 0;
	int res;
#ifdef DEBUG_IOS
	dprintf("Reloading to IOS%d\n",version);
#endif
	res = __IOS_ShutdownSubsystems();
	if(res < 0) ret = res;
	res = __ES_Init();
	if(res < 0) ret = res;
	else {
		res = __DI_StubLaunch();
		if(res < 0) {
			ret = res;
			__ES_Close();
		}
	}
	res = __IOS_InitializeSubsystems();
	if(res < 0) ret = res;
	return ret;
}
