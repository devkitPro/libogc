/*-------------------------------------------------------------

exception.c -- PPC exception handling support

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "irq.h"
#include "context.h"

#include "system.h"

#include "gx.h"
#include "pad.h"
#include "consol.h"
#include "console.h"
#include "lwp_threads.h"
#include "ios.h"

#include "ogc/video_types.h"

//#define _EXC_DEBUG

#define CPU_STACK_TRACE_DEPTH		10

typedef struct _framerec {
	struct _framerec *up;
	void *lr;
} frame_rec, *frame_rec_t;

static void *exception_xfb = (void*)0xC1710000;			//we use a static address above ArenaHi.

void __exception_sethandler(u32 nExcept, void (*pHndl)(frame_context*));

extern void udelay(int us);
extern void fpu_exceptionhandler();
extern void irq_exceptionhandler();
extern void dec_exceptionhandler();
extern void default_exceptionhandler();
extern void VIDEO_SetFramebuffer(void *);

extern s8 exceptionhandler_start[],exceptionhandler_end[],exceptionhandler_patch[];
extern s8 systemcallhandler_start[],systemcallhandler_end[];

void (*_exceptionhandlertable[NUM_EXCEPTIONS])(frame_context*);

static u32 exception_location[NUM_EXCEPTIONS] = {
		0x00000100, 0x00000200, 0x00000300, 0x00000400,
		0x00000500, 0x00000600, 0x00000700, 0x00000800,
		0x00000900, 0x00000C00, 0x00000D00, 0x00000F00,
		0x00001300, 0x00001400, 0x00001700 };

static const char *exception_name[NUM_EXCEPTIONS] = {
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
	_sync();
}


void __systemcall_init()
{
	__exception_load(EX_SYS_CALL,systemcallhandler_start,(systemcallhandler_end-systemcallhandler_start),NULL);
}

void __exception_init()
{
	s32 i;
#ifdef _EXC_DEBUG
	kprintf("__exception_init()\n\n");
#endif
	// init all exceptions with the default handler & vector code
	for(i=0;i<NUM_EXCEPTIONS;i++) {
		// load default exception vector handler code
		__exception_load(i,exceptionhandler_start,(exceptionhandler_end-exceptionhandler_start),exceptionhandler_patch);
		//set default exception handler into table
		__exception_sethandler(i,default_exceptionhandler);
	}
	__exception_sethandler(EX_FP,fpu_exceptionhandler);
	__exception_sethandler(EX_INT,irq_exceptionhandler);
	__exception_sethandler(EX_DEC,dec_exceptionhandler);

	mtmsr(mfmsr()|MSR_RI);
}

void __exception_close(u32 except)
{
	void *pAdd = (void*)(0x80000000|exception_location[except]);
	*(u32*)pAdd = 0x4C000064;
	DCFlushRangeNoSync(pAdd,0x100);
	ICInvalidateRange(pAdd,0x100);
	_sync();
}

void __exception_closeall()
{
	s32 i;
	for(i=0;i<NUM_EXCEPTIONS;i++) {
		__exception_close(i);
	}
	mtmsr(mfmsr()&~MSR_EE);
	mtmsr(mfmsr()|(MSR_FP|MSR_RI));
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
		if(i%4) kprintf("--> ");
		else {
			if(i>0) kprintf("-->\n");
			else kprintf("\n");
		}

		switch(i) {
			case 0:
				if(pc) kprintf("%p",pc);
				break;
			case 1:
				if(!l) l = (frame_rec_t)mfspr(8);
				kprintf("%p",(void*)l);
				break;
			default:
				kprintf("%p",(void*)(p->up->lr));
				break;
		}
	}
}

static void ( * Reload ) () = ( void ( * ) () ) 0x80001800;

static void waitForReload() {

	u32 level;

	while ( 1 ) {

		PAD_ScanPads();

		int buttonsDown = PAD_ButtonsDown(0);

		if( (buttonsDown & PAD_TRIGGER_Z) || SYS_ResetButtonDown() ) {
			_CPU_ISR_Disable(level);
			Reload ();
		}

		if ( buttonsDown & PAD_BUTTON_A )
		{
			kprintf("Reset\n");
			SYS_ResetSystem(SYS_HOTRESET,0,FALSE);
		}

		udelay(20000);
	}
}


//just implement core for unrecoverable exceptions.
void c_default_exceptionhandler(frame_context *pCtx)
{
	VIDEO_SetFramebuffer(exception_xfb);
	__console_init(exception_xfb,20,20,640,574,1280);

	kprintf("\n\nException (%s) occurred!\n", exception_name[pCtx->EXCPT_Number]);

	kprintf("GPR00 %08X GPR08 %08X GPR16 %08X GPR24 %08X\n",pCtx->GPR[0], pCtx->GPR[8], pCtx->GPR[16], pCtx->GPR[24]);
	kprintf("GPR01 %08X GPR09 %08X GPR17 %08X GPR25 %08X\n",pCtx->GPR[1], pCtx->GPR[9], pCtx->GPR[17], pCtx->GPR[25]);
	kprintf("GPR02 %08X GPR10 %08X GPR18 %08X GPR26 %08X\n",pCtx->GPR[2], pCtx->GPR[10], pCtx->GPR[18], pCtx->GPR[26]);
	kprintf("GPR03 %08X GPR11 %08X GPR19 %08X GPR27 %08X\n",pCtx->GPR[3], pCtx->GPR[11], pCtx->GPR[19], pCtx->GPR[27]);
	kprintf("GPR04 %08X GPR12 %08X GPR20 %08X GPR28 %08X\n",pCtx->GPR[4], pCtx->GPR[12], pCtx->GPR[20], pCtx->GPR[28]);
	kprintf("GPR05 %08X GPR13 %08X GPR21 %08X GPR29 %08X\n",pCtx->GPR[5], pCtx->GPR[13], pCtx->GPR[21], pCtx->GPR[29]);
	kprintf("GPR06 %08X GPR14 %08X GPR22 %08X GPR30 %08X\n",pCtx->GPR[6], pCtx->GPR[14], pCtx->GPR[22], pCtx->GPR[30]);
	kprintf("GPR07 %08X GPR15 %08X GPR23 %08X GPR31 %08X\n",pCtx->GPR[7], pCtx->GPR[15], pCtx->GPR[23], pCtx->GPR[31]);
	kprintf("LR %08X SRR0 %08x SRR1 %08x MSR %08x\n", pCtx->LR, pCtx->SRR0, pCtx->SRR1,pCtx->MSR);
	kprintf("DAR %08X DSISR %08X\n", mfspr(19), mfspr(18));

	_cpu_print_stack((void*)pCtx->SRR0,(void*)pCtx->LR,(void*)pCtx->GPR[1]);

	if((pCtx->EXCPT_Number==EX_DSI) || (pCtx->EXCPT_Number==EX_FP)) {
		u32 i;
		u32 *pAdd = (u32*)pCtx->SRR0;
		kprintf("\n\nCODE DUMP:\n\n");
		for (i=0; i<12; i+=4)
			kprintf("%p:  %08X %08X %08X %08X\n",
			&(pAdd[i]),pAdd[i], pAdd[i+1], pAdd[i+2], pAdd[i+3]);
	}

	waitForReload();
}


void __libogc_exit(int status)
{
	SYS_ResetSystem(SYS_SHUTDOWN,0,0);
	__lwp_thread_stopmultitasking(Reload);
}

