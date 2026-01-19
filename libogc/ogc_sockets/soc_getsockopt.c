#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}


	int ret = net_getsockopt(sockfd, level, optname, optval, optlen);

	if (ret<0) {
		errno = -ret;
		return -1;
	}
	return 0;
}