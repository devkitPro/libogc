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

extern void SYS_SetArenaLo(void *newLo);
extern void* SYS_GetArenaLo();

#ifdef REENTRANT_SYSCALLS_PROVIDED
void* _DEFUN(_sbrk_r,(ptr,incr),
			 struct _reent *ptr _AND
			 ptrdiff_t incr)
{
	u32 level;
	char *heap_end = 0;
	char *prev_heap = 0;

	_CPU_ISR_Disable(level);
	heap_end = (char*)SYS_GetArenaLo();
	if((heap_end+incr)>(char*)SYS_GetArenaHi()) {
		_CPU_ISR_Restore(level);
		abort();
	}

	prev_heap = heap_end;
	SYS_SetArenaLo((void*)(heap_end+incr));
	_CPU_ISR_Restore(level);

	return (void*)prev_heap;	
}
#else
void* _DEFUN(sbrk,(incr),
			 int incr)
{
	u32 level;
	char *heap_end = 0;
	char *prev_heap = 0;

	_CPU_ISR_Disable(level);
	heap_end = (char*)SYS_GetArenaLo();
	if((heap_end+incr)>(char*)SYS_GetArenaHi()) {
		_CPU_ISR_Restore(level);
		abort();
	}

	prev_heap = heap_end;
	SYS_SetArenaLo((void*)(heap_end+incr));
	_CPU_ISR_Restore(level);

	return (void*)prev_heap;	
}
#endif
