// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <string.h>
#include <tuxedo/thread.h>

extern KThread* s_firstThread;

typedef enum KThrUnblockMode {
	KTHR_UNBLOCK_ANY,
	KTHR_UNBLOCK_BY_VALUE,
	KTHR_UNBLOCK_BY_MASK,
} KThrUnblockMode;

MK_INLINE KThread* KThreadGetInsertPosition(KThread* t)
{
	KThread* pos = NULL;
	for (KThread* cur = s_firstThread; cur && cur->prio <= t->prio; cur = cur->next) {
		pos = cur;
	}
	return pos;
}

MK_INLINE KThread* KThreadGetPrevious(KThread* t)
{
	for (KThread* cur = s_firstThread; cur; cur = cur->next) {
		if (cur->next == t) {
			return cur;
		}
	}
	return NULL;
}

MK_INLINE void KThreadEnqueue(KThread* t)
{
	KThread* pos = KThreadGetInsertPosition(t);
	if (pos) {
		t->next = pos->next;
		pos->next = t;
	} else {
		t->next = s_firstThread;
		s_firstThread = t;
	}
}

MK_INLINE void KThreadDequeue(KThread* t)
{
	if (s_firstThread == t) {
		s_firstThread = t->next;
	} else {
		KThread* prev = KThreadGetPrevious(t);
		if (prev) {
			prev->next = t->next;
		}
	}
}

MK_INLINE KThread* KThreadFindRunnable(KThread* first)
{
	KThread* t;
	for (t = first; t && t->state != KTHR_STATE_RUNNING; t = t->next);
	return t;
}

MK_INLINE KThread* KThrQueueGetInsertPosition(KThrQueue* queue, KThread* t)
{
	KThread* pos;
	for (pos = queue->next; pos && pos->prio <= t->prio; pos = pos->wait.link.next);
	return pos ? pos->wait.link.prev : queue->prev;
}

MK_INLINE void KThrQueueInsert(KThrQueue* queue, KThread* t)
{
	KThread* pos = KThrQueueGetInsertPosition(queue, t);
	if (pos) {
		t->wait.link.next   = pos->wait.link.next;
		t->wait.link.prev   = pos;
		pos->wait.link.next = t;
	} else {
		t->wait.link.next   = queue->next;
		t->wait.link.prev   = NULL;
		queue->next    = t;
	}

	if (t->wait.link.next) {
		t->wait.link.next->wait.link.prev = t;
	} else {
		queue->prev = t;
	}

	t->wait.queue = queue;
}

MK_INLINE void KThrQueueRemove(KThrQueue* queue, KThread* t)
{
	if (t->wait.queue != queue) {
		return;
	}

	t->wait.queue = NULL;
	(t->wait.link.prev ? &t->wait.link.prev->wait.link : queue)->next = t->wait.link.next;
	(t->wait.link.next ? &t->wait.link.next->wait.link : queue)->prev = t->wait.link.prev;
}

MK_INLINE bool KThrQueueTestUnblock(KThread* t, KThrUnblockMode mode, u32 ref)
{
	switch (mode) {
		default:
		case KTHR_UNBLOCK_ANY:
			return true;
		case KTHR_UNBLOCK_BY_VALUE:
			return t->wait.token == ref;
		case KTHR_UNBLOCK_BY_MASK:
			return (t->wait.token & ref) != 0;
	}
}

void KThreadSwitchTo(KThread* t, PPCIrqState st);

MK_INLINE void KThreadReschedule(KThread* t, PPCIrqState st)
{
	if (t && t->prio < KThreadGetSelf()->prio) {
		KThreadSwitchTo(t, st);
	} else {
		PPCIrqUnlockByMsr(st);
	}
}

void KThreadUpdateDynamicPrio(KThread* t);
