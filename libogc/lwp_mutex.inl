#ifndef __LWP_MUTEX_INL__
#define __LWP_MUTEX_INL__

#include "lwp_mutex.h"
#include "lwp_threads.h"

#include "lwp_threads.inl"
#include "lwp_threadq.inl"

static __inline__ u32 __lwp_mutex_locked(lwp_mutex *mutex)
{
	return (mutex->lock==LWP_MUTEX_LOCKED);
}

static __inline__ u32 __lwp_mutex_ispriority(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_PRIORITY);
}

static __inline__ u32 __lwp_mutex_isfifo(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_FIFO);
}

static __inline__ u32 __lwp_mutex_isinheritprio(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_INHERITPRIO);
}

static __inline__ u32 __lwp_mutex_isprioceiling(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_PRIORITYCEIL);
}

static __inline__ u32 __lwp_mutex_seize_irq_trylock(lwp_mutex *mutex,u32 *isr_level)
{
	lwp_cntrl *exec;
	u32 level = *isr_level;

	exec = _thr_executing;
	exec->wait.ret_code = LWP_MUTEX_SUCCESSFUL;
	if(!__lwp_mutex_locked(mutex)) {
		mutex->lock = LWP_MUTEX_LOCKED;
		mutex->holder = exec;
		mutex->nest_cnt = 1;
		if(__lwp_mutex_isinheritprio(&mutex->atrrs) || __lwp_mutex_isprioceiling(&mutex->atrrs))
			exec->res_cnt++;
		if(!__lwp_mutex_isprioceiling(&mutex->atrrs)) {
			_CPU_ISR_Restore(level);
			return 0;
		}
		{
			u32 prioceiling,priocurr;
			
			prioceiling = mutex->atrrs.prioceil;
			priocurr = exec->cur_prio;
			if(priocurr==prioceiling) {
				_CPU_ISR_Restore(level);
				return 0;
			}
			if(priocurr>prioceiling) {
				__lwp_thread_dispatchdisable();
				_CPU_ISR_Restore(level);
				__lwp_thread_changepriority(mutex->holder,mutex->atrrs.prioceil,false);
				__lwp_thread_dispatchenable();
				return 0;
			}
			exec->wait.ret_code = LWP_MUTEX_CEILINGVIOL;
			mutex->nest_cnt = 0;
			exec->res_cnt--;
			_CPU_ISR_Restore(level);
			return 0;
		}
		return 0;
	}

	if(__lwp_thread_isexec(mutex->holder)) {
		switch(mutex->atrrs.nest_behavior) {
			case LWP_MUTEX_NEST_ACQUIRE:
				mutex->nest_cnt++;
				_CPU_ISR_Restore(level);
				return 0;
			case LWP_MUTEX_NEST_ERROR:
				exec->wait.ret_code = LWP_MUTEX_NEST_NOTALLOWED;
				_CPU_ISR_Restore(level);
				return 0;
			case LWP_MUTEX_NEST_BLOCK:
				break;
		}
	}
	return 1;
}

static __inline__ void __lwp_mutex_seize(lwp_mutex *mutex,s32 id,u8 wait,u32 timeout,u32 level)
{
	if(__lwp_mutex_seize_irq_trylock(mutex,&level)) {
		if(!wait) {
			_CPU_ISR_Restore(level);
			_thr_executing->wait.ret_code = LWP_MUTEX_UNSATISFIED_NOWAIT;
		} else {
			__lwp_threadqueue_csenter(&(mutex)->wait_queue);
			_thr_executing->wait.queue = &(mutex)->wait_queue;
			_thr_executing->wait.id = id;
			__lwp_thread_dispatchdisable();
			_CPU_ISR_Restore(level);
			__lwp_mutex_seize_irq_blocking(mutex,(u64)timeout);
		}
	}
}

#endif
