#include <stdlib.h>
#include <string.h>
#include "asm.h"
#include "cache.h"
#include "exception.h"
#include "processor.h"
#include "irq.h"

//#define _IRQ_DEBUG

#define CPU_STACK_ALIGNMENT				8
#define CPU_MINIMUM_STACK_FRAME_SIZE	16

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

struct irq_handler_s {
	raw_irq_handler_t *pHndl;
	void *pCtx;
};

static u32 currIrqMask = 0;
static struct irq_handler_s g_IRQHandler[32];

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;
static vu16* const _dspReg = (u16*)0xCC005000;
static vu32* const _exiReg = (u32*)0xCC006800;
static vu32* const _aiReg = (u32*)0xCC006C00;

static u32 const _irqPrio[] = {0x00000100,0x00000040,0xf8000000,0x00000200,
							   0x00000080,0x00003000,0x00000020,0x03ff8c00,
							   0x04000000,0x00004000,0xffffffff};

extern void __exception_load(u32,void *,u32,void *);

extern s8 irqhandler_start[],irqhandler_end[];
extern u8 __intrstack_addr[],__intrstack_end[];

#ifdef _IRQ_DEBUG
#include <lwp_threads.h>
#endif

static void __irqhandler_init()
{
	__exception_load(EX_INT,irqhandler_start,(irqhandler_end-irqhandler_start),NULL);
	mtmsr(mfmsr()|MSR_EE);
}

void c_irqdispatcher()
{
	u32 i,icause,intmask,irq;
	u32 cause = _piReg[0]&_piReg[1];
	if(!cause) return;

	intmask = 0;
	if(cause&0x00000080) {		//Memory Interface
		icause = _memReg[14]&_memReg[15];
		if(icause&0x00000001) {
			intmask |= IRQMASK(IRQ_MEM0);
		}
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_MEM1);
		}
		if(icause&0x00000004) {
			intmask |= IRQMASK(IRQ_MEM2);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_MEM3);
		}
		if(icause&0x00000010) {
			intmask |= IRQMASK(IRQ_MEMADDRESS);
		}
	}
	if(cause&0x00000040) {		//DSP
		icause = _dspReg[5]&(_SHIFTR((_dspReg[5]&0x150),1,16));
		if(icause&0x00000008){
			intmask |= IRQMASK(IRQ_DSP_AI);
		}
		if(icause&0x00000020){
			intmask |= IRQMASK(IRQ_DSP_ARAM);
		}
		if(icause&0x00000080){
			intmask |= IRQMASK(IRQ_DSP_DSP);
		}
	}
	if(cause&0x00000020) {		//Streaming
		icause = _aiReg[0]&(_SHIFTL((_aiReg[0]&0x04),1,16));
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_AI_AI);
		}
	}
	if(cause&0x00000010) {		//EXI
		//EXI 0
		icause = _exiReg[0]&(_SHIFTL((_exiReg[0]&0x405),1,16));
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_EXI0_EXI);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_EXI0_TC);
		}
		if(icause&0x00000800) {
			intmask |= IRQMASK(IRQ_EXI0_EXT);
		}
		//EXI 1
		icause = _exiReg[5]&(_SHIFTL((_exiReg[5]&0x405),1,16));
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_EXI1_EXI);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_EXI1_TC);
		}
		if(icause&0x00000800) {
			intmask |= IRQMASK(IRQ_EXI1_EXT);
		}
		//EXI 2
		icause = _exiReg[10]&(_SHIFTL((_exiReg[10]&0x405),1,16));
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_EXI2_EXI);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_EXI2_TC);
		}
	}
	if(cause&0x00000800) {		//Command FIFO
		intmask |= IRQMASK(IRQ_PI_CP);
	}
	if(cause&0x00000200) {		//Token Assertion (PE_TOKEN)
		intmask |= IRQMASK(IRQ_PI_PETOKEN);
	}
	if(cause&0x00000400) {		//Frame Ready (PE_FINISH)
		intmask |= IRQMASK(IRQ_PI_PEFINISH);
	}
	if(cause&0x00000008) {		//Serial
		intmask |= IRQMASK(IRQ_PI_SI);
	}
	if(cause&0x00000004) {		//DVD
		intmask |= IRQMASK(IRQ_PI_DI);
	}
	if(cause&0x00000002) {		//Reset Switch
		intmask |= IRQMASK(IRQ_PI_RSW);
	}
	if(cause&0x00000001) {		//GP Runtime Error
		intmask |= IRQMASK(IRQ_PI_ERROR);
	}
	if(cause&0x00000100) {		//Video Interface
		intmask |= IRQMASK(IRQ_PI_VI);
	}
	if(cause&0x00001000) {		//External Debugger
		intmask |= IRQMASK(IRQ_PI_DEBUG);
	}
	if(cause&0x00002000) {		//High Speed Port
		intmask |= IRQMASK(IRQ_PI_HSP);
	}
	
	i=0;
	irq = 0;
	while(i<(sizeof(_irqPrio)/sizeof(u32))) {
		if(intmask&_irqPrio[i]) {
			irq = cntlzw(intmask);
			break;
		}
		i++;
	}


