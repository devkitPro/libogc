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
int _DEFUN(_kill_r,(ptr,pid,sig),
		   struct _reent *ptr _AND
           int			 pid  _AND
           int			 sig)
{
	return -1;
}
#else
int _DEFUN(kill,(pid,sig),
           int pid  _AND
           int sig)
{
	return -1;
}
#endif
