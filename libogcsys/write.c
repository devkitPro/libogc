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
	return devoptab_list[file]->write_r(r,file,ptr,len);
}
#else
int _DEFUN (write, (file, ptr, len),
        int   file  _AND
        char *ptr   _AND
        int   len)
{
	int ret = -1;
	unsigned int whichdev = (unsigned)file;
	unsigned int whichfile = (unsigned)file;

	if(whichdev>STD_NET) {
		whichdev = STD_NET;
		whichfile -= STD_MAX;
	}
	ret = devoptab_list[whichdev]->write_r(0,whichfile,ptr,len);

	return ret;
}
#endif
