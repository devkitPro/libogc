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
#include "lwp_wkspace.h"
#include "lwp_mutex.h"

typedef struct _lock {
	int id;
	lwp_mutex lck;
} lock_t;

static int _g_libc_lck_id = 0;

void __libc_lock_init(int *lock,boolean recursive)
{
	lwp_mutex_attr attr;
	lock_t *retlck = NULL;

	if(!lock) return;

	__lwp_thread_dispatchdisable();
	retlck = (lock_t*)__lwp_wkspace_allocate(sizeof(lock_t));
	if(!retlck) {
		__lwp_thread_dispatchenable();
		return;
	}

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = recursive?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = 1; //__lwp_priotocore(LWP_PRIO_MAX-1);
	__lwp_mutex_initialize(&retlck->lck,&attr,LWP_MUTEX_UNLOCKED);

	retlck->id = ++_g_libc_lck_id;
	__lwp_thread_dispatchenable();
}

void __libc_lock_close(int *lock)
{
}
