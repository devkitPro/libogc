// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <tuxedo/ppc/exception.h>
#include <tuxedo/ppc/cache.h>
#include <tuxedo/thread.h>

extern const u32 PPCExcptEntryGeneral[];
extern const u32 PPCExcptEntryFpu[];

void PPCExcptInit(void)
{
	for (unsigned i = PPC_EXCPT_RESET; i <= PPC_EXCPT_BKPT; i ++) {
		const u32* impl = i != PPC_EXCPT_FPU ? PPCExcptEntryGeneral : PPCExcptEntryFpu;
		u32* target = (u32*)(0x80000000 + i*0x100);
		__builtin_memcpy(target, impl, impl[-1]);

		if (i != PPC_EXCPT_FPU) {
			target[11] |= i;
			PPCExcptSetHandler(i, PPCExcptDefaultHandler);
		}
	}

	PPCDCacheFlushAsync((void*)0x80000000, 0x3000);
	PPCICacheInvalidate((void*)0x80000000, 0x3000);
	PPCSyncInner();
}

static void PPCExcptDefaultPanicFn(unsigned exid, PPCContext* ctx)
{
	PPCDebugPrint("PPCExcptDefaultPanicFn: exid=%u ctx=%p thread=%p", exid, ctx, KThreadGetSelf());

#if 0
	for (unsigned i = 0; i < 8; i ++) {
		PPCDebugPrint("R%02d=%08X R%02d=%08X R%02d=%08X R%02d=%08X",
			0+i, ctx->gpr[0+i], 8+i, ctx->gpr[8+i], 16+i, ctx->gpr[16+i], 24+i, ctx->gpr[24+i]);
	}
#endif

	PPCDebugPrint("PC=%08X LR=%08X SP=%08X MSR=%08X", ctx->pc, ctx->lr, ctx->gpr[1], ctx->msr);
	PPCDebugPrint("CR=%08X CTR=%08X XER=%08X", ctx->cr, ctx->ctr, ctx->xer);
	if (exid == PPC_EXCPT_DSI) {
		PPCDebugPrint("DAR=%08X DSISR=%08X", PPCMfspr(DAR), PPCMfspr(DSISR));
	}

	for (;;);
}

PPCExcptPanicFn PPCExcptCurPanicFn = PPCExcptDefaultPanicFn;
