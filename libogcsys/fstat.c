#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#ifdef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN (_fstat_r,(ptr, fildes, st),
			struct _reent *ptr _AND
            int           fildes _AND
            struct stat   *st)
{
	return -1;
}
#else
int _DEFUN (fstat,(fildes, st),
            int          fildes _AND
            struct stat *st)
{
	return -1;
}
#endif