#ifdef _IRQ_DEBUG
	printf("c_irqdispatcher(%08x,%d,%d,%d)\n",intmask,irq,__lwp_isr_in_progress(),_thread_dispatch_disable_level);
#endif

	if(g_IRQHandler[irq].pHndl)
		g_IRQHandler[irq].pHndl(irq,g_IRQHandler[irq].pCtx);

	_piReg[0] = cause;
}

void __UnmaskIrq(u32 nMask)
{
	u32 level;
	u32 imask;
	
	_CPU_ISR_Disable(level);

	currIrqMask = (currIrqMask&~nMask)|nMask;
	if(nMask&IM_PI_ERROR) {
		_piReg[1] = (_piReg[1]&~0x00000001)|0x00000001;
	}
	if(nMask&IM_PI_RSW) {
		_piReg[1] = (_piReg[1]&~0x00000002)|0x00000002;
	}
	if(nMask&IM_PI_DI) {
		_piReg[1] = (_piReg[1]&~0x00000004)|0x00000004;
	}
	if(nMask&IM_PI_SI) {
		_piReg[1] = (_piReg[1]&~0x00000008)|0x00000008;
	}
	if(nMask&IM_EXI) {
		_piReg[1] = (_piReg[1]&~0x00000010)|0x00000010;

		if(nMask&IM_EXI0) {
			imask = _exiReg[0]&~0x2c0f;
			if(currIrqMask&IM_EXI0_EXI) imask |= 0x0001;
			if(currIrqMask&IM_EXI0_TC) imask |= 0x0004;
			if(currIrqMask&IM_EXI0_EXT) imask |= 0x0400;
			_exiReg[0] = imask;
		}
		if(nMask&IM_EXI1) {
			imask = _exiReg[5]&~0x0c0f;
			if(currIrqMask&IM_EXI1_EXI) imask |= 0x0001;
			if(currIrqMask&IM_EXI1_TC) imask |= 0x0004;
			if(currIrqMask&IM_EXI1_EXT) imask |= 0x0400;
			_exiReg[5] = imask;
		}
		if(nMask&IM_EXI2) {
			imask = _exiReg[10]&~0x000f;
			if(currIrqMask&IM_EXI2_EXI) imask |= 0x0001;
			if(currIrqMask&IM_EXI2_TC) imask |= 0x0004;
			_exiReg[10] = imask;
		}

	}
	if(nMask&IM_AI) {
		_piReg[1] = (_piReg[1]&~0x00000020)|0x00000020;

		imask = _aiReg[0]&~0x2c;
		if(currIrqMask&IM_AI_AI) imask |= 0x0004;
		_aiReg[0] = imask;
	}
	if(nMask&IM_DSP) {
		_piReg[1] = (_piReg[1]&~0x00000040)|0x00000040;

		imask = _dspReg[5]&~0x1f8;
		if(currIrqMask&IM_DSP_AI) imask |= 0x0010;
		if(currIrqMask&IM_DSP_ARAM) imask |= 0x0040;
		if(currIrqMask&IM_DSP_DSP) imask |= 0x0100;
		_dspReg[5] = (u16)imask;

	}
	if(nMask&IM_MEM) {
		_piReg[1] = (_piReg[1]&~0x00000080)|0x00000080;

		imask = 0;
		if(currIrqMask&IM_MEM0) imask |= 0x0001;
		if(currIrqMask&IM_MEM1) imask |= 0x0002;
		if(currIrqMask&IM_MEM2) imask |= 0x0004;
		if(currIrqMask&IM_MEM3) imask |= 0x0008;
		if(currIrqMask&IM_MEMADDRESS) imask |= 0x0010;
		_memReg[14] = (u16)imask;
	}
	if(nMask&IM_PI_VI) {
		_piReg[1] = (_piReg[1]&~0x00000100)|0x00000100;
	}
	if(nMask&IM_PI_PETOKEN) {
		_piReg[1] = (_piReg[1]&~0x00000200)|0x00000200;
	}
	if(nMask&IM_PI_PEFINISH) {
		_piReg[1] = (_piReg[1]&~0x00000400)|0x00000400;
	}
	if(nMask&IM_PI_CP) {
		_piReg[1] = (_piReg[1]&~0x00000800)|0x00000800;
	}
	if(nMask&IM_PI_DEBUG) {
		_piReg[1] = (_piReg[1]&~0x00001000)|0x00001000;
	}
	if(nMask&IM_PI_HSP) {
		_piReg[1] = (_piReg[1]&~0x00002000)|0x00002000;
	}

	_CPU_ISR_Restore(level);
}

