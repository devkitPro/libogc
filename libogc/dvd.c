#include <stdlib.h>
#include <unistd.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "dvd.h"

static u32 __DVDInitFlag = 0;
static u32 __DVDNextCmdNum;
static u32 __DVDWorkaround;
static u32 __DVDWorkaroundSeek;

static u32* const _diReg = (u32*)0xCC006000;

extern void udelay(int us);

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
