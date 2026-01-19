#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

int shutdown(int socket, int how) {
	socket = soc_get_fd(socket);

	if(socket < 0) {
		errno = -socket;
		return -1;
	}

	int ret = net_shutdown(socket, how);
	if (ret < 0 ) {
		errno = -ret;
		ret = -1;
	}
	return ret;
}