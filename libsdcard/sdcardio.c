#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#include <fcntl.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "mutex.h"
#include "libogcsys/iosupp.h"
#include "sdcard.h"

#define MAX_SDCARD_FD		64

static mutex_t sdfds_lck;
static int sdcardio_inited = 0;

static struct _sdio_fds {
	int locked;
	sd_file *sdfile;
} sdfds[MAX_SDCARD_FD];

int sdcardio_open(struct _reent *r,const char *path,int flags,int mode);
int sdcardio_close(struct _reent *r,int fd);
int sdcardio_write(struct _reent *r,int fd,const char *ptr,int len);
int sdcardio_read(struct _reent *r,int fd,char *ptr,int len);
int sdcardio_seek(struct _reent *r,int fd,int pos,int dir);

const devoptab_t dotab_sdcardio = {"sd",sdcardio_open,sdcardio_close,sdcardio_write,sdcardio_read,sdcardio_seek};

static int __sdcardio_allocfd(sd_file *sdfile)
{
	int i;

	if(!sdcardio_inited) return -1;

	LWP_MutexLock(&sdfds_lck);
	for(i=0;i<MAX_SDCARD_FD;i++) {
		if(!sdfds[i].sdfile) {
			sdfds[i].locked = 0;
			sdfds[i].sdfile = sdfile;
			LWP_MutexUnlock(&sdfds_lck);
			return i;
		}
	}
	LWP_MutexUnlock(&sdfds_lck);
	return -1;
}

static sd_file* __sdcardio_getfd(int fd)
{
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return NULL;

	LWP_MutexLock(&sdfds_lck);
	if(fd>=0 && fd<MAX_SDCARD_FD) {
		sdfile = sdfds[fd].sdfile;
	}
	LWP_MutexUnlock(&sdfds_lck);

	return sdfile;
}

static void __sdcardio_freefd(int fd)
{
	if(!sdcardio_inited) return;

	LWP_MutexLock(&sdfds_lck);
	if(fd>=0 && fd<MAX_SDCARD_FD) {
		sdfds[fd].locked = 0;
		sdfds[fd].sdfile = NULL;
	}
	LWP_MutexUnlock(&sdfds_lck);
}

static void __sdcardio_init()
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(!sdcardio_inited) {
		sdcardio_inited = 1;
		memset(sdfds,0,MAX_SDCARD_FD*sizeof(struct _sdio_fds));
		LWP_MutexInit(&sdfds_lck,FALSE);
	}
	_CPU_ISR_Restore(level);
}

int sdcardio_open(struct _reent *r,const char *path,int flags,int mode)
{
	char m;
	sd_file *sdfile;
	int nFd = -1;
	
	__sdcardio_init();
	
	m = 0;
	if(flags&O_RDONLY) m = 'r';
	else if(flags&O_WRONLY) m = 'w';

	sdfile = SDCARD_OpenFile(path,&m);
	if(sdfile) nFd = __sdcardio_allocfd(sdfile);

	return nFd;
}

int sdcardio_close(struct _reent *r,int fd)
{
	int ret = -1;
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return -1;

	sdfile = __sdcardio_getfd(fd);
	if(sdfile) {
		ret = SDCARD_CloseFile(sdfile);
		__sdcardio_freefd(fd);
	}

	return ret;
}

int sdcardio_write(struct _reent *r,int fd,const char *ptr,int len)
{
	int ret = -1;
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return -1;

	sdfile = __sdcardio_getfd(fd);
	if(sdfile) {
	}
	return ret;
}

int sdcardio_read(struct _reent *r,int fd,char *ptr,int len)
{
	int ret = -1;
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return -1;

	sdfile = __sdcardio_getfd(fd);
	if(sdfile) ret = SDCARD_ReadFile(sdfile,ptr,len);

	return ret;
}

int sdcardio_seek(struct _reent *r,int fd,int pos,int dir)
{
	int ret = -1;
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return -1;

	sdfile = __sdcardio_getfd(fd);
	if(sdfile) ret = SDCARD_SeekFile(sdfile,pos,dir);

	return ret;
}
