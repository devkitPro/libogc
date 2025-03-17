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


void __syscall_lock_init(_LOCK_T *lock)
{
	s32 ret;
	mutex_t retlck = LWP_MUTEX_NULL;
	if(!lock) return;

	*lock = 0;
	ret = LWP_MutexInit(&retlck,false);
	if(ret==0) *lock = (_LOCK_T)retlck;
}

void __syscall_lock_init_recursive(_LOCK_RECURSIVE_T *lock)
{
	s32 ret;
	mutex_t retlck = LWP_MUTEX_NULL;
	if(!lock) return;

	*lock = 0;
	ret = LWP_MutexInit(&retlck,true);
	if(ret==0) *lock = (_LOCK_T)retlck;
}

void __syscall_lock_close(_LOCK_T *lock)
{
	s32 ret;

	if(!lock || *lock==0) return;

	ret = LWP_MutexDestroy((mutex_t)*lock);
	if(ret==0) *lock = 0;

}

void __syscall_lock_acquire(_LOCK_T *lock)
{
	if(!lock || *lock==0) return;

	LWP_MutexLock((mutex_t)*lock);
}


void __syscall_lock_release(_LOCK_T *lock)
{
	if(!lock || *lock==0) return;

	LWP_MutexUnlock((mutex_t)*lock);
}

