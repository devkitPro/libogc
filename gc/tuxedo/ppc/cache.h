#pragma once

#define PPC_L1_CACHE_SZ         0x8000
#define PPC_L1_CACHE_LOG2       15
#define PPC_L1_CACHE_LINE_SZ    0x20
#define PPC_L1_CACHE_LINE_LOG2  5
#define PPC_L1_CACHE_WAYS       8
#define PPC_L1_CACHE_WAYS_LOG2  3

#define PPC_L2_CACHE_SZ         0x40000
#define PPC_L2_CACHE_LOG2       18
#define PPC_L2_CACHE_WAYS       2
#define PPC_L2_CACHE_WAYS_LOG2  1

#ifndef __ASSEMBLER__
#include "intrinsics.h"

MK_EXTERN_C_START

void PPCDCacheFlushAsync(const volatile void* buf, size_t size);

MK_INLINE void PPCDCacheFlush(const volatile void* buf, size_t size)
{
	PPCDCacheFlushAsync(buf, size);
	PPCSync();
}

void PPCDCacheStoreAsync(const volatile void* buf, size_t size);

MK_INLINE void PPCDCacheStore(const volatile void* buf, size_t size)
{
	PPCDCacheStoreAsync(buf, size);
	PPCSync();
}

void PPCDCacheTouch(const volatile void* buf, size_t size);

void PPCDCacheZero(const volatile void* buf, size_t size);

void PPCDCacheInvalidate(const volatile void* buf, size_t size);

void PPCICacheInvalidate(const volatile void* buf, size_t size);

MK_EXTERN_C_END

#endif
