#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#undef errno
extern int errno;

#include "sdcardio_fake.h"

int sdcardio_open_f(struct _reent *r,const char *path,int flags,int mode);
int sdcardio_close_f(struct _reent *r,int fd);
int sdcardio_write_f(struct _reent *r,int fd,const char *ptr,int len);
int sdcardio_read_f(struct _reent *r,int fd,char *ptr,int len);
int sdcardio_seek_f(struct _reent *r,int fd,int pos,int dir);
int sdcardio_stat_f(struct _reent *r,int fd,struct stat *st);

const devoptab_t dotab_sdcardfake = {"sd",sdcardio_open_f,sdcardio_close_f,sdcardio_write_f,sdcardio_read_f,sdcardio_seek_f,sdcardio_stat_f};

int sdcardio_open_f(struct _reent *r,const char *path,int flags,int mode)
{
	return -1;
}

int sdcardio_close_f(struct _reent *r,int fd)
{
	return -1;
}

int sdcardio_write_f(struct _reent *r,int fd,const char *ptr,int len)
{
	return -1;
}

int sdcardio_read_f(struct _reent *r,int fd,char *ptr,int len)
{
	return -1;
}

int sdcardio_seek_f(struct _reent *r,int fd,int pos,int dir)
{
	return -1;
}

int sdcardio_stat_f(struct _reent *r,int fd,struct stat *st)
{
	return -1;
}
