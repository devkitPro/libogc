#ifndef __IOSUPP_H__
#define __IOSUPP_H__

#include <reent.h>

#define STD_IN			0
#define STD_OUT			1
#define STD_NET			2
#define STD_MAX			3

typedef struct {
	const char *name;
	int (*open_r)(struct _reent *r,const char *path,int flags,int mode);
	int (*close_r)(struct _reent *r,int fd);
	int (*write_r)(struct _reent *r,int fd,const char *ptr,int len);
	int (*read_r)(struct _reent *r,int fd,char *ptr,int len);
} devoptab_t;

extern const devoptab_t *devoptab_list[];

#endif
