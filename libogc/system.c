#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "asm.h"
#include "irq.h"
#include "cache.h"
#include "video.h"
#include "exception.h"
#include "sys_state.h"
#include "lwp_threads.h"
#include "lwp_priority.h"
#include "lwp_watchdog.h"
#include "system.h"

//#define _DEBUG_CON

#define SYSMEM_SIZE 0x1800000

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static void *__sysarenalo = NULL;
static void *__sysarenahi = NULL;

static resetcallback __RSWCallback = NULL;
#ifdef _DEBUG_CON
static void *__dbgframebuffer = NULL;
#endif

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;

extern u32 __lwp_sys_init();
extern void __vi_init();
extern void __heap_init();
extern void __exception_init();
extern void __systemcall_init();
extern void __decrementer_init();
extern void __pad_init();
extern void __exi_init();
extern void __irq_init();
extern void __lwp_start_multitasking();
extern void __timesystem_init();
extern void __memlock_init();
extern void __libc_init(int);
/*#ifdef _DEBUG_CON
extern void console_init(void *,int,int,int);
#endif
*/
extern void __realmode(void*);
extern void __config24Mb();
extern void __config48Mb();

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern u8 __ArenaLo[],__ArenaHi[];

static void __MEMInterruptHandler()
{
	_memReg[16] = 0;
}

static void __RSWHandler()
{
	if(__RSWCallback)
		__RSWCallback();
	_piReg[0] = 2;
}


#ifdef _DEBUG_CON
static void __dbgconsole_init()		// for debugging purpose where we need it from the begining
{
	u32 arLo;
	const u32 fbsize = 640*576*sizeof(u16);

	arLo = (u32)SYS_GetArenaLo();
	arLo = (arLo+31)&~31;
	SYS_SetArenaLo((void*)(arLo+fbsize));

	__dbgframebuffer = MEM_K0_TO_K1(arLo);
	console_init(__dbgframebuffer,640,576,1280);

	VIDEO_Init(VIDEO_AUTODETECT);
	VIDEO_SetFrameBuffer(VIDEO_FRAMEBUFFER_1, MEM_VIRTUAL_TO_PHYSICAL(__dbgframebuffer));
	VIDEO_SetFrameBuffer(VIDEO_FRAMEBUFFER_2, MEM_VIRTUAL_TO_PHYSICAL(__dbgframebuffer+1280));
}
#endif

static void __lowmem_init()
{
	void *ram_start = (void*)0x80000000;

	memset(ram_start, 0, 0x100);

	// 0..f  game-info
	strcpy(ram_start, "GPOP8P");        // PSO
	*((u32*)(ram_start+8))		= 1;
	*((u32*)(ram_start+0x1C))	= 0xc2339f3d;   // DVD magic word

	*((u32*)(ram_start+0x20))	= 0x0d15ea5e;   // magic word "disease"
	*((u32*)(ram_start+0x24))	= 1;            // version
	*((u32*)(ram_start+0x28))	= SYSMEM_SIZE; // memory size
		// retail.
	*((u32*)(ram_start+0x2C))	= 1 + ((*(u32*)0xCC00302c)>>28);

	*((u32*)(ram_start+0x30))	= (u32)__ArenaLo;						// ArenaLo (guess: low-watermark of memory, aligned on a 32b boundary)
	*((u32*)(ram_start+0x34))	= (u32)__ArenaHi;						// ArenaHi (guess: hi-watermark of memory)
	*((u32*)(ram_start+0x38))	= 0; // 0x817fe8c0; // FST location in ram
	*((u32*)(ram_start+0x3C))	= 0; // 0x2026e; // FST max length ?

	*((u32*)(ram_start+0xCC))	= 1; // video mode

	*((u32*)(ram_start+0xEC))	= (u32)__ArenaHi; // top of memory?
	*((u32*)(ram_start+0xF0))	= SYSMEM_SIZE; // simulated memory size
	//	*((u32*)(ram_start+0xF4))	= 0x817fc8c0; // debug pointer.. not used
	*((u32*)(ram_start+0xF8))	= 162000000;  // bus speed: 162 MHz
	*((u32*)(ram_start+0xFC))	= 486000000;  // cpu speed: 486 Mhz

	*((u16*)(ram_start+0x30E0))	= 6; // production pads

	DCFlushRange(ram_start, 0x100);

	SYS_SetArenaLo((void*)__ArenaLo);
	SYS_SetArenaHi((void*)__ArenaHi);

	//clear last 65kb mem
	memset(__ArenaHi,0,65535);
}

