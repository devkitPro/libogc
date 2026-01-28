#pragma once
#define __LINUX_ERRNO_EXTENSIONS__

#include <errno.h>
#include <string.h>
#include <sys/iosupport.h>
#include <gctypes.h>
#include <gccore.h>
#include <network.h>

#define SYNC_ERROR ENODEV

typedef s32 Handle;

static inline int
soc_get_fd(int fd)
{
	__handle *handle = __get_handle(fd);
	if(handle == NULL)
		return -ENODEV;
	if(strcmp(devoptab_list[handle->device]->name, "soc") != 0)
		return -ENOTSOCK;
	return *(Handle*)handle->fileStruct;
}

s32 _net_convert_error(s32 sock_retval);
