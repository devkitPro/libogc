#include <config.h>
#include <_ansi.h>
#include <_syslist.h>

#include "timesupp.h"

unsigned int _DEFUN(usleep,(us),
           unsigned int us)
{
	struct timespec tb;

	tb.tv_sec = 0;
	tb.tv_nsec = us*TB_NSPERUS;
	return nanosleep(&tb);
}
