#include "soc_common.h"
#include <sys/socket.h>


ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int ret = net_sendto(sockfd, buf, len, flags, (struct sockaddr *)dest_addr, addrlen);
	if (ret < 0 ) {
		errno = -ret;
		ret = -1;
	}
	return ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int ret = net_send(sockfd, buf, len, flags);

	if (ret < 0 ) {
		errno = -ret;
		ret = -1;
	}
	return ret;
}