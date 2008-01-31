/*-------------------------------------------------------------

cache.c -- Cache interface

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

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


#include <asm.h>
#include <processor.h>
#include "cache.h"

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

extern void __LCEnable();

void LCEnable()
{
	u32 level;

	_CPU_ISR_Disable(level);
	__LCEnable();
	_CPU_ISR_Restore(level);
}

u32 LCLoadData(void *dstAddr,void *srcAddr,u32 nCount)
{
	u32 cnt,blocks;

	if((s32)nCount<=0) return 0;

	cnt = (nCount+31)>>5;
	blocks = (cnt+127)>>7;
	while(cnt) {
		if(cnt<0x80) {
			LCLoadBlocks(dstAddr,srcAddr,cnt);
			cnt = 0;
			break;
		}
		LCLoadBlocks(dstAddr,srcAddr,0);
		cnt -= 128;
		dstAddr += 4096;
		srcAddr += 4096;
	}
	return blocks;
}

u32 LCStoreData(void *dstAddr,void *srcAddr,u32 nCount)
{
	u32 cnt,blocks;

	if((s32)nCount<=0) return 0;

	cnt = (nCount+31)>>5;
	blocks = (cnt+127)>>7;
	while(cnt) {
		if(cnt<0x80) {
			LCStoreBlocks(dstAddr,srcAddr,cnt);
			cnt = 0;
			break;
		}
		LCStoreBlocks(dstAddr,srcAddr,0);
		cnt -= 128;
		dstAddr += 4096;
		srcAddr += 4096;
	}
	return blocks;
}

u32 LCQueueLength()
{
	u32 hid2 = mfspr(920);
	return _SHIFTR(hid2,4,4);
}

u32 LCQueueWait(u32 len)
{
	len++;
	while(_SHIFTR(mfspr(920),4,4)>=len);
	return len;
}

void LCFlushQueue()
{
	mtspr(922,0);
	mtspr(923,1);
	ppcsync();
}

void LCAlloc(void *addr,u32 bytes)
{
	u32 level,cnt,hid2;

	cnt = bytes>>5;
	hid2 = mfspr(920);
	if(!(hid2&0x10000000)) {
		_CPU_ISR_Disable(level);
		__LCEnable();
		_CPU_ISR_Restore(level);
	}
	LCAllocTags(TRUE,addr,cnt);
}

void LCAllocNoInvalidate(void *addr,u32 bytes)
{
	u32 level,cnt,hid2;

	cnt = bytes>>5;
	hid2 = mfspr(920);
	if(!(hid2&0x10000000)) {
		_CPU_ISR_Disable(level);
		__LCEnable();
		_CPU_ISR_Restore(level);
	}
	LCAllocTags(FALSE,addr,cnt);
}
