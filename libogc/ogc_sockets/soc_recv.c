#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int ret = net_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);

	if (ret < 0 ) {
		errno = -ret;
		ret = -1;
	}
	return ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int ret = net_recv(sockfd, buf, len, flags);

	if (ret < 0 ) {
		errno = -ret;
		ret = -1;
	}

	return ret;
}
