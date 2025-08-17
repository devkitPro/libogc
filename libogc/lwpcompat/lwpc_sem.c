#include "lwpc_common.h"
#include <ogc/semaphore.h>

static u64 lwpc_sem_used;
static s32 lwpc_sem_table[64];
static KThrQueue lwpc_sem_queue;

s32 LWP_SemInit(sem_t* sem, u32 start, u32 max)
{
	int slot = lwpc_alloc_slot(&lwpc_sem_used);
	if (slot < 0) {
		*sem = LWP_SEM_NULL;
		return -1;
	}

	*sem = slot+1;
	lwpc_sem_table[slot] = start;
	return 0;
}

s32 LWP_SemDestroy(sem_t sem)
{
	if (!sem || sem == LWP_SEM_NULL) {
		return -1;
	}

	lwpc_free_slot(&lwpc_sem_used, sem-1);
	return 0;
}

s32 LWP_SemWait(sem_t sem)
{
	if (!sem || sem == LWP_SEM_NULL) {
		return -1;
	}

	s32* obj = &lwpc_sem_table[sem-1];
	PPCIrqState st = PPCIrqLockByMsr();

	if (--(*obj) < 0) {
		KThrQueueBlock(&lwpc_sem_queue, sem);
	}

	PPCIrqUnlockByMsr(st);
	return 0;
}

s32 LWP_SemGetValue(sem_t sem, u32* value)
{
	if (!sem || sem == LWP_SEM_NULL) {
		return -1;
	}

	s32* obj = &lwpc_sem_table[sem-1];
	*value = *obj >= 0 ? *obj : 0;
	return 0;
}

s32 LWP_SemPost(sem_t sem)
{
	if (!sem || sem == LWP_SEM_NULL) {
		return -1;
	}

	s32* obj = &lwpc_sem_table[sem-1];
	PPCIrqState st = PPCIrqLockByMsr();

	if ((*obj)++ < 0) {
		KThrQueueUnblockOneByValue(&lwpc_sem_queue, sem);
	}

	PPCIrqUnlockByMsr(st);
	return -1;
}
