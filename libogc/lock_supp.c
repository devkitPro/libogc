#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "mutex.h"
#include <gcbool.h>


static void __libogc_lock_init(_LOCK_T *lock,int recursive)
{
	s32 ret;
	mutex_t retlck = LWP_MUTEX_NULL;

	if(!lock) return;

	*lock = 0;
	ret = LWP_MutexInit(&retlck,(recursive?TRUE:FALSE));
	if(ret==0) *lock = (_LOCK_T)retlck;

}

void __syscall_lock_init(_LOCK_T *lock)
{
	__libogc_lock_init(lock,0);
}

void __syscall_lock_init_recursive(_LOCK_T *lock)
{
	__libogc_lock_init(lock,1);
}

void __syscall_lock_close(_LOCK_T *lock)
{
	s32 ret;
	mutex_t plock;

	if(!lock || *lock==0) return;

	plock = (mutex_t)*lock;
	ret = LWP_MutexDestroy(plock);
	if(ret==0) *lock = 0;

}

void __syscall_lock_acquire(_LOCK_T *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return;

	plock = (mutex_t)*lock;
	LWP_MutexLock(plock);
}


void __syscall_lock_release(int *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return;

	plock = (mutex_t)*lock;
	LWP_MutexUnlock(plock);
}

