#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>


int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
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

	int ret = net_getsockname(sockfd, addr, addrlen);

	if (ret < 0 ) {
		errno = -ret;
		ret = -1;
	}
	return ret;
}
