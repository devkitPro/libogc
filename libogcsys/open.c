#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <string.h>
#include <stdio.h>
#ifdef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "iosupp.h"

#ifdef REENTRANT_SYSCALLS_PROVIDED
int _DEFUN (_open_r, (r, file, flags, mode),
			struct _reent *r _AND
			char *file  _AND
			int   flags _AND
			int   mode)
{
	int whichdev,fd;

	fd = -1;
	whichdev = 0;
	while(devoptab_list[whichdev]) {
		if(strcmp(devoptab_list[whichdev]->name,file)==0) {
			fd = whichdev;
			break;
		}
		whichdev++;
	}
	if(fd!=-1) devoptab_list[fd]->open_r(r,file,flags,mode);

	return fd;
}
#else
int _DEFUN (open, (file, flags, mode),
        char *file  _AND
        int   flags _AND
        int   mode)
{
	int i,whichdev,fd;
	char *lfile;

	i = 0;
	whichdev = -1;
	lfile = file;
	while(devoptab_list[i]) {
		if(strncmp(devoptab_list[i]->name,file,strlen(devoptab_list[i]->name))==0) {
			if(lfile[strlen(devoptab_list[i]->name)]==':') {
				lfile = (lfile+strlen(devoptab_list[i]->name)+1);
			}
			whichdev = i;
			break;
		}
		i++;
	}

	fd = -1;
	if(whichdev!=-1) {
		fd = devoptab_list[whichdev]->open_r(0,lfile,flags,mode);
		if(fd!=-1) fd += STD_MAX;
	}

	return fd;
}
#endif
