#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#ifndef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN(lseek,(file, ptr, dir),
           int   file  _AND
           int   ptr   _AND
           int   dir)
{
	return 0;
}

#else
int _DEFUN(_lseek_r,(ptr, file, pos, dir),
		   struct _reent *ptr _AND
		   int   file  _AND
           int   pos   _AND
           int   dir)
{
	return 0;
}

#endif
