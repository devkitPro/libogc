#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#ifdef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "iosupp.h"

#ifdef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN(_close_r,(ptr,fildes),
		   struct reent *ptr _AND
           int fildes)
{
	return -1;
}
#else
int _DEFUN(close,(fildes),
           int fildes)
{
	int ret = -1;
	unsigned int dev;
	unsigned int fd;

	if(fildes!=-1) {
		dev = _SHIFTR(fildes,16,16);
		fd = fildes&0xffff;
		ret = devoptab_list[dev]->close_r(0,fd);
	}
	return ret;
}
#endif
