#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "irq.h"
#include "context.h"

#include "pad.h"

//#define _EXC_DEBUG

#define CPU_STACK_TRACE_DEPTH		10

typedef struct _framerec {
	struct _framerec *up;
	void *lr;
} frame_rec, *frame_rec_t;

extern s8 default_exceptionhandler_start[],default_exceptionhandler_end[],default_exceptionhandler_patch[];
extern s8 systemcall_handler_start[],systemcall_handler_end[];

void (*_exceptionhandlertable[NUM_EXCEPTIONS])(frame_context*);

void c_default_exceptionhandler(frame_context *);
void __exception_sethandler(u32 nExcept, void (*pHndl)(frame_context*));

extern void __lwp_fpucontext_handler(frame_context*);

static u32 exception_location[NUM_EXCEPTIONS] = {
		0x00000100, 0x00000200, 0x00000300, 0x00000400,
		0x00000500, 0x00000600, 0x00000700, 0x00000800,
		0x00000900, 0x00000C00, 0x00000D00, 0x00000F00,
		0x00001300, 0x00001400, 0x00001700 };

static const s8 *exception_name[NUM_EXCEPTIONS] = {
		"System Reset", "Machine Check", "DSI", "ISI",
		"Interrupt", "Alignment", "Program", "Floating Point",
		"Decrementer", "System Call", "Trace", "Performance",
		"IABR", "Reserved", "Thermal"};

void __exception_load(u32 nExc,void *data,u32 len,void *patch)
{
	void *pAddr = (void*)(0x80000000|exception_location[nExc]);
	memcpy(pAddr,data,len);
	if(patch)
		*(u32*)((u32)pAddr+(patch-data)) |= nExc;

	DCFlushRangeNoSync(pAddr,len);
	ICInvalidateRange(pAddr,len);
}


void __systemcall_init()
{
	__exception_load(EX_SYS_CALL,systemcall_handler_start,(systemcall_handler_end-systemcall_handler_start),NULL);
}

void __exception_init()
{
	s32 i;
#ifdef _EXC_DEBUG
	printf("__exception_init()\n\n");
#endif
	// init all exceptions with the default handler & vector code
	for(i=0;i<NUM_EXCEPTIONS;i++) {
		// load default exception vector handler code
		__exception_load(i,default_exceptionhandler_start,(default_exceptionhandler_end-default_exceptionhandler_start),default_exceptionhandler_patch);
		//set default exception handler into table
		__exception_sethandler(i,c_default_exceptionhandler);
	}
	__exception_sethandler(EX_FP,__lwp_fpucontext_handler);
	mtmsr(mfmsr()|MSR_RI);
}

void __excpetion_close()
{
	s32 i;
	for(i=0;i<NUM_EXCEPTIONS;i++) {
		void *pAdd = (void*)(0x80000000|exception_location[i]);
		*(u32*)pAdd = 0x4C000064;
		DCFlushRangeNoSync(pAdd,0x100);
		ICInvalidateRange(pAdd,0x100);
	}
}

void __exception_sethandler(u32 nExcept, void (*pHndl)(frame_context*))
{
	_exceptionhandlertable[nExcept] = pHndl;
}

static void _cpu_print_stack(void *pc,void *lr,void *r1)
{
	register u32 i = 0;
	register frame_rec_t l,p = (frame_rec_t)lr;

	l = p;
	p = r1;
	if(!p) __asm__ __volatile__("mr %0,%%r1" : "=r"(p));

	for(i=0;i<CPU_STACK_TRACE_DEPTH-1 && p->up;p=p->up,i++) {
		if(i%5) printf("--> ");
		else printf("\n\n");

		switch(i) {
			case 0:
				if(pc) printf("%p",pc);
				break;
			case 1:
				if(!l) l = (frame_rec_t)mfspr(8);
				printf("%p",(void*)l);
				break;
			default:
				printf("%p",(void*)(p->up->lr));
				break;
		}
	}
}
//just implement core for unrecoverable exceptions.
void c_default_exceptionhandler(frame_context *pCtx)
{
	printf("Exception (%s) occured!\n", exception_name[pCtx->EXCPT_Number]);

	printf("GPR00 %08x GPR08 %08x GPR16 %08x GPR24 %08x\n",pCtx->GPR[0], pCtx->GPR[8], pCtx->GPR[16], pCtx->GPR[24]);
	printf("GPR01 %08x GPR09 %08x GPR17 %08x GPR25 %08x\n",pCtx->GPR[1], pCtx->GPR[9], pCtx->GPR[17], pCtx->GPR[25]);
	printf("GPR02 %08x GPR10 %08x GPR18 %08x GPR26 %08x\n",pCtx->GPR[2], pCtx->GPR[10], pCtx->GPR[18], pCtx->GPR[26]);
	printf("GPR03 %08x GPR11 %08x GPR19 %08x GPR27 %08x\n",pCtx->GPR[3], pCtx->GPR[11], pCtx->GPR[19], pCtx->GPR[27]);
	printf("GPR04 %08x GPR12 %08x GPR20 %08x GPR28 %08x\n",pCtx->GPR[4], pCtx->GPR[12], pCtx->GPR[20], pCtx->GPR[28]);
	printf("GPR05 %08x GPR13 %08x GPR21 %08x GPR29 %08x\n",pCtx->GPR[5], pCtx->GPR[13], pCtx->GPR[21], pCtx->GPR[29]);
	printf("GPR06 %08x GPR14 %08x GPR22 %08x GPR30 %08x\n",pCtx->GPR[6], pCtx->GPR[14], pCtx->GPR[22], pCtx->GPR[30]);
	printf("GPR07 %08x GPR15 %08x GPR23 %08x GPR31 %08x\n",pCtx->GPR[7], pCtx->GPR[15], pCtx->GPR[23], pCtx->GPR[31]);
	printf("LR %08x SRR0 %08x SRR1 %08x MSR %08x\n", pCtx->LR, pCtx->SRR0, pCtx->SRR1,pCtx->MSR);
	printf("DAR %08x DSISR %08x\n", mfspr(19), mfspr(18));

	_cpu_print_stack((void*)pCtx->SRR0,(void*)pCtx->LR,(void*)pCtx->GPR[1]);

	if((pCtx->EXCPT_Number==EX_DSI) || (pCtx->EXCPT_Number==EX_FP)) {
		u32 i;
		u32 *pAdd = (u32*)pCtx->SRR0;
		printf("\n\nCODE DUMP:\n\n");
		for (i=0; i<12; i+=4)
			printf("%p:  %08x %08x %08x %08x\n",
			&(pAdd[i]),pAdd[i], pAdd[i+1], pAdd[i+2], pAdd[i+3]);
	}

	PADStatus pads[PAD_CHANMAX];

	while(1) {
		PAD_Read(pads);
		if(pads[PAD_CHAN0].button&PAD_BUTTON_START) {
			void (*reload)() = (void(*)())0x80001800;
			reload();
		}
	}
}
