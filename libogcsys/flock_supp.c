#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

void __flockfile(FILE *fp)
{
	printf("__flockfile()\n");
}

void __funlockfile(FILE *fp)
{
	printf("__funlockfile()\n");
}
