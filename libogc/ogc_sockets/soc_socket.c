#include "soc_common.h"
#include <errno.h>

int socket(int domain, int type, int protocol)
{
	s32 ret = 0;
	int fd, dev;
	__handle *handle;

	dev = FindDevice("soc:");
	if(dev < 0) {
		errno = ENODEV;
		return -1;
	}

	fd = __alloc_handle(dev);
	if(fd < 0) return fd;
	handle = __get_handle(fd);
	handle->device = dev;
	handle->fileStruct = ((void *)handle) + sizeof(__handle);

#ifdef __wii__
	// wii network stack only supports IPPROTO_IP
	protocol = 0;
#endif
	ret = net_socket( domain, type, protocol);

	if (ret < 0) {
		__release_handle(fd);
		errno = -ret;
		return -1;
	}

	*(Handle*)handle->fileStruct = ret;

	return fd;
}