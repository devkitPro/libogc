/*-------------------------------------------------------------

$Id: cond.c,v 1.10 2005-11-21 12:15:46 shagkur Exp $

cond.c -- Thread subsystem V

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

$Log: not supported by cvs2svn $

-------------------------------------------------------------*/


#include <stdlib.h>
#include <errno.h>
#include "asm.h"
#include "mutex.h"
#include "lwp_threadq.h"
#include "lwp_wkspace.h"
#include "cond.h"

#define LWP_MAXCONDVARS			1024

struct _cond {
	mutex_t mutex;
	lwp_thrqueue wait_queue;
};

struct _cond_obj {
	u32 cond_id;
	struct _cond cond;
};

static struct _cond_obj _cond_objects[LWP_MAXCONDVARS];

extern int clock_gettime(struct timespec *tp);
extern void timespec_substract(const struct timespec *tp_start,const struct timespec *tp_end,struct timespec *result);

static struct _cond* __lwp_cond_allochandle()
{
	s32 i;
	u32 level;
	struct _cond *ret = NULL;
	
	_CPU_ISR_Disable(level);

	i = 0;
	while(i<LWP_MAXCONDVARS && _cond_objects[i].cond_id!=-1) i++;
	if(i<LWP_MAXCONDVARS) {
		_cond_objects[i].cond_id = i;
		ret = &_cond_objects[i].cond;
	}

	_CPU_ISR_Restore(level);
	return ret;
}

static void __lwp_cond_freehandle(struct _cond *cond)
{
	s32 i;
	u32 level;

	_CPU_ISR_Disable(level);

	i = 0;
	while(i<LWP_MAXCONDVARS && cond!=&_cond_objects[i].cond) i++;
	if(i<LWP_MAXCONDVARS && _cond_objects[i].cond_id!=-1) {
		_cond_objects[i].cond_id = -1;
	}
	_CPU_ISR_Restore(level);
}

void __lwp_cond_init()
{
	s32 i;
	u32 level;

	_CPU_ISR_Disable(level);
	for(i=0;i<LWP_MAXCONDVARS;i++) _cond_objects[i].cond_id = -1;
	_CPU_ISR_Restore(level);
}

static s32 __lwp_cond_waitsupp(cond_t cond,mutex_t mutex,u32 timeout,u8 timedout)
{
	u32 status,mstatus;
	struct _cond *thecond = (struct _cond*)cond;

	if(!cond) return -1;

	__lwp_thread_dispatchdisable();
	if(thecond->mutex && thecond->mutex!=mutex) {
		__lwp_thread_dispatchenable();
		return EINVAL;
	}

	LWP_MutexUnlock(mutex);
	if(!timedout) {
		thecond->mutex = mutex;
		__lwp_threadqueue_csenter(&thecond->wait_queue);
		_thr_executing->wait.ret_code = 0;
		_thr_executing->wait.queue = &thecond->wait_queue;
		_thr_executing->wait.id = (u32)thecond;
		__lwp_threadqueue_enqueue(&thecond->wait_queue,timeout);
		__lwp_thread_dispatchenable();
		
		status = _thr_executing->wait.ret_code;
		if(status && status!=ETIMEDOUT)
			return status;
	} else {
		__lwp_thread_dispatchenable();
		status = ETIMEDOUT;
	}
	mstatus = LWP_MutexLock(mutex);
	if(mstatus)
		return EINVAL;

	return status;
}

static s32 __lwp_cond_signalsupp(cond_t cond,u8 isbroadcast)
{
	lwp_cntrl *thethread;
	struct _cond *thecond = (struct _cond*)cond;
	
	if(!thecond) return -1;
	
	__lwp_thread_dispatchdisable();
	do {
		thethread = __lwp_threadqueue_dequeue(&thecond->wait_queue);
		if(!thethread) thecond->mutex = NULL;
	} while(isbroadcast && thethread);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_CondInit(cond_t *cond)
{
	struct _cond *ret;
	
	if(!cond) return -1;
	
	__lwp_thread_dispatchdisable();
	ret = __lwp_cond_allochandle();
	if(!ret) {
		__lwp_thread_dispatchenable();
		return ENOMEM;
	}

	ret->mutex = NULL;
	__lwp_threadqueue_init(&ret->wait_queue,LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_CONDVAR,ETIMEDOUT);

	*cond = (cond_t)ret;
	__lwp_thread_dispatchenable();

	return 0;
}

s32 LWP_CondWait(cond_t cond,mutex_t mutex)
{
	return __lwp_cond_waitsupp(cond,mutex,LWP_THREADQ_NOTIMEOUT,FALSE);
}

s32 LWP_CondSignal(cond_t cond)
{
	return __lwp_cond_signalsupp(cond,FALSE);
}

s32 LWP_CondBroadcast(cond_t cond)
{
	return __lwp_cond_signalsupp(cond,TRUE);
}

s32 LWP_CondTimedWait(cond_t cond,mutex_t mutex,const struct timespec *abstime)
{
	u64 timeout;
	struct timespec curr_time;
	struct timespec diff;
	boolean timedout = FALSE;
	
	clock_gettime(&curr_time);
	timespec_substract(&curr_time,abstime,&diff);
	if(diff.tv_sec<0 || (diff.tv_sec==0&& diff.tv_nsec<0)) timedout = TRUE;

	timeout = __lwp_wd_calc_ticks(&diff);
	return __lwp_cond_waitsupp(cond,mutex,timeout,timedout);
}

s32 LWP_CondDestroy(cond_t cond)
{
	struct _cond *ptr = (struct _cond*)cond;
	__lwp_thread_dispatchdisable();
	if(__lwp_threadqueue_first(&ptr->wait_queue)) {
		__lwp_thread_dispatchenable();
		return EBUSY;
	}
	__lwp_cond_freehandle(ptr);
	__lwp_thread_dispatchenable();
	return 0;
}
