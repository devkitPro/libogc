#include <stdlib.h>
#include <errno.h>
#include "asm.h"
#include "lwp_mutex.h"
#include "lwp_wkspace.h"
#include "mutex.h"

static u32 _g_mutex_id = 0;

static u32 __lwp_mutex_locksupp(mutex_t *mutex,u32 timeout,u8 block)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__lwp_mutex_seize((lwp_mutex*)mutex->mutex,mutex->id,block,timeout,level);
	return _thr_executing->wait.ret_code;
}

u32 LWP_MutexInit(mutex_t *mutex,boolean use_recursive)
{
	lwp_mutex_attr attr;
	lwp_mutex *ret;
	
	if(!mutex) return -1;
	if(mutex->mutex) return 0;

	__lwp_thread_dispatchdisable();
	ret = (lwp_mutex*)__lwp_wkspace_allocate(sizeof(lwp_mutex));
	if(!ret) {
		__lwp_thread_dispatchunnest();
		return -1;
	}

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = use_recursive?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = 1; //__lwp_priotocore(LWP_PRIO_MAX-1);
	__lwp_mutex_initialize(ret,&attr,LWP_MUTEX_UNLOCKED);

	mutex->id = ++_g_mutex_id;
	mutex->mutex = (void*)ret;
	__lwp_thread_dispatchunnest();
	return 0;
}

u32 LWP_MutexDestroy(mutex_t *mutex)
{
	__lwp_thread_dispatchdisable();
	if(__lwp_mutex_locked((lwp_mutex*)mutex->mutex)) {
		__lwp_thread_dispatchenable();
		return EBUSY;
	}
	
	__lwp_mutex_flush((lwp_mutex*)mutex->mutex,EINVAL);
	__lwp_wkspace_free(mutex->mutex);

	__lwp_thread_dispatchenable();
	return 0;
}

u32 LWP_MutexLock(mutex_t *mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_THREADQ_NOTIMEOUT,TRUE);
}

u32 LWP_MutexTryLock(mutex_t *mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_THREADQ_NOTIMEOUT,FALSE);
}

u32 LWP_MutexUnlock(mutex_t *mutex)
{
	u32 ret;
	__lwp_thread_dispatchdisable();
	ret = __lwp_mutex_surrender((lwp_mutex*)mutex->mutex);
	__lwp_thread_dispatchenable();
	return ret;
}
