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
	return (254 - prio);
}

u32 LWP_CreateThread(lwp_t *thethread,void* (*entry)(void *),void *arg,void *stackbase,u32 stack_size,u8 prio)
{
	u32 status;
	lwp_cntrl *lwp_thread;
	
	__lwp_thread_dispatchdisable();

	lwp_thread = (lwp_cntrl*)__lwp_wkspace_allocate(sizeof(lwp_cntrl));
	if(!lwp_thread) {
		__lwp_thread_dispatchenable();
		return -1;
	}

	status = __lwp_thread_init(lwp_thread,stackbase,stack_size,__lwp_priotocore(prio),0);
	if(!status) {
		__lwp_wkspace_free(lwp_thread);
		__lwp_thread_dispatchenable();
		return -1;
	}
	
	status = __lwp_thread_start(lwp_thread,entry,arg);
	if(!status) {
		__lwp_wkspace_free(lwp_thread);
		__lwp_thread_dispatchenable();
		return -1;
	}

	*thethread = lwp_thread;
	__lwp_thread_dispatchenable();

	return 0;
}

u32 LWP_SuspendThread(lwp_t thethread)
{
	lwp_cntrl *lwp_thread = (lwp_cntrl*)thethread;

	if(lwp_thread==_thr_executing) __lwp_thread_dispatchdisable();
	if(!__lwp_statesuspended(lwp_thread->cur_state)) {
		__lwp_thread_suspend(lwp_thread);
		__lwp_thread_dispatchenable();
		return LWP_SUCCESSFUL;
	}
	__lwp_thread_dispatchenable();
	return LWP_ALLREADY_SUSPENDED;
}

u32 LWP_ResumeThread(lwp_t thethread)
{
	lwp_cntrl *lwp_thread = (lwp_cntrl*)thethread;

	if(lwp_thread==_thr_executing) __lwp_thread_dispatchdisable();
	if(__lwp_statesuspended(lwp_thread->cur_state)) {
		__lwp_thread_resume(lwp_thread,TRUE);
		__lwp_thread_dispatchenable();
		return LWP_SUCCESSFUL;
	}
	__lwp_thread_dispatchenable();
	return LWP_NOT_SUSPENDED;
}

lwp_t LWP_GetSelf()
{
	return (lwp_t)_thr_executing;
}

void LWP_SetThreadPriority(lwp_t thethread,u32 prio)
{
	lwp_cntrl *lwp_thread = NULL;
	if(thethread==NULL) lwp_thread = _thr_executing;
	else lwp_thread = (lwp_cntrl*)thethread;
	__lwp_thread_changepriority(lwp_thread,__lwp_priotocore(prio),FALSE);
}

void LWP_YieldThread()
{
	__lwp_thread_dispatchdisable();
	__lwp_thread_yield();
	__lwp_thread_dispatchenable();
}

void LWP_CloseThread(lwp_t thethread)
{
	lwp_cntrl *lwp_thread = (lwp_cntrl*)thethread;
	if(!lwp_thread) return;
	__lwp_thread_close(lwp_thread);
}

u32 LWP_JoinThread(lwp_t thethread,void **value_ptr)
{
	u32 level;
	void *return_ptr;
	lwp_obj *object;
	lwp_cntrl *exec,*lwp_thread = (lwp_cntrl*)thethread;
	
	__lwp_thread_dispatchdisable();

	object = __lwp_thread_getobject(lwp_thread);
	if(!object) {
		__lwp_thread_dispatchenable();
		return 0;
	}

	if(__lwp_thread_isexec(lwp_thread)) {
		__lwp_thread_dispatchenable();
		return EDEADLK;			//EDEADLK
	}

	exec = _thr_executing;
	_CPU_ISR_Disable(level);
	__lwp_threadqueue_csenter(&object->join_list);
	exec->wait.ret_arg = (void*)&return_ptr;
	_CPU_ISR_Restore(level);
	__lwp_threadqueue_enqueue(&object->join_list,LWP_WD_NOTIMEOUT);
	__lwp_thread_dispatchenable();

	if(value_ptr)
		*value_ptr = return_ptr;
	
	return 0;
}

void LWP_InitQueue(lwpq_t *thequeue)
{
	lwp_thrqueue *tqueue;

	__lwp_thread_dispatchdisable();
	tqueue = (lwp_thrqueue*)__lwp_wkspace_allocate(sizeof(lwp_thrqueue));
	if(!tqueue) {
		__lwp_thread_dispatchenable();
		return;
	}
	__lwp_threadqueue_init(tqueue,LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_ON_THREADQ,0);

	*thequeue = tqueue;
	__lwp_thread_dispatchenable();
}

void LWP_CloseQueue(lwpq_t thequeue)
{
	LWP_WakeThread(thequeue);
	__lwp_wkspace_free(thequeue);
}

u32 LWP_SleepThread(lwpq_t thequeue)
{
	u32 level;
	lwp_cntrl *exec = NULL;
	lwp_thrqueue *tqueue = (lwp_thrqueue*)thequeue;

	__lwp_thread_dispatchdisable();
	if(__lwp_isr_in_progress()) {
		__lwp_thread_dispatchenable();
		return -1;
	}

	exec = _thr_executing;
	_CPU_ISR_Disable(level);
	__lwp_threadqueue_csenter(tqueue);
	exec->wait.ret_code = 0;
	exec->wait.queue = tqueue;
	exec->wait.id = LWP_SLEEP_THREAD;
	_CPU_ISR_Restore(level);
	__lwp_threadqueue_enqueue(tqueue,LWP_THREADQ_NOTIMEOUT);
	__lwp_thread_dispatchenable();

	return 0;
}

void LWP_WakeThread(lwpq_t thequeue)
{
	lwp_cntrl *thethread;
	lwp_thrqueue *tqueue = (lwp_thrqueue*)thequeue;

	__lwp_thread_dispatchdisable();
	do {
		thethread = __lwp_threadqueue_dequeue(tqueue);
	} while(thethread);
	__lwp_thread_dispatchenable();
}
