#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <gctypes.h>

#define GDBSTUB_PORT		2828

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct _ptregs {
	u32 gpr[32];
	f64 fpr[32];
	u32 pc, ps, cnd, lr;
	u32 cnt, xer, mq;
} pt_regs;
		
void DEBUG_Init(u32 port);

#ifdef __cplusplus
	}
#endif

#endif
