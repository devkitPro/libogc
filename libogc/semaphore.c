/*-------------------------------------------------------------

$Id: semaphore.c,v 1.9 2005-12-09 09:35:45 shagkur Exp $

semaphore.c -- Thread subsystem IV

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
Revision 1.8  2005/11/21 12:35:32  shagkur
no message


-------------------------------------------------------------*/


#include <stdlib.h>
#include <errno.h>
#include <asm.h>
#include "lwp_sema.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "semaphore.h"

typedef struct _sema_st
{
	lwp_obj object;
	lwp_sema sema;
} sema_st;

static lwp_objinfo _lwp_sema_objects;

static s32 __lwp_semwaitsupp(lwp_sema *sem,u32 id,u32 timeout,u8 blocking)
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

void __lwp_sema_init()
{
	__lwp_objmgr_initinfo(&_lwp_sema_objects,LWP_MAX_SEMAS,sizeof(sema_st));
}

s32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
{
	u32 level;
	lwp_semattr attr;
	sema_st *ret;

	if(!sem) return -1;
	*sem = NULL;
	
	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	ret = (sema_st*)__lwp_objmgr_allocate(&_lwp_sema_objects);
	if(!ret) {
		__lwp_thread_dispatchenable();
		_CPU_ISR_Restore(level);
		return -1;
	}

	attr.max_cnt = max;
	attr.mode = LWP_SEMA_MODEFIFO;
	__lwp_sema_initialize(&ret->sema,&attr,start);

	*sem = (sem_t)ret;
	__lwp_objmgr_open(&_lwp_sema_objects,&ret->object);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);
	return 0;
}

s32 LWP_SemWait(sem_t sem)
{
	return __lwp_semwaitsupp(&((sema_st*)sem)->sema,(u32)sem,LWP_THREADQ_NOTIMEOUT,TRUE);
}

s32 LWP_SemPost(sem_t sem)
{
	if(!sem) return -1;

	__lwp_thread_dispatchdisable();
	__lwp_sema_surrender(&((sema_st*)sem)->sema,(u32)sem);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_SemDestroy(sem_t sem)
{
	u32 level;
	sema_st *p = (sema_st*)sem;
	if(!p) return -1;

	_CPU_ISR_Disable(level);
	__lwp_thread_dispatchdisable();
	__lwp_sema_flush(&p->sema,-1);
	__lwp_objmgr_close(&_lwp_sema_objects,&p->object);
	__lwp_objmgr_free(&_lwp_sema_objects,&p->object);
	__lwp_thread_dispatchenable();
	_CPU_ISR_Restore(level);

	return 0;
}
