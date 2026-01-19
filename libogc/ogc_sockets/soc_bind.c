#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{

	sockfd = soc_get_fd(sockfd);

	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int ret = net_bind(sockfd, (struct sockaddr *)addr, addrlen);
	if (ret) {
		errno = -ret;
		return -1;
	}
	return 0;
}