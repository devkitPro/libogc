#pragma once
#include <tuxedo/thread.h>
#include <tuxedo/sync.h>

MK_INLINE int lwpc_alloc_slot(u64* used)
{
	PPCIrqState st = PPCIrqLockByMsr();
	int slot = __builtin_ffsll(~*used)-1;
	if (slot >= 0) {
		*used |= 1ULL << slot;
	}
	PPCIrqUnlockByMsr(st);

	return slot;
}

MK_INLINE void lwpc_free_slot(u64* used, int slot)
{
	PPCIrqState st = PPCIrqLockByMsr();
	*used &= ~(1ULL << slot);
	PPCIrqUnlockByMsr(st);
}
