#include <stdlib.h>
#include <unistd.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "lwp.h"
#include "irq.h"
#include "system.h"
#include "dvd.h"

#define DVD_BRK				(1<<0)
#define DVD_DE_MSK			(1<<1)
#define DVD_DE_INT			(1<<2)
#define DVD_TC_MSK			(1<<3)
#define DVD_TC_INT			(1<<4)
#define DVD_BRK_MSK			(1<<5)
#define DVD_BRK_INT			(1<<6)

#define DVD_CVR_INT			(1<<2)
#define DVD_CVR_MASK		(1<<1)
#define DVD_CVR_STATE		(1<<0)

#define DVD_DI_MODE			(1<<2)
#define DVD_DI_DMA			(1<<1)
#define DVD_DI_START		(1<<0)

#define DVD_READSECTOR		0xA8000000

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))


static u32 __dvd_initflag = 0;
static u32 __dvd_stopnextint = 0;
static u32 __dvd_lastlen;
static u32 __dvd_nextcmdnum;
static u32 __dvd_workaround;
static u32 __dvd_workaroundseek;
static u32 __dvd_lastcmdwasread;
static lwpq_t __dvd_wait_queue;
static sysalarm __dvd_alarmtimeout;
static DVDCallback __dvd_callback = NULL;

static vu32* const _piReg = (u32*)0xCC003000;
static vu32* const _diReg = (u32*)0xCC006000;

static u8 __dvd_unlockcmd$221[12] = {0xff,0x01,'M','A','T','S','H','I','T','A',0x02,0x00};
static u8 __dvd_unlockcmd$222[12] = {0xff,0x00,'D','V','D','-','G','A','M','E',0x03,0x00};

extern void udelay(int us);

extern void __MaskIrq(u32);
extern void __UnmaskIrq(u32);

static void __dvd_timeouthandler(sysalarm *alarm)
{
	DVDCallback cb;

	__MaskIrq(IRQMASK(IRQ_PI_DI));
	cb = __dvd_callback;
	if(cb) cb(0x10);
}

static void __SetupTimeoutAlarm(const struct timespec *tp)
{
	SYS_CreateAlarm(&__dvd_alarmtimeout);
	SYS_SetAlarm(&__dvd_alarmtimeout,tp,__dvd_timeouthandler);
}

static void __Read(void *buffer,u32 len,u32 offset,DVDCallback cb)
{
	u32 val;
	struct timespec tb;
	__dvd_callback = cb;
	__dvd_stopnextint = 0;
	__dvd_lastcmdwasread = 1;
	
	_diReg[2] = DVD_READSECTOR;
	_diReg[3] = offset>>2;
	_diReg[4] = len;
	_diReg[5] = (u32)buffer;
	_diReg[6] = len;

	__dvd_lastlen = len;

	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);
	val = _diReg[7];
	if(val>0x00a00000) {
		tb.tv_sec = 20;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	} else {
		tb.tv_sec = 10;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	}
}

static void __DVDLowWATypeSet(u32 workaround,u32 workaroundseek)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_workaround = workaround;
	__dvd_workaroundseek = workaroundseek;
	_CPU_ISR_Restore(level);
}

static void __DVDInitWA()
{
	__dvd_nextcmdnum = 0;
	__DVDLowWATypeSet(0,0);	
}

static void __DVDInterruptHandler(u32 nIrq,void *pCtx)
{
	u32 status,ir,irm,irmm;

	irmm = 0;
	status = _diReg[0];
	irm = (status&(DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK));
	ir = ((status&(DVD_DE_INT|DVD_TC_INT|DVD_BRK_INT))&(irm<<1));


	if(ir&DVD_DE_INT) irmm |= DVD_DE_INT;
	if(ir&DVD_TC_INT) irmm |= DVD_TC_INT;
	if(ir&DVD_BRK_INT) irmm |= DVD_BRK_INT;

	_diReg[0] = (ir|irm);
}

static void __DVDUnlockDrive()
{
	u32 i;

	_diReg[0] |= (DVD_TC_INT|DVD_DE_INT);
	_diReg[1] = 0;

	for(i=0;i<3;i++) _diReg[2+i] = ((u32*)__dvd_unlockcmd$221)[i];
	_diReg[7] = DVD_DI_START;
	while(!(_diReg[0]&(DVD_DE_INT|DVD_TC_INT)));
	
	for(i=0;i<3;i++) _diReg[2+i] = ((u32*)__dvd_unlockcmd$222)[i];
	_diReg[7] = DVD_DI_START;
	while(!(_diReg[0]&(DVD_DE_INT|DVD_TC_INT)));
}

void DVD_LowReset()
{
	_diReg[1] = DVD_CVR_MASK;
	_piReg[9] &= ~0x0004;
	_piReg[9] |= 0x0001;

	udelay(12);

	_piReg[9] |= 0x0004;
	_piReg[9] |= 0x0001;
}


s32 DVD_ReadId(void *pDst)
{
	vs32 *pDI = (vs32*)0xCC006000;
	
	pDI[0] = 0x2E;
	pDI[1] = 0;
	pDI[2] = 0xA8000040;
	pDI[3] = 0;
	pDI[4] = 0x20;
	pDI[5] = (u32)pDst;
	pDI[6] = 0x20;
	pDI[7] = 3;
	
	if((((s32)pDst)&0xC0000000)==0x80000000) DCInvalidateRange(pDst,0x20);
	while(1) {
		if(pDI[0]&0x04)
			return 0;
		if(!pDI[6])
			return 1;
	}
}

s32 DVD_Read(void *pDst,s32 nLen,u32 nOffset)
{
	vs32 *pDI = (vs32*)0xCC006000;
	
	pDI[0] = 0x2E;
	pDI[1] = 0;
	pDI[2] = 0xA8000000;
	pDI[3] = nOffset>>2;
	pDI[4] = nLen;
	pDI[5] = (u32)pDst;
	pDI[6] = nLen;
	pDI[7] = 3;
	
	if((((s32)pDst)&0xC0000000)==0x80000000) DCInvalidateRange(pDst,nLen);
	while(1) {
		if(pDI[0]&0x04)
			return 0;
		if(!pDI[6])
			return 1;
	}
}

void DVD_Init()
{
	if(__dvd_initflag) return;
	__dvd_initflag = 1;

	__DVDInitWA();

	IRQ_Request(IRQ_PI_DI,__DVDInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_DI));

	LWP_InitQueue(&__dvd_wait_queue);

	_diReg[0] = (DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK);
	_diReg[1] = 0;
	
	if(!DVD_ReadId((void*)0x80000000)) return;
	DCInvalidateRange((void*)0x80000000,0x20);

}

void DVD_Reset()
{
	u32 val;
	vs32 *pDI = (vs32*)0xCC006000;

	pDI[0] = 0x2A;
	pDI[1] = 0;
	
	val = *(u32*)0xCC003024;
	val &= ~4;
	val |= 1;
	*(u32*)0xCC003024 = val;
	udelay(10);

	val |= 4;
	val |= 1;
	*(u32*)0xCC003024 = val;
	udelay(10);
}
