#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#undef errno
extern int errno;

#include "stdin_fake.h"

int stdin_open(struct _reent *r,const char *path,int flags,int mode);
int stdin_close(struct _reent *r,int fd);
int stdin_write(struct _reent *r,int fd,const char *ptr,int len);
int stdin_read(struct _reent *r,int fd,char *ptr,int len);

const devoptab_t dotab_stdin = {"stdin",stdin_open,stdin_close,stdin_write,stdin_read,NULL,NULL};

int stdin_open(struct _reent *r,const char *path,int flags,int mode)
{
	return -1;
}

int stdin_close(struct _reent *r,int fd)
{
	return -1;
}

int stdin_write(struct _reent *r,int fd,const char *ptr,int len)
{
	return -1;
}

int stdin_read(struct _reent *r,int fd,char *ptr,int len)
{
	return -1;
}

