#include "soc_common.h"
#include <sys/socket.h>

#include "poll.h"

#include <stdlib.h>

#ifdef HW_RVL
__attribute__((weak)) size_t __ogc_pollfd_sb_max_fds = 64;

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    struct pollsd *fds2;
    int ret = 0;

    if(fds == NULL) {
        errno = EFAULT;
        return -1;
    }

    if(nfds <= __ogc_pollfd_sb_max_fds)
        fds2 = (struct pollsd *)alloca(nfds * sizeof(struct pollfd));
    else
        fds2 = (struct pollsd *)malloc(nfds * sizeof(struct pollfd));

    if(fds2 == NULL) {
        errno = ENOMEM;
        return -1;
    }

    for(nfds_t i = 0; i < nfds; i++) {
        fds2[i].events = fds[i].events;
        fds2[i].revents = 0;
        if(fds[i].fd < 0) {
            fds2[i].socket = -1;
        } else {
            fds2[i].socket = soc_get_fd(fds[i].fd);
            if(fds2[i].socket == -1) {
                ret = -1;
                break;
            }
        }
    }

    if(ret != -1)
        ret = net_poll(fds2, nfds, timeout);

    if (ret < 0 ) {
        errno = -ret;
        ret = -1;
    }

    if(ret != -1) {
        for(nfds_t i = 0; i < nfds; i++) {
            fds[i].revents = (short)fds2[i].revents;
        }
    }

    if(nfds > __ogc_pollfd_sb_max_fds)
        free(fds2);
    return ret;
}
#endif

#ifdef HW_DOL
int poll (struct pollfd *const fds_, nfds_t const nfds_, int const timeout_)
{
    fd_set readFds;
    fd_set writeFds;
    fd_set exceptFds;

    FD_ZERO (&readFds);
    FD_ZERO (&writeFds);
    FD_ZERO (&exceptFds);

    for (nfds_t i = 0; i < nfds_; ++i)
    {
        if (fds_[i].events & POLLIN)
            FD_SET (soc_get_fd(fds_[i].fd), &readFds);
        if (fds_[i].events & POLLOUT)
            FD_SET (soc_get_fd(fds_[i].fd), &writeFds);
    }

    struct timeval tv;
    tv.tv_sec     = timeout_ / 1000;
    tv.tv_usec    = (timeout_ % 1000) * 1000;
    const int rc = net_select (nfds_, &readFds, &writeFds, &exceptFds, &tv);
    if (rc < 0)
        return rc;

    int count = 0;
    for (nfds_t i = 0; i < nfds_; ++i)
    {
        bool counted    = false;
        fds_[i].revents = 0;

        if (FD_ISSET (soc_get_fd(fds_[i].fd), &readFds))
        {
            counted = true;
            fds_[i].revents |= POLLIN;
        }

        if (FD_ISSET (soc_get_fd(fds_[i].fd), &writeFds))
        {
            counted = true;
            fds_[i].revents |= POLLOUT;
        }

        if (FD_ISSET (soc_get_fd(fds_[i].fd), &exceptFds))
        {
            counted = true;
            fds_[i].revents |= POLLERR;
        }

        if (counted)
            ++count;
    }

    return count;
}
#endif