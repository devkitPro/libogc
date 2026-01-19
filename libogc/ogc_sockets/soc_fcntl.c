#include "soc_common.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

int fcntl(int sockfd, int cmd, ...)
{

	int arg = 0;

	sockfd = soc_get_fd(sockfd);
	if(sockfd < 0) {
		errno = -sockfd;
		return -1;
	}

	va_list args;
	if(cmd == F_SETFL) {
		va_start(args, cmd);
		arg = va_arg(args, int);
		va_end(args);
	}

	return net_fcntl(sockfd, cmd, arg);
}