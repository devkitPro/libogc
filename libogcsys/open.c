#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
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
	int handle;
	int i,dev,fd,namelen;
	char lfile[256];

	i = 0;
	dev = -1;
	while(i<STD_MAX) {
		if(devoptab_list[i]) {
			namelen = strlen(devoptab_list[i]->name);
			if(strncmp(devoptab_list[i]->name,file,namelen)==0) {
				switch(i) {
					case STD_NET:
						if(file[namelen]!=':') return -1;
						sprintf(lfile,"%s",&file[namelen+1]);
						break;
					case STD_SDCARD:
						if(!isdigit(file[namelen]) || file[namelen+1]!=':') return -1;
						sprintf(lfile,"dev%d:%s",(file[namelen]-'0'),&file[namelen+2]);
						break;
					default:
						break;

				}

				dev = i;
				break;
			}
		}
		i++;
	}

	fd = -1;
	handle = -1;
	if(dev!=-1 && devoptab_list[dev]->open_r) {
		fd = devoptab_list[dev]->open_r(r,lfile,flags,mode);
		if(fd!=-1) handle = _SHIFTL(dev,16,16)|(fd&0xffff);
	}

	return handle;
}
#else
int _DEFUN (open, (file, flags, mode),
        char *file  _AND
        int   flags _AND
        int   mode)
{
	int handle;
	int i,dev,fd,namelen;
	char lfile[256];

	printf("file = %s\n",file);
	i = 0;
	dev = -1;
	while(i<STD_MAX) {
		if(devoptab_list[i]) {
			namelen = strlen(devoptab_list[i]->name);
			if(strncmp(devoptab_list[i]->name,file,namelen)==0) {
				switch(i) {
					case STD_NET:
						if(file[namelen]!=':') return -1;
						sprintf(lfile,"%s",&file[namelen+1]);
						break;
					case STD_SDCARD:
						if(!isdigit(file[namelen]) && file[namelen+1]!=':') return -1;
						sprintf(lfile,"dev%d:%s",(file[namelen]-'0'),&file[namelen+2]);
						break;
					default:
						break;

				}

				dev = i;
				break;
			}
		}
		i++;
	}
	if(i>=STD_MAX) return -1;

	printf("lfile = %s, dev = %d\n",lfile,i);

	fd = -1;
	handle = -1;
	if(dev!=-1 && devoptab_list[dev]->open_r) {
		fd = devoptab_list[dev]->open_r(0,lfile,flags,mode);
		if(fd!=-1) handle = _SHIFTL(dev,16,16)|(fd&0xffff);
	}

	return handle;
}
#endif
