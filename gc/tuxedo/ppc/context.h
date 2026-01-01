#pragma once

#define PPC_CONTEXT_PC    0x000
#define PPC_CONTEXT_MSR   0x004
#define PPC_CONTEXT_CR    0x008
#define PPC_CONTEXT_LR    0x00c
#define PPC_CONTEXT_CTR   0x010
#define PPC_CONTEXT_XER   0x014
#define PPC_CONTEXT_GPR   0x018
#define PPC_CONTEXT_FPSCR 0x098
#define PPC_CONTEXT_FPR   0x0a0
#define PPC_CONTEXT_GQR   0x1a0
#define PPC_CONTEXT_PS    0x1c0
#define PPC_CONTEXT_SIZE  0x2c0

#ifndef __ASSEMBLER__
#include "../types.h"

MK_EXTERN_C_START

typedef struct PPCContext {
	u32 pc;
	u32 msr;
	u32 cr;
	u32 lr;
	u32 ctr;
	u32 xer;
	u32 gpr[32];

	u64 fpscr;
	u64 fpr[32];
	u32 gqr[8];
	u64 ps[32];
} PPCContext;

MK_EXTERN_C_END

#endif
