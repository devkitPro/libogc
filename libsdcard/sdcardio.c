#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "mutex.h"
#include "libogcsys/iosupp.h"
#include "sdcard.h"

//#define _SDCARDIO_DEBUG

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
int sdcardio_stat(struct _reent *r,int fd,struct stat *st);

const devoptab_t dotab_sdcardio = {"sd",sdcardio_open,sdcardio_close,sdcardio_write,sdcardio_read,sdcardio_seek,sdcardio_stat};

static int __sdcardio_allocfd(sd_file *sdfile)
{
	int i;

	if(!sdcardio_inited) return -1;

	LWP_MutexLock(sdfds_lck);
	for(i=0;i<MAX_SDCARD_FD;i++) {
		if(!sdfds[i].sdfile) {
			sdfds[i].locked = 0;
			sdfds[i].sdfile = sdfile;
			LWP_MutexUnlock(sdfds_lck);
			return i;
		}
	}
	LWP_MutexUnlock(sdfds_lck);
	return -1;
}

static sd_file* __sdcardio_getfd(int fd)
{
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return NULL;

	LWP_MutexLock(sdfds_lck);
	if(fd>=0 && fd<MAX_SDCARD_FD) {
		sdfile = sdfds[fd].sdfile;
	}
	LWP_MutexUnlock(sdfds_lck);

	return sdfile;
}

static void __sdcardio_freefd(int fd)
{
	if(!sdcardio_inited) return;

	LWP_MutexLock(sdfds_lck);
	if(fd>=0 && fd<MAX_SDCARD_FD) {
		sdfds[fd].locked = 0;
		sdfds[fd].sdfile = NULL;
	}
	LWP_MutexUnlock(sdfds_lck);
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

	m = 'r';
	if(flags==O_WRONLY) m = 'w';
#ifdef _SDCARDIO_DEBUG
	printf("path = %s, flags = %d, mode = %d, m = %c\n",path,flags,mode,m);
#endif
	sdfile = SDCARD_OpenFile(path,&m);
	if(sdfile) nFd = __sdcardio_allocfd(sdfile);

	return nFd;
}

int sdcardio_close(struct _reent *r,int fd)
{
	int ret = -1;
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return -1;
#ifdef _SDCARDIO_DEBUG
	printf("sdcardio_close(%d)\n",fd);
#endif
	sdfile = __sdcardio_getfd(fd);
	if(sdfile) {
		ret = SDCARD_CloseFile(sdfile);
#ifdef _SDCARDIO_DEBUG
	printf("sdcardio_close(%d)\n",ret);
#endif
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
	if(sdfile) ret = SDCARD_WriteFile(sdfile,ptr,len);
#ifdef _SDCARDIO_DEBUG
	printf("sdcardio_write(%d,%d,%p,%d)\n",fd,ret,ptr,len);
#endif
	return ret;
}

int sdcardio_read(struct _reent *r,int fd,char *ptr,int len)
{
	int ret = -1;
	sd_file *sdfile = NULL;

	if(!sdcardio_inited) return -1;

	sdfile = __sdcardio_getfd(fd);
	if(sdfile) ret = SDCARD_ReadFile(sdfile,ptr,len);
#ifdef _SDCARDIO_DEBUG
	printf("sdcardio_read(%d,%d,%p,%d)\n",fd,ret,ptr,len);
#endif
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
 
int sdcardio_stat(struct _reent *r,int fd,struct stat *st)
{
	int attr,ret = -1;
	sd_file *sdfile = NULL;
	SDSTAT stats;

	
	if(!sdcardio_inited) return -1;

	sdfile = __sdcardio_getfd(fd);
	if(sdfile) {
#ifdef _SDCARDIO_DEBUG
		printf("sdcardio_stat(%d,%p)\n",fd,sdfile);
#endif
		if(SDCARD_GetStats(sdfile,&stats)==SDCARD_ERROR_READY) {
			st->st_size = stats.size;
			st->st_ino = stats.ino;
			st->st_dev = stats.dev;
			st->st_mtime = mktime(&stats.ftime);
			
			attr = 0;
			if(stats.attr&SDCARD_ATTR_ARCHIVE) attr |= S_IFREG;
			else if(stats.attr&SDCARD_ATTR_DIR) attr |= S_IFDIR;
			if(stats.attr&SDCARD_ATTR_RONLY) attr |= (S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
			else attr |= (S_IRWXU|S_IRWXG|S_IRWXO);
			st->st_mode = attr;

			st->st_blksize = stats.blk_sz;
			st->st_blocks = stats.blk_cnt;
#ifdef _SDCARDIO_DEBUG
			printf("sdcardio_stat(%d,%08x,%d,%08x,%08x,%d,%d)\n",st->st_dev,st->st_ino,(int)st->st_size,st->st_mode,attr,(int)st->st_blksize,(int)st->st_blocks);
#endif
			ret = 0;
		}
	}

	return ret;
}
