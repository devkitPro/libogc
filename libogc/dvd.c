#include <stdlib.h>
#include <unistd.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "lwp.h"
#include "irq.h"
#include "dvd.h"

#define DVD_BRK				(1<<0)
#define DVD_DE_MSK			(1<<1)
#define DVD_DE_INT			(1<<2)
#define DVD_TC_MSK			(1<<3)
#define DVD_TC_INT			(1<<4)
#define DVD_BRK_MSK			(1<<5)
#define DVD_BRK_INT			(1<<6)

static u32 __DVDInitFlag = 0;
static u32 __DVDStopNextInt = 0;
static u32 __DVDNextCmdNum;
static u32 __DVDWorkaround;
static u32 __DVDWorkaroundSeek;

static lwpq_t __dvd_wait_queue;

static u32* const _diReg = (u32*)0xCC006000;

extern void udelay(int us);

extern void __MaskIrq(u32);
extern void __UnmaskIrq(u32);

static void __DVDLowWATypeSet(u32 workaround,u32 workaroundseek)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__DVDWorkaround = workaround;
	__DVDWorkaroundSeek = workaroundseek;
	_CPU_ISR_Restore(level);
}

static void __DVDInitWA()
{
	__DVDNextCmdNum = 0;
	__DVDLowWATypeSet(0,0);	
}

static void __DVDInterruptHandler(u32 nIrq,void *pCtx)
{
	u32 status,ir,imr,immr;

	immr = 0;
	status = _diReg[0];
	imr = (status&(DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK));
	ir = ((status&(DVD_DE_INT|DVD_TC_INT|DVD_BRK_INT))&(imr<<1));


	if(ir&DVD_DE_INT) immr |= DVD_DE_INT;
	if(ir&DVD_TC_INT) immr |= DVD_TC_INT;
	if(ir&DVD_BRK_INT) immr |= DVD_BRK_INT;

	_diReg[0] = (ir|imr);
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
	if(__DVDInitFlag) return;
	__DVDInitFlag = 1;

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
