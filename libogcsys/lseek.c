#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "iosupp.h"

#ifdef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN(_lseek_r,(ptr, file, pos, dir),
		   struct _reent *ptr _AND
		   int   file  _AND
           int   pos   _AND
           int   dir)
{
	int ret = -1;
	unsigned int dev;
	unsigned int fd;

	if(file!=-1) {
		dev = _SHIFTR(file,16,16);
		fd = file&0xffff;

		if(devoptab_list[dev]->seek_r)
			ret = devoptab_list[dev]->seek_r(ptr,fd,pos,dir);
	}
	return ret;
}
#else
int _DEFUN(lseek,(file, pos, dir),
           int   file  _AND
		   int   pos   _AND
           int   dir)
{
	int ret = -1;
	unsigned int dev;
	unsigned int fd;

	if(file!=-1) {
		dev = _SHIFTR(file,16,16);
		fd = file&0xffff;

		if(devoptab_list[dev]->seek_r)
			ret = devoptab_list[dev]->seek_r(0,fd,pos,dir);
	}
	return ret;
}

#endif
