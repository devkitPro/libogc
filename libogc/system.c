#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "asm.h"
#include "irq.h"
#include "exi.h"
#include "cache.h"
#include "video.h"
#include "sys_state.h"
#include "lwp_threads.h"
#include "lwp_priority.h"
#include "lwp_watchdog.h"
#include "lwp_wkspace.h"
#include "system.h"

#define SYSMEM_SIZE				0x1800000
#define KERNEL_HEAP				(512*1024)

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_ARDMA         0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

struct _sramcntrl {
	u8 srambuf[64];
	u32 offset;
	s32 enabled;
	s32 locked;
	s32 sync;
} sramcntrl ATTRIBUTE_ALIGN(32);

static u32 system_initialized = 0;
static sysalarm *system_alarm = NULL;

static void *__sysarenalo = NULL;
static void *__sysarenahi = NULL;

static resetcallback __RSWCallback = NULL;

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;
static vu16* const _dspReg = (u16*)0xCC005000;

void SYS_SetArenaLo(void *newLo);
void* SYS_GetArenaLo();
void SYS_SetArenaHi(void *newHi);
void* SYS_GetArenaHi();

static u32 __sram_writecallback();

extern u32 __lwp_sys_init();
extern void __heap_init();
extern void __exception_init();
extern void __systemcall_init();
extern void __decrementer_init();
extern void __exi_init();
extern void __si_init();
extern void __irq_init();
extern void __lwp_start_multitasking();
extern void __timesystem_init();
extern void __memlock_init();
extern void __libc_init(int);

extern void __realmode(void*);
extern void __config24Mb();
extern void __config48Mb();

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern void settime(long long);
extern long long gettime();
extern unsigned int gettick();
extern int clock_gettime(struct timespec *tp);
extern void timespec_substract(const struct timespec *tp_start,const struct timespec *tp_end,struct timespec *result);

extern u8 __ArenaLo[],__ArenaHi[];

static u32 _dsp_initcode[] = 
{
	0x029F0010,0x029F0033,0x029F0034,0x029F0035,
	0x029F0036,0x029F0037,0x029F0038,0x029F0039,
	0x12061203,0x12041205,0x00808000,0x0088FFFF,
	0x00841000,0x0064001D,0x02180000,0x81001C1E,
	0x00441B1E,0x00840800,0x00640027,0x191E0000,
	0x00DEFFFC,0x02A08000,0x029C0028,0x16FC0054,
	0x16FD4348,0x002102FF,0x02FF02FF,0x02FF02FF,
	0x02FF02FF,0x00000000,0x00000000,0x00000000
};

static void __sys_alarmhandler(void *arg)
{
	sysalarm *alarm = (sysalarm*)arg;

	__lwp_thread_dispatchdisable();
	if(alarm) {
		if(alarm->alarmhandler) alarm->alarmhandler(alarm);
		if(alarm->periodic) __lwp_wd_insert_ticks(alarm->handle,alarm->periodic);
	}
	__lwp_thread_dispatchunnest();
}

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

static __inline__ u32 __getrtc(u32 *gctime)
{
	u32 ret;
	u32 cmd;
	u32 time;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}
	
	ret = 0;
	time = 0;
	cmd = 0x20000000;
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_Imm(EXI_CHANNEL_0,&time,4,EXI_READ,NULL)==0) ret |= 0x04;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;
	
	EXI_Unlock(EXI_CHANNEL_0);
	*gctime = time;
	if(ret) return 0;
	
	return 1;
}

static __inline__ u32 __sram_read(void *buffer)
{
	u32 command,ret;

	DCInvalidateRange(buffer,64);
	
	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}

	ret = 0;
	command = 0x20000100;
	if(EXI_Imm(EXI_CHANNEL_0,&command,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_Dma(EXI_CHANNEL_0,buffer,64,EXI_READ,NULL)==0) ret |= 0x04;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;
	if(EXI_Unlock(EXI_CHANNEL_0)==0) ret |= 0x20;

	if(ret) return 0;
	return 1;
}

