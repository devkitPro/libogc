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
