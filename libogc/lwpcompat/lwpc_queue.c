#include "lwpc_common.h"
#include <ogc/lwp.h>

static KThrQueue lwpc_queue;
static u64 lwpc_queue_used;

s32 LWP_InitQueue(lwpq_t* thequeue)
{
	int slot = lwpc_alloc_slot(&lwpc_queue_used);

	if (slot >= 0) {
		*thequeue = slot+1;
		return 0;
	} else {
		*thequeue = LWP_TQUEUE_NULL;
		return -1;
	}
}

void LWP_CloseQueue(lwpq_t thequeue)
{
	if (thequeue && thequeue != LWP_TQUEUE_NULL) {
		lwpc_free_slot(&lwpc_queue_used, thequeue-1);
	}
}

s32 LWP_ThreadSleep(lwpq_t thequeue)
{
	if (thequeue && thequeue != LWP_TQUEUE_NULL) {
		KThrQueueBlock(&lwpc_queue, thequeue);
		return 0;
	}

	return -1;
}

void LWP_ThreadSignal(lwpq_t thequeue)
{
	if (thequeue && thequeue != LWP_TQUEUE_NULL) {
		KThrQueueUnblockOneByValue(&lwpc_queue, thequeue);
	}
}

void LWP_ThreadBroadcast(lwpq_t thequeue)
{
	if (thequeue && thequeue != LWP_TQUEUE_NULL) {
		KThrQueueUnblockAllByValue(&lwpc_queue, thequeue);
	}
}
