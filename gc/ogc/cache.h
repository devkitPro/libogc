#ifndef __CACHE_H__
#define __CACHE_H__

#include <gctypes.h>

#define LC_BASEPREFIX		0xe000
#define LC_BASE				(LC_BASEPREFIX<<16)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void DCEnable();
void DCDisable();
void DCFreeze();
void DCUnfreeze();
void DCFlashInvalidate();
void DCInvalidateRange(void *,u32);
void DCFlushRange(void *,u32);
void DCStoreRange(void *,u32);
void DCFlushRangeNoSync(void *,u32);
void DCStoreRangeNoSync(void *,u32);
void DCZeroRange(void *,u32);
void DCTouchRange(void *,u32);

void ICSync();
void ICFlashInvalidate();
void ICEnable();
void ICDisable();
void ICFreeze();
void ICUnfreeze();
void ICBlockInvalidate(void *);
void ICInvalidateRange(void *,u32);

void LCEnable();
void LCDisable();
void LCLoadBlocks(void *,void *,u32);
void LCStoreBlocks(void *,void *,u32);
u32 LCLoadData(void *,void *,u32);
u32 LCStoreData(void *,void *,u32);
u32 LCQueueLength();
u32 LCQueueWait(u32);
void LCFlushQueue();
void LCAlloc(void *,u32);
void LCAllocNoInvalidate(void *,u32);
void LCAllocOneTag(BOOL,void *);
void LCAllocTags(BOOL,void *,u32);

#define LCGetBase()		((void*)LC_BASE)
#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
