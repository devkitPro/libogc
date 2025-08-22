/*-------------------------------------------------------------

default_panic.c -- Default panic screen for libogc

Copyright (C) 2025
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
fincs

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

#include <gctypes.h>
#include <tuxedo/thread.h>
#include <tuxedo/ppc/exception.h>
#include <ogc/system.h>
#include <ogc/pad.h>
#include <ogc/video.h>
#include <ogc/gx.h>
#include <ogc/console.h>
#include <ogc/color.h>
#include <ogc/machine/processor.h>
#include "console_internal.h"

#define CPU_STACK_TRACE_DEPTH		10

typedef struct PPCStackFrame {
	uptr next;
	void* lr;
} PPCStackFrame;

static void *exception_xfb = (void*)0xC1700000;			//we use a static address above ArenaHi.
static int reload_timer = -1;

extern void udelay(int us);
extern void VIDEO_SetFramebuffer(void *);
extern void __VIClearFramebuffer(void*, u32, u32);
extern void __reload(void);

static const char* const exception_name[] =
{
	[PPC_EXCPT_RESET]   = "System Reset",
	[PPC_EXCPT_MCHK]    = "Machine Check",
	[PPC_EXCPT_DSI]     = "DSI",
	[PPC_EXCPT_ISI]     = "ISI",
	[PPC_EXCPT_IRQ]     = "Interrupt",
	[PPC_EXCPT_ALIGN]   = "Alignment",
	[PPC_EXCPT_UNDEF]   = "Program",
	[PPC_EXCPT_FPU]     = "Floating Point",
	[PPC_EXCPT_DECR]    = "Decrementer",
	[PPC_EXCPT_SYSCALL] = "System Call",
	[PPC_EXCPT_TRACE]   = "Trace",
	[PPC_EXCPT_PM]      = "Performance",
	[PPC_EXCPT_BKPT]    = "IABR",
};

MK_INLINE PPCStackFrame* check_stack_address(uptr sp)
{
	if (!sp || sp == UINT32_MAX) {
		return NULL;
	}

	return (PPCStackFrame*)sp;
}

static void __libogc_print_stack(uptr pc, uptr sp)
{
	kprintf("\nSTACK DUMP:\n%p", (void*)pc);

	PPCStackFrame* p = check_stack_address(sp);
	for (unsigned i = 1; p && i < CPU_STACK_TRACE_DEPTH; p = check_stack_address(p->next), i ++) {
		if ((i%4) == 0) {
			kprintf(" -->\n");
		} else {
			kprintf(" --> ");
		}

		kprintf("%p", p->lr);
	}

	if (p) {
		kprintf(" --> ...");
	}

	kprintf("\n");
}

void __exception_setreload(int t)
{
	reload_timer = t*50;
}

static void waitForReload(void)
{
	PAD_Init();

	kprintf("\nPress RESET, or Z on a GameCube Controller, to reload\n");

	for (;;) {
		udelay(20000);
		PAD_ScanPads();

		unsigned buttonsDown = PAD_ButtonsDown(0);

		if (!reload_timer || SYS_ResetButtonDown() || (buttonsDown & PAD_TRIGGER_Z)) {
			kprintf("\x1b[2K\rReload\n");
			__reload();
		}

		if (buttonsDown & PAD_BUTTON_A) {
			kprintf("\x1b[2K\rReset\n");
			*(vu32*)0x80001804 = 0; // Destroy bootstub signature
			__reload();
		}

		if (reload_timer > 0) {
			kprintf("\x1b[2K\rReloading in %d seconds", 1+reload_timer/50);
		}

		if (reload_timer > 0) {
			reload_timer--;
		}
	}
}

void __libogc_panic(unsigned exid, PPCContext* ctx)
{
	const bool is_pal = VIDEO_GetCurrentTvMode() == VI_PAL;
	const unsigned console_width  = 640;
	const unsigned console_height = is_pal ? 574 : 480;
	const unsigned border_width   = 48;
	const unsigned border_height  = is_pal ? 96 : 48;

	GX_AbortFrame();
	VIDEO_SetFramebuffer(exception_xfb);
	__VIClearFramebuffer(exception_xfb, console_height*console_width*VI_DISPLAY_PIX_SZ, COLOR_MAROON);
	__console_init(exception_xfb, border_width, border_height, console_width - 2*border_width, console_height - 2*border_height, 2*console_width);
	CON_EnableGecko(1, true);

	kprintf("Exception (%s) occurred!\n", exception_name[exid]);

	for (unsigned i = 0; i < 8; i ++) {
		kprintf("GPR%02d %08X GPR%02d %08X GPR%02d %08X GPR%02d %08X\n",
			0+i, ctx->gpr[0+i], 8+i, ctx->gpr[8+i], 16+i, ctx->gpr[16+i], 24+i, ctx->gpr[24+i]);
	}

	kprintf("   PC %08X    LR %08X   MSR %08X  Thrd %08X\n", ctx->pc, ctx->lr, ctx->msr, (u32)KThreadGetSelf());
	kprintf("   CR %08X   CTR %08X   XER %08X\n", ctx->cr, ctx->ctr, ctx->xer);
	if (exid == PPC_EXCPT_DSI) {
		kprintf("  DAR %08X DSISR %08X\n", mfspr(DAR), mfspr(DSISR));
	}

	__libogc_print_stack(ctx->pc, ctx->gpr[1]);

	if(exid == PPC_EXCPT_DSI) {
		u32* pAdd = (u32*)ctx->pc;
		kprintf("\nCODE DUMP:\n");
		for (unsigned i = 0; i < 12; i += 4) {
			kprintf("%p:  %08X %08X %08X %08X\n",
				&pAdd[i], pAdd[i], pAdd[i+1], pAdd[i+2], pAdd[i+3]);
		}
	}

	waitForReload();
}
