#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#undef errno
extern int errno;

int _DEFUN(isatty,(file),
           int file)
{
	return 0;
}
