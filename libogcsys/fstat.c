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

#include "iosupp.h"

#ifdef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN (_fstat_r,(ptr, fildes, st),
			struct _reent *ptr _AND
            int           fildes _AND
            struct stat   *st)
{
	int ret = -1;
	unsigned int dev = 0;
	unsigned int fd = -1;

	if(fildes!=-1) {
		dev = fildes;
		if(fildes&0xf000) {
			dev = _SHIFTR(fildes,12,4);
			fd = fildes&0x0fff;
		}
		if(devoptab_list[dev]->stat_r)
			ret = devoptab_list[dev]->stat_r(ptr,fd,st);
	}
	return ret;
}
#else
int _DEFUN (fstat,(fildes, st),
            int          fildes _AND
            struct stat *st)
{
	int ret = -1;
	unsigned int dev = 0;
	unsigned int fd = -1;

	if(fildes!=-1) {
		dev = fildes;
		if(fildes&0xf000) {
			dev = _SHIFTR(fildes,12,4);
			fd = fildes&0x0fff;
		}

		if(devoptab_list[dev]->stat_r)
			ret = devoptab_list[dev]->stat_r(0,fd,st);
	}
	return ret;
}
#endif
