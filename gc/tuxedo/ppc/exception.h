// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

#define PPC_EXCPT_RESET   1
#define PPC_EXCPT_MCHK    2
#define PPC_EXCPT_DSI     3
#define PPC_EXCPT_ISI     4
#define PPC_EXCPT_IRQ     5
#define PPC_EXCPT_ALIGN   6
#define PPC_EXCPT_UNDEF   7
#define PPC_EXCPT_FPU     8
#define PPC_EXCPT_DECR    9
#define PPC_EXCPT_SYSCALL 12
#define PPC_EXCPT_TRACE   13
#define PPC_EXCPT_PM      15
#define PPC_EXCPT_BKPT    19

#ifndef __ASSEMBLER__
#include "../types.h"
#include "context.h"

MK_EXTERN_C_START

typedef void (*PPCExcptHandlerFn)(void);
typedef void (*PPCExcptPanicFn)(unsigned exid, PPCContext* ctx);

extern PPCExcptHandlerFn __ppc_excpt_table[];
extern PPCExcptPanicFn PPCExcptCurPanicFn;

void PPCExcptDefaultHandler(void);
void PPCExcptDefaultScHandler(void);

MK_INLINE void PPCExcptSetHandler(unsigned exid, PPCExcptHandlerFn handler)
{
	__ppc_excpt_table[exid] = handler;
}

MK_INLINE PPCExcptHandlerFn PPCExcptGetHandler(unsigned exid)
{
	return __ppc_excpt_table[exid];
}

MK_EXTERN_C_END

#endif
