#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "mutex.h"


int __libc_lock_init(int *lock,int recursive)
{
	s32 ret;
	mutex_t retlck = NULL;

	if(!lock) return -1;
	
	ret = LWP_MutexInit(&retlck,(recursive?TRUE:FALSE));
	*lock = (int)retlck;

	return ret;
}

int __libc_lock_close(int *lock)
{
	s32 ret;
	mutex_t plock;
	
	if(!lock) return -1;
	
	plock = (mutex_t)*lock;
	ret = LWP_MutexDestroy(plock);
	*lock = 0;
	return ret;
}

int __libc_lock_acquire(int *lock)
{
	mutex_t plock;
	
	if(!lock) return -1;
	if(!*lock) return -1;

	plock = (mutex_t)*lock;
	return LWP_MutexLock(plock);
}

int __libc_lock_try_acquire(int *lock)
{
	mutex_t plock;
	
	if(!lock) return -1;
	if(!*lock) return -1;
	
	plock = (mutex_t)*lock;
	return LWP_MutexTryLock(plock);
}

int __libc_lock_release(int *lock)
{
	mutex_t plock;
	
	if(!lock) return -1;
	if(!*lock) return -1;

	plock = (mutex_t)*lock;
	return LWP_MutexUnlock(plock);
}

