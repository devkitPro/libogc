#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "dsp.h"

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_ARDMA         0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static u32 __dsp_inited = FALSE;

static vu16* const _dspReg = (u16*)0xCC005000;

extern void __MaskIrq(u32);
extern void __UnmaskIrq(u32);

static void __dsp_handler(u32 nIrq,void *pCtx)
{
}

void DSP_Init()
{
	u32 level;

	if(__dsp_inited==FALSE) {
		_CPU_ISR_Disable(level);	
		IRQ_Request(IRQ_DSP_DSP,__dsp_handler,NULL);
		__UnmaskIrq(IRQMASK(IRQ_DSP_DSP));

		_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
		_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
		
		__dsp_inited = TRUE;
		_CPU_ISR_Restore(level);	
	}
}

u32 DSP_CheckMailTo()
{
	return _SHIFTR(_dspReg[0],15,1);
}

u32 DSP_CheckMailFrom()
{
	return _SHIFTR(_dspReg[2],15,1);
}

u32 DSP_ReadMailFrom()
{
	return (_SHIFTL(_dspReg[2],16,16)|(_dspReg[3]&0xffff));
}

void DSP_AssertInt()
{
	u32 level;

	_CPU_ISR_Disable(level);
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_PIINT;
	_CPU_ISR_Restore(level);
}