void __MaskIrq(u32 nMask)
{
	u32 level;
	u32 imask;

	_CPU_ISR_Disable(level);

	currIrqMask = (currIrqMask&~nMask);
	if(nMask&IM_PI_ERROR) {
		_piReg[1] = (_piReg[1]&~0x00000001);
	}
	if(nMask&IM_PI_RSW) {
		_piReg[1] = (_piReg[1]&~0x00000002);
	}
	if(nMask&IM_PI_DI) {
		_piReg[1] = (_piReg[1]&~0x00000004);
	}
	if(nMask&IM_PI_SI) {
		_piReg[1] = (_piReg[1]&~0x00000008);
	}
	if(nMask&IM_EXI) {
		if(nMask&IM_EXI0) {
			imask = _exiReg[0]&~0x2c0f;
			if(currIrqMask&IM_EXI0_EXI) imask |= 0x0001;
			if(currIrqMask&IM_EXI0_TC) imask |= 0x0004;
			if(currIrqMask&IM_EXI0_EXT) imask |= 0x0400;
			_exiReg[0] = imask;
		}
		if(nMask&IM_EXI1) {
			imask = _exiReg[5]&~0x0c0f;
			if(currIrqMask&IM_EXI1_EXI) imask |= 0x0001;
			if(currIrqMask&IM_EXI1_TC) imask |= 0x0004;
			if(currIrqMask&IM_EXI1_EXT) imask |= 0x0400;
			_exiReg[5] = imask;
		}
		if(nMask&IM_EXI2) {
			imask = _exiReg[10]&~0x000f;
			if(currIrqMask&IM_EXI2_EXI) imask |= 0x0001;
			if(currIrqMask&IM_EXI2_TC) imask |= 0x0004;
			_exiReg[10] = imask;
		}
		if(!(currIrqMask&IM_EXI)) _piReg[1] = (_piReg[1]&~0x00000010);
	}
	if(nMask&IM_AI) {
		imask = _aiReg[0]&~0x2c;
		if(currIrqMask&IM_AI_AI) imask |= 0x0004;
		_aiReg[0] = imask;
		if(!(currIrqMask&IM_AI)) _piReg[1] = (_piReg[1]&~0x00000020);
	}
	if(nMask&IM_DSP) {
		imask = _dspReg[5]&~0x1f8;
		if(currIrqMask&IM_DSP_AI) imask |= 0x0010;
		if(currIrqMask&IM_DSP_ARAM) imask |= 0x0040;
		if(currIrqMask&IM_DSP_DSP) imask |= 0x0100;
		_dspReg[5] = (u16)imask;
		if(!(currIrqMask&IM_DSP)) _piReg[1] = (_piReg[1]&~0x00000040);

	}
	if(nMask&IM_MEM) {
		imask = 0;
		if(currIrqMask&IM_MEM0) imask |= 0x0001;
		if(currIrqMask&IM_MEM1) imask |= 0x0002;
		if(currIrqMask&IM_MEM2) imask |= 0x0004;
		if(currIrqMask&IM_MEM3) imask |= 0x0008;
		if(currIrqMask&IM_MEMADDRESS) imask |= 0x0010;
		_memReg[14] = (u16)imask;
		if(!(currIrqMask&IM_MEM)) _piReg[1] = (_piReg[1]&~0x00000080);
	}
	if(nMask&IM_PI_VI) {
		_piReg[1] = (_piReg[1]&~0x00000100);
	}
	if(nMask&IM_PI_PETOKEN) {
		_piReg[1] = (_piReg[1]&~0x00000200);
	}
	if(nMask&IM_PI_PEFINISH) {
		_piReg[1] = (_piReg[1]&~0x00000400);
	}
	if(nMask&IM_PI_CP) {
		_piReg[1] = (_piReg[1]&~0x00000800);
	}
	if(nMask&IM_PI_DEBUG) {
		_piReg[1] = (_piReg[1]&~0x00001000);
	}
	if(nMask&IM_PI_HSP) {
		_piReg[1] = (_piReg[1]&~0x00002000);
	}

	_CPU_ISR_Restore(level);
}

