#include <stdlib.h>
#include <unistd.h>
#include "cache.h"
#include "dvd.h"

extern void udelay(int us);

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

s32 DVD_Init()
{
	if(!DVD_ReadId((void*)0x80000000)) return 0;
	
	DCInvalidateRange((void*)0x80000000,0x20);
	return 1;
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
