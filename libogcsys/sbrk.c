#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "system.h"

#ifdef REENTRANT_SYSCALLS_PROVIDED
void* _DEFUN(_sbrk_r,(ptr,incr),
			 struct _reent *ptr _AND
			 ptrdiff_t incr)
{
	char *heap_end = 0;
	char *prev_heap = 0;

	heap_end = (char*)SYS_GetArenaLo();
	if((heap_end+incr)>(char*)SYS_GetArenaHi()) abort();

	prev_heap = heap_end;
	SYS_SetArenaLo((void*)(heap_end+incr));

	return (void*)prev_heap;	
}
#else
void* _DEFUN(sbrk,(incr),
			 int incr)
{
	char *heap_end = 0;
	char *prev_heap = 0;

	heap_end = (char*)SYS_GetArenaLo();
	if((heap_end+incr)>(char*)SYS_GetArenaHi()) abort();

	prev_heap = heap_end;
	SYS_SetArenaLo((void*)(heap_end+incr));

	return (void*)prev_heap;	
}
#endif
