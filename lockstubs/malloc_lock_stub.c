#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "lwp_mutex.h"

/*static int initialized = 0;
static lwp_mutex mem_lock;
*/
void __memlock_init()
{
}

#ifndef REENTRANT_SYSCALLS_PROVIDED
void _DEFUN(__malloc_lock,(r),
			struct _reent *r)
{
}

void _DEFUN(__malloc_unlock,(r),
			struct _reent *r)
{
}

#else
void _DEFUN(__malloc_lock,(ptr),
			struct _reent *ptr)
{
}

void _DEFUN(__malloc_unlock,(ptr),
			struct _reent *ptr)
{
}

#endif