static void __memprotect_init()
{
	u32 level;
	u32 realmem = *((u32*)0x80000028);
	u32 simmem = *((u32*)0x800000F0);

	_CPU_ISR_Disable(level);

	if(simmem<=0x1800000) __realmode(__config24Mb);
	else if(simmem<=0x3000000) __realmode(__config48Mb);

	_memReg[16] = 0;
	_memReg[8] = 255;

	IRQ_Request(IRQ_MEM0,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEM1,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEM2,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEM3,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEMADDRESS,__MEMInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_MEMADDRESS));		//only enable memaddress irq atm

	if(simmem<=realmem && !(simmem-0x1800000)) _memReg[20] = 2;

	_CPU_ISR_Restore(level);
}

static void DisableWriteGatherPipe()
{
	mtspr(920,(mfspr(920)&~0x40000000));
}

void SYS_Init()
{
	__lowmem_init();
#ifdef _DEBUG_CON
	__dbgconsole_init();
#endif
	__libc_init(1);
	__memprotect_init();
	__sys_state_init();
	__lwp_priority_init();
	__lwp_watchdog_init();
	__exception_init();
	__systemcall_init();
	__decrementer_init();
	__timesystem_init();
	__irq_init();
	__lwp_sys_init();
	__memlock_init();
	__pad_init();
	__exi_init();
	__vi_init();
	DisableWriteGatherPipe();

	IRQ_Request(IRQ_PI_RSW,__RSWHandler,NULL);

	__lwp_start_multitasking();
}

void SYS_SetArenaLo(void *newLo)
{
	__sysarenalo = newLo;
}

void* SYS_GetArenaLo()
{
	return __sysarenalo;
}

void SYS_SetArenaHi(void *newHi)
{
	__sysarenahi = newHi;
}

void* SYS_GetArenaHi()
{
	return __sysarenahi;
}

void SYS_ProtectRange(u32 chan,void *addr,u32 bytes,u32 cntrl)
{
	u16 rcntrl;
	u32 pstart,pend,level;

	if(chan<SYS_PROTECTCHANMAX) {
		pstart = ((u32)addr)&~0x3ff;
		pend = ((((u32)addr)+bytes)+1023)&~0x3ff;
		DCFlushRange((void*)pstart,(pend-pstart));

		_CPU_ISR_Disable(level);

		__UnmaskIrq(IRQMASK(chan));
		_memReg[chan<<2] = _SHIFTR(pstart,10,16);
		_memReg[(chan<<2)+1] = _SHIFTR(pend,10,16);

		rcntrl = _memReg[8];
		rcntrl = (rcntrl&~(_SHIFTL(3,(chan<<1),2)))|(_SHIFTL(cntrl,(chan<<1),2));
		_memReg[8] = rcntrl;

		if(cntrl==SYS_PROTECTRDWR)
			__MaskIrq(IRQMASK(chan));


		_CPU_ISR_Restore(level);
	}
}

resetcallback SYS_SetResetCallback(resetcallback cb)
{
	u32 level;
	resetcallback old = __RSWCallback;

	_CPU_ISR_Disable(level);

	__RSWCallback = cb;
	if(__RSWCallback) {
		_piReg[0] = 2;
		__UnmaskIrq(IRQMASK(IRQ_PI_RSW));
	} else
		__MaskIrq(IRQMASK(IRQ_PI_RSW));

	_CPU_ISR_Restore(level);
	return old;
}

void SYS_StartPMC(u32 mcr0val,u32 mcr1val)
{
	mtmmcr0(mcr0val);
	mtmmcr1(mcr1val);
}

void SYS_StopPMC()
{
	mtmmcr0(0);
	mtmmcr1(0);
}

void SYS_ResetPMC()
{
	mtpmc1(0);
	mtpmc2(0);
	mtpmc3(0);
	mtpmc4(0);
}

void SYS_DumpPMC()
{
	printf("<%d load/stores / %d miss cycles / %d cycles / %d instructions>\n",mfpmc1(),mfpmc2(),mfpmc3(),mfpmc4());
}
