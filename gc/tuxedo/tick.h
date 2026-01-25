// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "ppc/intrinsics.h"
#include "ppc/clock.h"

MK_EXTERN_C_START

typedef struct KTickTask KTickTask;
typedef void (* KTickTaskFn)(KTickTask*);

struct KTickTask {
	KTickTask* next;
	KTickTaskFn fn;
	u64 target;
	u64 period;
};

void KTickTaskStart(KTickTask* t, KTickTaskFn fn, u64 delay_ticks, u64 period_ticks);
void KTickTaskStop(KTickTask* t);

MK_EXTERN_C_END
