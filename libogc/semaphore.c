/*-------------------------------------------------------------

$Id: semaphore.c,v 1.8 2005-11-21 12:35:32 shagkur Exp $

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

-------------------------------------------------------------*/


#include <stdlib.h>
#include <errno.h>
#include "lwp_sema.h"
#include "lwp_wkspace.h"
#include "semaphore.h"

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

s32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
{
	lwp_semattr attr;
	lwp_sema *ret;

	if(!sem) return -1;
	*sem = NULL;
	
	__lwp_thread_dispatchdisable();
	ret = __lwp_sema_allocsema();
	if(!ret) {
		__lwp_thread_dispatchenable();
		return -1;
	}

	attr.max_cnt = max;
	attr.mode = LWP_SEMA_MODEFIFO;
	__lwp_sema_initialize(ret,&attr,start);

	*sem = (void*)ret;
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_SemWait(sem_t sem)
{
	return __lwp_semwaitsupp((lwp_sema*)sem,(u32)sem,LWP_THREADQ_NOTIMEOUT,TRUE);
}

s32 LWP_SemPost(sem_t sem)
{
	if(!sem) return -1;

	__lwp_thread_dispatchdisable();
	__lwp_sema_surrender((lwp_sema*)sem,(u32)sem);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_SemDestroy(sem_t sem)
{
	lwp_sema *p = (lwp_sema*)sem;
	if(!p) return -1;

	__lwp_thread_dispatchdisable();
	__lwp_sema_flush(p,-1);
	__lwp_sema_freesema(p);
	__lwp_thread_dispatchenable();

	return 0;
}
