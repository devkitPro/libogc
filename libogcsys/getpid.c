#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#ifdef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#ifdef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN(_getpid_r,(ptr),
           struct _reent *ptr)
{
	return -1;
}
#else
int _DEFUN(getpid,(),
           _NOARGS)
{
	return -1;
}
#endif
