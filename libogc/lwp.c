#include <stdlib.h>
#include <errno.h>
#include "asm.h"
#include "processor.h"
#include "lwp_threadq.h"
#include "lwp_threads.h"
#include "lwp_wkspace.h"
#include "lwp.h"

#define LWP_SLEEP_THREAD		240

static __inline__ u32 __lwp_priotocore(u32 prio)
{
	return (255 - prio);
}

s32 LWP_CreateThread(lwp_t *thethread,void* (*entry)(void *),void *arg,void *stackbase,u32 stack_size,u8 prio)
{
	u32 status,level;
	lwp_cntrl *lwp_thread;
	
	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();

	lwp_thread = __lwp_thread_alloclwp();
	if(!lwp_thread) {
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return -1;
	}

	status = __lwp_thread_init(lwp_thread,stackbase,stack_size,__lwp_priotocore(prio),0,TRUE);
	if(!status) {
		__lwp_thread_freelwp(lwp_thread);
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return -1;
	}
	
	status = __lwp_thread_start(lwp_thread,entry,arg);
	if(!status) {
		__lwp_thread_freelwp(lwp_thread);
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return -1;
	}

	*thethread = lwp_thread;
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
	return 0;
}

s32 LWP_SuspendThread(lwp_t thethread)
{
	u32 level;
	lwp_cntrl *lwp_thread = (lwp_cntrl*)thethread;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	if(!__lwp_statesuspended(lwp_thread->cur_state)) {
		__lwp_thread_suspend(lwp_thread);
		__lwp_thread_dispatchenable();
		_CPU_ISR_Restore(level);
		return LWP_SUCCESSFUL;
	}
	__lwp_thread_dispatchunnest();
	_CPU_ISR_Restore(level);
	return LWP_ALLREADY_SUSPENDED;
}

s32 LWP_ResumeThread(lwp_t thethread)
{
	u32 level;
	lwp_cntrl *lwp_thread = (lwp_cntrl*)thethread;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	if(__lwp_statesuspended(lwp_thread->cur_state)) {
		__lwp_thread_resume(lwp_thread,TRUE);
		__lwp_thread_dispatchenable();
		_CPU_ISR_Restore(level);
		return LWP_SUCCESSFUL;
	}
	__lwp_thread_dispatchunnest();
	_CPU_ISR_Restore(level);
	return LWP_NOT_SUSPENDED;
}

lwp_t LWP_GetSelf()
{
	return (lwp_t)_thr_executing;
}

void LWP_SetThreadPriority(lwp_t thethread,u32 prio)
{
	u32 level;
	lwp_cntrl *lwp_thread = NULL;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	if(thethread==NULL) lwp_thread = _thr_executing;
	else lwp_thread = (lwp_cntrl*)thethread;
	__lwp_thread_changepriority(lwp_thread,__lwp_priotocore(prio),TRUE);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
}

void LWP_YieldThread()
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	__lwp_thread_yield();
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
}

void LWP_Reschedule(u32 prio)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	__lwp_rotate_readyqueue(prio);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
}

s32 LWP_ThreadIsSuspended(lwp_t thethread)
{
	u32 level;
	s32 state;

	_CPU_ISR_Disable(level);
    lwp_cntrl *lwp_thread = ( lwp_cntrl*)thethread;
    state = __lwp_statesuspended(lwp_thread->cur_state) ? TRUE : FALSE;
	_CPU_ISR_Restore(level);
	return state;
}


s32 LWP_JoinThread(lwp_t thethread,void **value_ptr)
{
	u32 level;
	void *return_ptr;
	lwp_obj *object;
	lwp_cntrl *exec,*lwp_thread = (lwp_cntrl*)thethread;
	
	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();

	object = __lwp_thread_getobject(lwp_thread);
	if(!object || lwp_thread->id==-1) {
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(__lwp_thread_isexec(lwp_thread)) {
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return EDEADLK;			//EDEADLK
	}

	exec = _thr_executing;
	__lwp_threadqueue_csenter(&lwp_thread->join_list);
	exec->wait.ret_arg = (void*)&return_ptr;
	__lwp_threadqueue_enqueue(&lwp_thread->join_list,LWP_WD_NOTIMEOUT);
	__lwp_thread_dispatchenable();

	if(value_ptr)
		*value_ptr = return_ptr;
	_CPU_ISR_Restore(level);

	return 0;
}

void LWP_InitQueue(lwpq_t *thequeue)
{
	u32 level;
	lwp_thrqueue *tqueue;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	tqueue = (lwp_thrqueue*)__lwp_wkspace_allocate(sizeof(lwp_thrqueue));
	if(!tqueue) {
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return;
	}
	__lwp_threadqueue_init(tqueue,LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_ON_THREADQ,0);

	*thequeue = tqueue;
	__lwp_thread_dispatchunnest();
	_CPU_ISR_Restore(level);
}

void LWP_CloseQueue(lwpq_t thequeue)
{
	u32 level;

	_CPU_ISR_Disable(level);
	LWP_WakeThread(thequeue);
	__lwp_wkspace_free(thequeue);
	_CPU_ISR_Restore(level);
}

s32 LWP_SleepThread(lwpq_t thequeue)
{
	u32 level;
	lwp_cntrl *exec = NULL;
	lwp_thrqueue *tqueue = (lwp_thrqueue*)thequeue;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	if(__lwp_isr_in_progress()) {
		__lwp_thread_dispatchunnest();
		_CPU_ISR_Restore(level);
		return -1;
	}

	exec = _thr_executing;
	__lwp_threadqueue_csenter(tqueue);
	exec->wait.ret_code = 0;
	exec->wait.ret_arg = NULL;
	exec->wait.ret_arg_1 = NULL;
	exec->wait.queue = tqueue;
	exec->wait.id = LWP_SLEEP_THREAD;
	__lwp_threadqueue_enqueue(tqueue,LWP_THREADQ_NOTIMEOUT);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);

	return 0;
}

void LWP_WakeThread(lwpq_t thequeue)
{
	u32 level;
	lwp_cntrl *thethread;
	lwp_thrqueue *tqueue = (lwp_thrqueue*)thequeue;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	do {
		thethread = __lwp_threadqueue_dequeue(tqueue);
	} while(thethread);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
}
