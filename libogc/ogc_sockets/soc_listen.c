#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

int listen(int sockfd, int max_connections)
{
	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	int ret = net_listen(sockfd, max_connections);
	if (ret) {
		errno = -ret;
		return -1;
	}
	return 0;
}