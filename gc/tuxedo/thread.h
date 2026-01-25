// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"
#include "ppc/context.h"
#include "ppc/intrinsics.h"
#include "tick.h"

#include <sys/reent.h>

#define KTHR_MAX_PRIO  0
#define KTHR_MAIN_PRIO 0x3f
#define KTHR_MIN_PRIO  0x7f

MK_EXTERN_C_START

typedef struct KThread KThread;

typedef struct KThrQueue {
	KThread* next;
	KThread* prev;
} KThrQueue;

typedef enum KThrState {
	KTHR_STATE_UNINIT,
	KTHR_STATE_FINISHED,
	KTHR_STATE_RUNNING,
	KTHR_STATE_WAITING,
	KTHR_STATE_WAITING_MUTEX,
} KThrState;

typedef sptr (*KThreadFn)(void* arg);

struct KThread {
	PPCContext ctx;

	KThread* next;

	u16 state;
	u16 suspend;
	u16 prio;
	u16 baseprio;

	KThrQueue waiters;

	union {
		// Data for waiting threads
		struct {
			KThrQueue link;
			KThrQueue* queue;
			uptr token;
		} wait;

		// Data for finished threads
		struct {
			sptr rc;
		} finish;
	};

	struct _reent r;
};

void KThreadPrepare(KThread* t, KThreadFn entrypoint, void* arg, void* stack_top, u16 prio);
void KThreadResume(KThread* t);
void KThreadSuspend(KThread* t);
void KThreadSetPrio(KThread* t, u16 prio);
sptr KThreadJoin(KThread* t);

void KThreadYield(void);
void KThreadSleepTicks(u64 ticks);
void KThreadExit(sptr rc);

MK_INLINE KThread* KThreadGetSelf(void)
{
	return (KThread*)PPCMfspr(SPRG2);
}

MK_INLINE void KThreadSleepUs(u64 us)
{
	KThreadSleepTicks(PPCUsToTicks(us));
}

MK_INLINE void KThreadSleepMs(u64 ms)
{
	KThreadSleepTicks(PPCMsToTicks(ms));
}

uptr KThrQueueBlock(KThrQueue* q, uptr token);
void KThrQueueUnblockOneByValue(KThrQueue* q, uptr ref);
void KThrQueueUnblockOneByMask(KThrQueue* q, uptr ref);
void KThrQueueUnblockAllByValue(KThrQueue* q, uptr ref);
void KThrQueueUnblockAllByMask(KThrQueue* q, uptr ref);
void KThrQueueCancel(KThrQueue* q, KThread* t);

MK_EXTERN_C_END
