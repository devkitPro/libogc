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
int _DEFUN (_write_r, (ptr, file, buf, len),
			struct _reent *r _AND
			int   file  _AND
			char *ptr   _AND
			int   len)
{
	int ret = -1;
	unsigned int dev = 0;
	unsigned int fd = -1;

	if(file!=-1) {
		dev = file;
		if(file&0xf000) {
			dev = _SHIFTR(file,12,4);
			fd = file&0x0fff;
		}
		if(devoptab_list[dev]->write_r)
			ret = devoptab_list[dev]->write_r(r,fd,ptr,len);
	}
	return ret;
}
#else
int _DEFUN (write, (file, ptr, len),
        int   file  _AND
        char *ptr   _AND
        int   len)
{
	int ret = -1;
	unsigned int dev = 0;
	unsigned int fd = -1;

	if(file!=-1) {
		dev = file;
		if(file&0xf000) {
			dev = _SHIFTR(file,12,4);
			fd = file&0x0fff;
		}	
		if(devoptab_list[dev]->write_r)
			ret = devoptab_list[dev]->write_r(0,fd,ptr,len);
	}
	return ret;
}
#endif
