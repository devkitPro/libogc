#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	sockfd = soc_get_fd(sockfd);

	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int dev = FindDevice("soc:");
	if(dev < 0) {
		errno = ENODEV;
		return -1;
	}

	int fd = __alloc_handle(dev);
	if(fd < 0) return fd;


	int ret = net_accept(sockfd, addr, addrlen);
	if (ret < 0 ) {
		errno = -ret;
		return -1;
	}

	__handle *handle = __get_handle(fd);
	*(Handle*)handle->fileStruct = ret;

	return fd;
}
