#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#undef errno
extern int errno;

#include "iosupp.h"

int netio_open_f(struct _reent *r,const char *path,int flags,int mode);
int netio_close_f(struct _reent *r,int fd);
int netio_write_f(struct _reent *r,int fd,const char *ptr,int len);
int netio_read_f(struct _reent *r,int fd,char *ptr,int len);

const devoptab_t dotab_netfake = {"stdnet",netio_open_f,netio_close_f,netio_write_f,netio_read_f,NULL,NULL};

int netio_open_f(struct _reent *r,const char *path,int flags,int mode)
{
	printf("netio_open_f(%p,%s,%d,%d)\n",r,path,flags,mode);
	return -1;
}

int netio_close_f(struct _reent *r,int fd)
{
	return -1;
}

int netio_write_f(struct _reent *r,int fd,const char *ptr,int len)
{
	printf("netio_write_f(%p,%d,%p,%d)\n",r,fd,ptr,len);
	return -1;
}

int netio_read_f(struct _reent *r,int fd,char *ptr,int len)
{
	return -1;
}

