#include <stdlib.h>
#include <errno.h>
#include "lwp_sema.h"
#include "lwp_wkspace.h"
#include "semaphore.h"

static u32 __lwp_semwaitsupp(lwp_sema *sem,u32 id,u32 timeout,u8 blocking)
{
	if(!sem) return -1;
	
	__lwp_thread_dispatchdisable();
	__lwp_sema_seize(sem,id,blocking,timeout);
	__lwp_thread_dispatchenable();

	switch(_thr_executing->wait.ret_code) {
		case LWP_SEMA_SUCCESSFUL:
			break;
		case LWP_SEMA_UNSATISFIED_NOWAIT:
			return EAGAIN;
		case LWP_SEMA_DELETED:
			return EAGAIN;
		case LWP_SEMA_TIMEOUT:
			return ETIMEDOUT;
			
	}
	return 0;
}

u32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
{
	lwp_semattr attr;
	lwp_sema *ret;

	if(!sem) return -1;
	*sem = NULL;
	
	__lwp_thread_dispatchdisable();
	ret = (lwp_sema*)__lwp_wkspace_allocate(sizeof(lwp_sema));
	if(!ret) {
		__lwp_thread_dispatchunnest();
		return -1;
	}

	attr.max_cnt = max;
	attr.mode = LWP_SEMA_MODEFIFO;
	__lwp_sema_initialize(ret,&attr,start);

	*sem = (void*)ret;
	__lwp_thread_dispatchunnest();
	return 0;
}

u32 LWP_SemWait(sem_t sem)
{
	return __lwp_semwaitsupp((lwp_sema*)sem,(u32)sem,LWP_THREADQ_NOTIMEOUT,TRUE);
}

u32 LWP_SemPost(sem_t sem)
{
	if(!sem) return -1;

	__lwp_thread_dispatchdisable();
	__lwp_sema_surrender((lwp_sema*)sem,(u32)sem);
	__lwp_thread_dispatchenable();
	return 0;
}

u32 LWP_SemDestroy(sem_t sem)
{
	lwp_sema *p = (lwp_sema*)sem;
	if(!p) return -1;

	__lwp_thread_dispatchdisable();
	__lwp_sema_flush(p,-1);
	__lwp_wkspace_free(p);
	__lwp_thread_dispatchenable();

	return 0;
}
