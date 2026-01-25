// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <tuxedo/ppc/cache.h>

// GCC generates *very* stupid code for these functions unless we disable loop unrolling...
#pragma GCC optimize("no-unroll-loops")

MK_INLINE unsigned _PPCCalcNumCacheLines(const volatile void* buf, size_t size)
{
	uintptr_t addr = (uintptr_t)buf &~ (PPC_L1_CACHE_LINE_SZ - 1);
	uintptr_t end = (uintptr_t)buf + size + PPC_L1_CACHE_LINE_SZ - 1;
	return (end - addr) >> PPC_L1_CACHE_LINE_LOG2;
}

#define MAKE_CACHE_FUNC(_name, _ppcopcode) \
void _name(const volatile void* buf, size_t size) \
{ \
	unsigned count = _PPCCalcNumCacheLines(buf, size); \
	for (unsigned i = 0; i < count; i ++) { \
		_ppcopcode((uptr)buf); \
		buf += PPC_L1_CACHE_LINE_SZ; \
	} \
}

MAKE_CACHE_FUNC(PPCDCacheFlushAsync, PPCDcbf)
MAKE_CACHE_FUNC(PPCDCacheStoreAsync, PPCDcbst)
MAKE_CACHE_FUNC(PPCDCacheTouch,      PPCDcbt)
MAKE_CACHE_FUNC(PPCDCacheZero,       PPCDcbz)
MAKE_CACHE_FUNC(PPCDCacheInvalidate, PPCDcbi)
MAKE_CACHE_FUNC(PPCICacheInvalidate, PPCIcbi)
