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
	int ret = -1;
	unsigned int dev = 0;
	unsigned int fd = -1;

	if(file!=-1) {
		dev = file;
		if(file&0xffff0000) {
			dev = _SHIFTR(file,16,16);
			fd = file&0xffff;
		}
		if(devoptab_list[dev]->read_r)
			ret = devoptab_list[dev]->read_r(ptr,fd,ptr,len);
	}
	return ret;
}
#else
int _DEFUN(read,(file, ptr, len),
           int   file  _AND
           char *ptr   _AND
           int   len)
{
	int ret = -1;
	unsigned int dev = 0;
	unsigned int fd = -1;

	if(file!=-1) {
		dev = file;
		if(file&0xffff0000) {
			dev = _SHIFTR(file,16,16);
			fd = file&0xffff;
		}
		if(devoptab_list[dev]->read_r)
			ret = devoptab_list[dev]->read_r(0,fd,ptr,len);
	}
	return ret;
}

#endif
