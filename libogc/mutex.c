/*-------------------------------------------------------------

$Id: mutex.c,v 1.10 2005-12-09 09:35:45 shagkur Exp $

mutex.c -- Thread subsystem III

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
Revision 1.9  2005/11/21 12:35:32  shagkur
no message


-------------------------------------------------------------*/


#include <stdlib.h>
#include <errno.h>
#include "asm.h"
#include "lwp_mutex.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "mutex.h"

typedef struct _mutex_st
{
	lwp_obj object;
	lwp_mutex mutex;
} mutex_st;

static lwp_objinfo _lwp_mutex_objects;

static s32 __lwp_mutex_locksupp(mutex_t mutex,u32 timeout,u8 block)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__lwp_mutex_seize(&((mutex_st*)mutex)->mutex,(u32)mutex,block,timeout,level);
	return _thr_executing->wait.ret_code;
}

void __lwp_mutex_init()
{
	__lwp_objmgr_initinfo(&_lwp_mutex_objects,LWP_MAX_MUTEXES,sizeof(mutex_st));
}


s32 LWP_MutexInit(mutex_t *mutex,boolean use_recursive)
{
	u32 level;
	lwp_mutex_attr attr;
	mutex_st *ret;
	
	if(!mutex) return -1;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	ret = (mutex_st*)__lwp_objmgr_allocate(&_lwp_mutex_objects);
	if(!ret) {
		__lwp_thread_dispatchenable();
		_CPU_ISR_Restore(level);
		return -1;
	}

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = use_recursive?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = 1; //__lwp_priotocore(LWP_PRIO_MAX-1);
	__lwp_mutex_initialize(&ret->mutex,&attr,LWP_MUTEX_UNLOCKED);

	*mutex = (mutex_t)ret;
	__lwp_objmgr_open(&_lwp_mutex_objects,&ret->object);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
	return 0;
}

s32 LWP_MutexDestroy(mutex_t mutex)
{
	u32 level;
	mutex_st *p = (mutex_st*)mutex;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	if(__lwp_mutex_locked(&p->mutex)) {
		__lwp_thread_dispatchenable();
		_CPU_ISR_Restore(level);
		return EBUSY;
	}
	
	__lwp_mutex_flush(&p->mutex,EINVAL);
	__lwp_objmgr_close(&_lwp_mutex_objects,&p->object);
	__lwp_objmgr_free(&_lwp_mutex_objects,&p->object);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
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
	ret = __lwp_mutex_surrender(&((mutex_st*)mutex)->mutex);
	__lwp_thread_dispatchenable();
	return ret;
}
