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

int __libc_lock_init(int *lock,int recursive)
{
	return 0;
}

int __libc_lock_close(int *lock)
{
	return 0;
	
}

int __libc_lock_acquire(int *lock)
{
	return 0;
}

int __libc_lock_try_acquire(int *lock)
{
	return 0;
}

int __libc_lock_release(int *lock)
{
	return 0;
}

