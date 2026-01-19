#include "soc_common.h"
#include <errno.h>
#include <sys/socket.h>

static int
soc_open(struct _reent *r,
         void          *fileStruct,
         const char    *path,
         int           flags,
         int           mode)
{
  r->_errno = ENOSYS;
  return -1;
}

static int
soc_close(struct _reent *r,
          void          *fd)
{

  int ret = net_close(*(Handle*)fd);

  if (ret < 0 ) {
    errno = -ret;
    ret = -1;
  }

  return ret;
}

static ssize_t
soc_write(struct _reent *r,
          void          *fd,
          const char    *ptr,
          size_t        len)
{

  int ret = net_sendto(*(Handle*)fd, ptr, len, 0, NULL, 0);

  if (ret < 0 ) {
    errno = -ret;
    ret = -1;
  }

  return ret;
}

static ssize_t
soc_read(struct _reent *r,
         void          *fd,
         char          *ptr,
         size_t        len)
{

  int ret = net_recvfrom(*(Handle*)fd, ptr, len, 0, NULL, NULL);

  if (ret < 0 ) {
    errno = -ret;
    ret = -1;
  }
  return ret;
}


static devoptab_t
soc_devoptab =
{
  .name         = "soc",
  .structSize   = sizeof(Handle),
  .open_r       = soc_open,
  .close_r      = soc_close,
  .write_r      = soc_write,
  .read_r       = soc_read,
  .seek_r       = NULL,
  .fstat_r      = NULL,
  .stat_r       = NULL,
  .link_r       = NULL,
  .unlink_r     = NULL,
  .chdir_r      = NULL,
  .rename_r     = NULL,
  .mkdir_r      = NULL,
  .dirStateSize = 0,
  .diropen_r    = NULL,
  .dirreset_r   = NULL,
  .dirnext_r    = NULL,
  .dirclose_r   = NULL,
  .statvfs_r    = NULL,
  .ftruncate_r  = NULL,
  .fsync_r      = NULL,
  .deviceData   = 0,
  .chmod_r      = NULL,
  .fchmod_r     = NULL,
};


int socInit(void)
{
  /* check that the "soc" device doesn't already exist */
  int dev = FindDevice("soc:");
  if(dev >= 0)
    return -1;

  /* add the "soc" device */
  dev = AddDevice(&soc_devoptab);
  if(dev < 0)
    return -1;

  return 0;
}

int socExit(void)
{
  int ret = 0;
  int dev;

  dev = FindDevice("soc:");

  if(dev >= 0)
    RemoveDevice("soc:");

  return ret;
}