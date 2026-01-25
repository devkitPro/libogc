// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"

MK_EXTERN_C_START

typedef struct KMailbox {
	uptr* slots;
	u16 num_slots;
	u16 cur_slot;
	u16 pending_slots;
	u8 send_waiters;
	u8 recv_waiters;
} KMailbox;

MK_INLINE void KMailboxPrepare(KMailbox* mb, uptr* slots, unsigned num_slots)
{
	mb->slots = slots;
	mb->num_slots = num_slots;
	mb->cur_slot = 0;
	mb->pending_slots = 0;
	mb->send_waiters = 0;
	mb->recv_waiters = 0;
}

bool KMailboxTrySend(KMailbox* mb, uptr message);

bool KMailboxTryRecv(KMailbox* mb, uptr* out);

uptr KMailboxRecv(KMailbox* mb);

MK_EXTERN_C_END
