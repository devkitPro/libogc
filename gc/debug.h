#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <gctypes.h>

#define GDBSTUB_PORT		2828

#ifdef __cplusplus
	extern "C" {
#endif

void _break();	
void DEBUG_Init(u32 port);

#ifdef __cplusplus
	}
#endif

#endif
