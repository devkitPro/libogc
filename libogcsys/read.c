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
int _DEFUN(_read_r,(ptr, file, buf, len),
		   struct _reent *ptr _AND
           int			 file _AND
           char			 *buf _AND
           int			 len)
{
	return -1;
}
#else
int _DEFUN(read,(file, ptr, len),
           int   file  _AND
           char *ptr   _AND
           int   len)
{
	int ret = -1;
	unsigned int dev;
	unsigned int fd;

	if(file!=-1) {
		dev = _SHIFTR(file,16,16);
		fd = file&0xffff;
		ret = devoptab_list[dev]->read_r(0,fd,ptr,len);
	}
	return ret;
}

#endif