static __inline__ u32 __sram_write(void *buffer,u32 loc,u32 len)
{
	u32 cmd,ret;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,__sram_writecallback)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}

	ret = 0;
	cmd = 0xa0000100+(loc<<6);
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_ImmEx(EXI_CHANNEL_0,buffer,len,EXI_WRITE)==0) ret |= 0x04;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Unlock(EXI_CHANNEL_0)==0) ret |= 0x10;
	
	if(ret) return 0;
	return 1;
}

static u32 __sram_writecallback()
{
	sramcntrl.sync = __sram_write(sramcntrl.srambuf+sramcntrl.offset,sramcntrl.offset,(64-sramcntrl.offset));
	if(sramcntrl.sync) sramcntrl.offset = 64;
	
	return 1;
}

static void __sram_init()
{
	sramcntrl.enabled = 0;
	sramcntrl.locked = 0;
	sramcntrl.sync = __sram_read(sramcntrl.srambuf);
	
	sramcntrl.offset = 64;
}

static void DisableWriteGatherPipe()
{
	mtspr(920,(mfspr(920)&~0x40000000));
}

static __inline__ void __buildchecksum(u16 *buffer,u16 *c1,u16 *c2)
{
	u32 i;
	
	*c1 = 0;
	*c2 = 0;
	for(i=0;i<4;i++) {
		*c1 += buffer[6+i];
		*c2 += buffer[6+i]^-1;
	}
}
static __inline__ void* __locksram(u32 loc)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(!sramcntrl.locked) {
		sramcntrl.enabled = level;
		sramcntrl.locked = 1;
		return (void*)((u32)sramcntrl.srambuf+loc);
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

static u32 __unlocksram(u32 write,u32 loc)
{
	syssram *sram = (syssram*)sramcntrl.srambuf;

	if(write) {
		if(!loc) {
			if((sram->flags&0x03)>0x02) sram->flags = (sram->flags&~0x03);
			__buildchecksum((u16*)sramcntrl.srambuf,&sram->checksum,&sram->checksum_inv);
		}
		if(loc<sramcntrl.offset) sramcntrl.offset = loc;
		
		sramcntrl.sync = __sram_write(sramcntrl.srambuf+sramcntrl.offset,sramcntrl.offset,(64-sramcntrl.offset));
		if(sramcntrl.sync) sramcntrl.offset = 64;
	}
	sramcntrl.locked = 0;
	_CPU_ISR_Restore(sramcntrl.enabled);
	return sramcntrl.sync;
}

static void __dsp_bootstrap()
{
	u16 status;
	u32 tick;

	memcpy(SYS_GetArenaHi()-128,(void*)0x81000000,128);
	memcpy((void*)0x81000000,_dsp_initcode,128);
	DCFlushRange((void*)0x81000000,128);
	
	_dspReg[9] = 67;
	_dspReg[5] = (DSPCR_DSPRESET|DSPCR_DSPINT|DSPCR_ARINT|DSPCR_AIINT|DSPCR_HALT);
	_dspReg[5] |= DSPCR_RES;
	while(_dspReg[5]&DSPCR_RES);

	_dspReg[0] = 0;
	while((_SHIFTL(_dspReg[2],16,16)|(_dspReg[3]&0xffff))&0x80000000);

	((u32*)_dspReg)[8] = 0x01000000;
	((u32*)_dspReg)[9] = 0;
	((u32*)_dspReg)[10] = 32;

	status = _dspReg[5];
	while(!(status&DSPCR_ARINT)) status = _dspReg[5];
	_dspReg[5] = status;

	tick = gettick();
	while((gettick()-tick)<2194);

	((u32*)_dspReg)[8] = 0x01000000;
	((u32*)_dspReg)[9] = 0;
	((u32*)_dspReg)[10] = 32;

	status = _dspReg[5];
	while(!(status&DSPCR_ARINT)) status = _dspReg[5];
	_dspReg[5] = status;

	_dspReg[5] &= ~DSPCR_DSPRESET;
	while(_dspReg[5]&0x400);

	_dspReg[5] &= ~DSPCR_HALT;
	while(!(_dspReg[2]&0x8000));
	status = _dspReg[3];

	_dspReg[5] |= DSPCR_HALT;
	_dspReg[5] = (DSPCR_DSPRESET|DSPCR_DSPINT|DSPCR_ARINT|DSPCR_AIINT|DSPCR_HALT);
	_dspReg[5] |= DSPCR_RES;
	while(_dspReg[5]&DSPCR_RES);

	memcpy((void*)0x81000000,SYS_GetArenaHi()-128,128);
#ifdef _SYS_DEBUG
	printf("__audiosystem_init(finish)\n");
#endif
}

syssram* __SYS_LockSram()
{
	return (syssram*)__locksram(0);
}

syssramex* __SYS_LockSramEx()
{
	return (syssramex*)__locksram(sizeof(syssram));
}

u32 __SYS_UnlockSram(u32 write)
{
	return __unlocksram(write,0);
}

u32 __SYS_UnlockSramEx(u32 write)
{
	return __unlocksram(write,sizeof(syssram));
}

u32 __SYS_GetRTC(u32 *gctime)
{
	u32 cnt,ret;
	u32 time1,time2;

	cnt = 0;
	ret = 0;
	while(cnt<16) {
		if(__getrtc(&time1)==0) ret |= 0x01;
		if(__getrtc(&time2)==0) ret |= 0x02;
		if(ret) return 0;
		if(time1==time2) {
			*gctime = time1;
			return 1;
		}
		cnt++;
	}
	return 0;
}

void __SYS_SetTime(s64 time)
{
	u32 level;
	s64 now;
	s64 *pBootTime = (s64*)0x800030d8;

	_CPU_ISR_Disable(level);
	now = gettime();
	now -= time;
	now += *pBootTime;
	*pBootTime = now;
	settime(now);
	EXI_ProbeReset();
	_CPU_ISR_Restore(level);
}

s64 __SYS_GetSystemTime()
{
	u32 level;
	s64 now;
	s64 *pBootTime = (s64*)0x800030d8;

	_CPU_ISR_Disable(level);
	now = gettime();
	now += *pBootTime;
	_CPU_ISR_Restore(level);
	return now;
}

void __SYS_SetBootTime()
{
	syssram *sram;
	u32 gctime;

	sram = __SYS_LockSram();
	__SYS_GetRTC(&gctime);
	__SYS_SetTime(secs_to_ticks(gctime));
	__SYS_UnlockSram(0);
}

void __sdloader_boot()
{
	void (*reload)() = (void(*)())0x80001800;
	reload();
}

void SYS_Init()
{
	if(system_initialized) return;

	system_initialized = 1;

	__lowmem_init();
	__lwp_wkspace_init(KERNEL_HEAP);
	__libc_init(1);
	__sys_state_init();
	__lwp_priority_init();
	__lwp_watchdog_init();
	__exception_init();
	__systemcall_init();
	__decrementer_init();
	__irq_init();
	__exi_init();
	__sram_init();
	__lwp_sys_init();
	__dsp_bootstrap();
	__memprotect_init();
	__memlock_init();
	__timesystem_init();
	__si_init();
#ifdef SDLOADER_FIX
	__SYS_SetBootTime();
#endif
	DisableWriteGatherPipe();

	IRQ_Request(IRQ_PI_RSW,__RSWHandler,NULL);
	__MaskIrq(IRQMASK(IRQ_PI_RSW));

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

void* SYS_AllocateFramebuffer(GXRModeObj *rmode)
{
	u32 level;
	void *ret;

	u32 size = VIDEO_PadFramebufferWidth(rmode->fbWidth)*rmode->xfbHeight*VI_DISPLAY_PIX_SZ+32;
	_CPU_ISR_Disable(level);
	ret = (void*)((((u32)SYS_GetArenaHi()-size)+31)&~31);
	if(ret<SYS_GetArenaLo()) {
		_CPU_ISR_Restore(level);
		abort();
	}
	SYS_SetArenaHi(ret);
	_CPU_ISR_Restore(level);
	return ret;
}

void SYS_CreateAlarm(sysalarm *alarm)
{
	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->start_per = 0;
	alarm->periodic = 0;
}

void SYS_SetAlarm(sysalarm *alarm,const struct timespec *tp,alarmcallback cb)
{
	u32 found,level;
	sysalarm *ptr;

	alarm->alarmhandler = cb;
	alarm->ticks = __lwp_wd_calc_ticks(tp);

	alarm->periodic = 0;
	alarm->start_per = 0;

	found = 0;

	_CPU_ISR_Disable(level);
	ptr = system_alarm;
	while(ptr && ptr->next && ptr!=alarm) ptr = ptr->next;
	if(ptr && ptr==alarm) found = 1;
	else {
		alarm->prev = NULL;
		alarm->next = NULL;
		if(ptr) {
			alarm->prev = ptr;
			ptr->next = alarm;
		} else
			system_alarm = ptr;
	}
	_CPU_ISR_Restore(level);

	if(!found) alarm->handle = __lwp_wkspace_allocate(sizeof(wd_cntrl));
	if(alarm->handle) {
		__lwp_wd_initialize(alarm->handle,__sys_alarmhandler,alarm);
		__lwp_wd_insert_ticks(alarm->handle,alarm->ticks);
	}
}

void SYS_SetPeriodicAlarm(sysalarm *alarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb)
{
	u32 found,level;
	sysalarm *ptr;

	alarm->start_per = __lwp_wd_calc_ticks(tp_start);
	alarm->periodic = __lwp_wd_calc_ticks(tp_period);
	alarm->alarmhandler = cb;

	alarm->ticks = 0;

	found = 0;

	_CPU_ISR_Disable(level);
	ptr = system_alarm;
	while(ptr && ptr->next && ptr!=alarm) ptr = ptr->next;
	if(ptr && ptr==alarm) found = 1;
	else {
		alarm->prev = NULL;
		alarm->next = NULL;
		if(ptr) {
			alarm->prev = ptr;
			ptr->next = alarm;
		} else
			system_alarm = ptr;
	}
	_CPU_ISR_Restore(level);

	if(!found) alarm->handle = __lwp_wkspace_allocate(sizeof(wd_cntrl));
	if(alarm->handle) {
		__lwp_wd_initialize(alarm->handle,__sys_alarmhandler,alarm);
		__lwp_wd_insert_ticks(alarm->handle,alarm->start_per);
	}
}

void SYS_RemoveAlarm(sysalarm *alarm)
{
	u32 level;
	sysalarm *prev,*next;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->periodic = 0;
	alarm->start_per = 0;
	
	_CPU_ISR_Disable(level);
	prev = alarm->prev;
	next = alarm->next;
	if(alarm==system_alarm) system_alarm = system_alarm->next;
	if(prev) prev->next = next;
	if(next) next->prev = prev;
	_CPU_ISR_Restore(level);

	if(alarm->handle) {
		__lwp_wd_remove(alarm->handle);
		__lwp_wkspace_free(alarm->handle);
	}
}

void SYS_CancelAlarm(sysalarm *alarm)
{
	if(alarm->handle) __lwp_wd_remove(alarm->handle);
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

void SYS_SetWirelessID(u32 chan,u32 id)
{
	u16 *ex_base,write;

	write = 0;
	ex_base = (u16*)__SYS_LockSramEx();
	if(ex_base[14+chan]!=(u16)id) {
		ex_base[14+chan] = id;
		write = 1;
	}
	__SYS_UnlockSramEx(write);
}

u32 SYS_GetWirelessID(u32 chan)
{
	u16 *ex_base,id;

	id = 0;
	ex_base = (u16*)__SYS_LockSramEx();
	id = ex_base[14+chan];
	__SYS_UnlockSramEx(0);
	return id;
}
