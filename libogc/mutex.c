#include <stdlib.h>
#include <errno.h>
#include "asm.h"
#include "lwp_mutex.h"
#include "lwp_wkspace.h"
#include "mutex.h"

static s32 __lwp_mutex_locksupp(mutex_t mutex,u32 timeout,u8 block)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__lwp_mutex_seize((lwp_mutex*)mutex,(u32)mutex,block,timeout,level);
	return _thr_executing->wait.ret_code;
}

s32 LWP_MutexInit(mutex_t *mutex,boolean use_recursive)
{
	lwp_mutex_attr attr;
	lwp_mutex *ret;
	
	if(!mutex) return -1;

	__lwp_thread_dispatchdisable();
	ret = __lwp_mutex_allocmutex();
	if(!ret) {
		__lwp_thread_dispatchenable();
		return -1;
	}

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = use_recursive?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = 1; //__lwp_priotocore(LWP_PRIO_MAX-1);
	__lwp_mutex_initialize(ret,&attr,LWP_MUTEX_UNLOCKED);

	*mutex = (mutex_t)ret;
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_MutexDestroy(mutex_t mutex)
{
	lwp_mutex *p = (lwp_mutex*)mutex;
	__lwp_thread_dispatchdisable();
	if(__lwp_mutex_locked(p)) {
		__lwp_thread_dispatchenable();
		return EBUSY;
	}
	
	__lwp_mutex_flush(p,EINVAL);
	__lwp_mutex_freemutex(p);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_MutexLock(mutex_t mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_THREADQ_NOTIMEOUT,TRUE);
}

s32 LWP_MutexTryLock(mutex_t mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_THREADQ_NOTIMEOUT,FALSE);
}

s32 LWP_MutexUnlock(mutex_t mutex)
{
	u32 ret;
	__lwp_thread_dispatchdisable();
	ret = __lwp_mutex_surrender((lwp_mutex*)mutex);
	__lwp_thread_dispatchenable();
	return ret;
}
