#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <gctypes.h>

#define GDBSTUB_PORT		2828

#ifdef __cplusplus
	extern "C" {
#endif

extern const char *dbg_local_ip;
extern const char *dbg_netmask;
extern const char *dbg_gw;

void _break();	
void DEBUG_Init(u16 port);

#ifdef __cplusplus
	}
#endif

#endif