void __irq_init()
{
	register u32 intrStack = (u32)__intrstack_addr;
	register u32 intrStack_end = (u32)__intrstack_end;
	register u32 irqNestingLevel = 0;

	memset(g_IRQHandler,0,32*sizeof(struct irq_handler_s));

	*((u32*)intrStack_end) = 0xDEADBEEF;
	intrStack = intrStack - CPU_MINIMUM_STACK_FRAME_SIZE;
	intrStack &= ~(CPU_STACK_ALIGNMENT-1);
	*((u32*)intrStack) = 0;

	mtspr(272,irqNestingLevel);
	mtspr(273,intrStack);
	
	__irqhandler_init();
	_piReg[1] = 0xf0;
}

raw_irq_handler_t* IRQ_Request(u32 nIrq,raw_irq_handler_t *pHndl,void *pCtx)
{
	u32 level;

	raw_irq_handler_t *old = g_IRQHandler[nIrq].pHndl;
	_CPU_ISR_Disable(level);
	g_IRQHandler[nIrq].pHndl = pHndl;
	g_IRQHandler[nIrq].pCtx = pCtx;
	_CPU_ISR_Restore(level);
	return old;
}

raw_irq_handler_t* IRQ_GetHandler(u32 nIrq)
{
	u32 level;
	raw_irq_handler_t *ret;

	_CPU_ISR_Disable(level);
	ret = g_IRQHandler[nIrq].pHndl;
	_CPU_ISR_Restore(level);
	return ret;
}

raw_irq_handler_t* IRQ_Free(u32 nIrq)
{
	u32 level;

	raw_irq_handler_t *old = g_IRQHandler[nIrq].pHndl;
	_CPU_ISR_Disable(level);
	g_IRQHandler[nIrq].pHndl = NULL;
	g_IRQHandler[nIrq].pCtx = NULL;
	_CPU_ISR_Restore(level);
	return old;
}
