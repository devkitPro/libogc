#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#undef errno
extern int errno;

#include "timesupp.h"

unsigned int _DEFUN(sleep,(s),
		   unsigned int s)
{
	struct timespec tb;

	tb.tv_sec = s;
	tb.tv_nsec = 0;
	return nanosleep(&tb);
}
