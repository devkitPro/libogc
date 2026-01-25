// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"
#include "mm.h"
#include "ppc/bit.h"

#define IRQMASK(_irq)     PPCBIT(_irq)

#define IM_MEM0           IRQMASK(IRQ_MEM0)
#define IM_MEM1           IRQMASK(IRQ_MEM1)
#define IM_MEM2           IRQMASK(IRQ_MEM2)
#define IM_MEM3           IRQMASK(IRQ_MEM3)
#define IM_MEMADDRESS     IRQMASK(IRQ_MEMADDRESS)
#define IM_MEM            (IM_MEM0|IM_MEM1|IM_MEM2|IM_MEM3|IM_MEMADDRESS)

#define IM_DSP_AI         IRQMASK(IRQ_DSP_AI)
#define IM_DSP_ARAM       IRQMASK(IRQ_DSP_ARAM)
#define IM_DSP_DSP        IRQMASK(IRQ_DSP_DSP)
#define IM_DSP            (IM_DSP_AI|IM_DSP_ARAM|IM_DSP_DSP)

#define IM_AI             IRQMASK(IRQ_AI)

#define IM_EXI0_EXI       IRQMASK(IRQ_EXI0_EXI)
#define IM_EXI0_TC        IRQMASK(IRQ_EXI0_TC)
#define IM_EXI0_EXT       IRQMASK(IRQ_EXI0_EXT)
#define IM_EXI0           (IM_EXI0_EXI|IM_EXI0_TC|IM_EXI0_EXT)
#define IM_EXI1_EXI       IRQMASK(IRQ_EXI1_EXI)
#define IM_EXI1_TC        IRQMASK(IRQ_EXI1_TC)
#define IM_EXI1_EXT       IRQMASK(IRQ_EXI1_EXT)
#define IM_EXI1           (IM_EXI1_EXI|IM_EXI1_TC|IM_EXI1_EXT)
#define IM_EXI2_EXI       IRQMASK(IRQ_EXI2_EXI)
#define IM_EXI2_TC        IRQMASK(IRQ_EXI2_TC)
#define IM_EXI2           (IM_EXI2_EXI|IM_EXI2_TC)
#define IM_EXI            (IM_EXI0|IM_EXI1|IM_EXI2)

#define IM_PI_CP          IRQMASK(IRQ_PI_CP)
#define IM_PI_PETOKEN     IRQMASK(IRQ_PI_PETOKEN)
#define IM_PI_PEFINISH    IRQMASK(IRQ_PI_PEFINISH)
#define IM_PI_SI          IRQMASK(IRQ_PI_SI)
#define IM_PI_DI          IRQMASK(IRQ_PI_DI)
#define IM_PI_RSW         IRQMASK(IRQ_PI_RSW)
#define IM_PI_ERROR       IRQMASK(IRQ_PI_ERROR)
#define IM_PI_VI          IRQMASK(IRQ_PI_VI)
#define IM_PI_DEBUG       IRQMASK(IRQ_PI_DEBUG)
#define IM_PI_HSP         IRQMASK(IRQ_PI_HSP)
#if defined(__gamecube__)
#define IM_PI             (IM_PI_CP|IM_PI_PETOKEN|IM_PI_PEFINISH|IM_PI_SI|IM_PI_DI|IM_PI_RSW|IM_PI_ERROR|IM_PI_VI|IM_PI_DEBUG|IM_PI_HSP)
#elif defined(__wii__)
#define IM_PI_ACR         IRQMASK(IRQ_PI_ACR)
#define IM_PI             (IM_PI_CP|IM_PI_PETOKEN|IM_PI_PEFINISH|IM_PI_SI|IM_PI_DI|IM_PI_RSW|IM_PI_ERROR|IM_PI_VI|IM_PI_DEBUG|IM_PI_HSP|IM_PI_ACR)
#endif

MK_EXTERN_C_START

typedef enum KIrqId {
	IRQ_PI_ERROR,
	IRQ_PI_DEBUG,
	IRQ_MEM0,
	IRQ_MEM1,
	IRQ_MEM2,
	IRQ_MEM3,
	IRQ_MEMADDRESS,
	IRQ_PI_RSW,
	IRQ_PI_VI,
	IRQ_PI_PETOKEN,
	IRQ_PI_PEFINISH,
	IRQ_PI_HSP,
	IRQ_DSP_ARAM,
	IRQ_DSP_DSP,
	IRQ_AI,
	IRQ_EXI0_EXI,
	IRQ_EXI0_TC,
	IRQ_EXI0_EXT,
	IRQ_EXI1_EXI,
	IRQ_EXI1_TC,
	IRQ_EXI1_EXT,
	IRQ_EXI2_EXI,
	IRQ_EXI2_TC,
	IRQ_PI_SI,
	IRQ_PI_DI,
	IRQ_DSP_AI,
	IRQ_PI_CP,
#ifdef __wii__
	IRQ_PI_ACR,
#endif
	IRQ_MAX,
} KIrqId;

typedef void (*KIrqHandlerFn)(KIrqId id, void* user);

void KIrqEnable(u32 mask);

void KIrqDisable(u32 mask);

KIrqHandlerFn KIrqGet(KIrqId id);

KIrqHandlerFn KIrqSet(KIrqId id, KIrqHandlerFn fn, void* user);

MK_INLINE KIrqHandlerFn KIrqClear(KIrqId id)
{
	return KIrqSet(id, NULL, NULL);
}

MK_EXTERN_C_END
