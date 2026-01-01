#include <tuxedo/asm.inc>
#include <tuxedo/ppc/spr.h>
#include <tuxedo/ppc/context.h>

FUNC_START PPCExcptIrqHandler

	// Get current context
	mfsprg2 r12

	// Save GPR0-10
	.rept   11
	stw     r\+, PPC_CONTEXT_GPR+4*\+(r12)
	.endr

	// Save GPR11-13
	mfsprg0 r3
	mfsprg1 r4
	stw     r3,  PPC_CONTEXT_GPR+4*11(r12)
	stw     r4,  PPC_CONTEXT_GPR+4*12(r12)
	stw     r13, PPC_CONTEXT_GPR+4*13(r12)

	// Save PC,MSR,CR
	lis     r10, __ppc_excpt_buf@ha
	lwz     r3,  __ppc_excpt_buf@l+PPC_CONTEXT_PC(r10)
	lwz     r4,  __ppc_excpt_buf@l+PPC_CONTEXT_MSR(r10)
	lwz     r5,  __ppc_excpt_buf@l+PPC_CONTEXT_CR(r10)
	stw     r3,  PPC_CONTEXT_PC(r12)
	stw     r4,  PPC_CONTEXT_MSR(r12)
	stw     r5,  PPC_CONTEXT_CR(r12)

	// Save LR,CTR,XER
	mflr    r3
	mfctr   r4
	mfxer   r5
	stw     r3,  PPC_CONTEXT_LR(r12)
	stw     r4,  PPC_CONTEXT_CTR(r12)
	stw     r5,  PPC_CONTEXT_XER(r12)

	// Reload SDA/SDA2
	lis     r2, _SDA2_BASE_@h
	ori     r2, r2, _SDA2_BASE_@l
	lis     r13, _SDA_BASE_@h
	ori     r13, r13, _SDA_BASE_@l

	// Pivot to exception stack
	lwz     sp, __ppc_excpt_sp@sda21(0)

	// Call C interrupt dispatcher
	mr      r3, r11
	bl      KIrqDispatch

	// Check if we have to change context
	lwz     r11, __ppc_next_ctx@sda21(0)
	cmpwi   r11, 0
	mfsprg2 r12
	beq+    .Lirq_return

	// Save GPR14-31
	stmw    r14, PPC_CONTEXT_GPR+4*14(r12)

.Lcontext_load:
	// XX: Consider adding a thread switch callback here

	// Update current context
	li      r0, 0
	stw     r0, __ppc_next_ctx@sda21(0)
	mtsprg2 r11
	mr      r12, r11

	// Restore GPR14-31
	lmw     r14, PPC_CONTEXT_GPR+4*14(r12)

.Lirq_return:
	// Clear atomic reservation
	li      r0, 4
	stwcx.  r0, sp, r0

	// Restore MSR, taking into account whether this context owns FP
	mfsprg3 r11
	lwz     r3,  PPC_CONTEXT_MSR(r12)
	cmplw   r11, r12
	rlwinm  r3, r3, 0, 19, 17
	bne-    1f
	ori     r3, r3, MSR_FP
1:	mtsrr1  r3

	// Restore PC/CR/LR/CTR/XER
	lwz     r3,  PPC_CONTEXT_PC(r12)
	mtsrr0  r3
	lwz     r3,  PPC_CONTEXT_CR(r12)
	mtcr    r3
	lwz     r3,  PPC_CONTEXT_LR(r12)
	mtlr    r3
	lwz     r3,  PPC_CONTEXT_CTR(r12)
	mtctr   r3
	lwz     r3,  PPC_CONTEXT_XER(r12)
	mtxer   r3

	// Restore GPR0-11
	.rept   12
	lwz     r\+, PPC_CONTEXT_GPR+4*\+(r12)
	.endr

	// Restore GPR13,12
	lwz     r13, PPC_CONTEXT_GPR+4*13(r12)
	lwz     r12, PPC_CONTEXT_GPR+4*12(r12)

	// Finally return!
	rfi

FUNC_END

FUNC_START __PPCContextPrepareToJump

	mfsprg2 r5

	stw     r1,  PPC_CONTEXT_GPR+4*1(r5)
	stw     r2,  PPC_CONTEXT_GPR+4*2(r5)
	stw     r3,  PPC_CONTEXT_GPR+4*3(r5)
	stmw    r13, PPC_CONTEXT_GPR+4*13(r5)

	mflr    r6
	stw     r6,  PPC_CONTEXT_PC(r5)
	stw     r6,  PPC_CONTEXT_LR(r5)
	mfmsr   r6
	rlwinm  r6, r6, 0, 17, 15 // Remove EE
	or      r7, r6, r4
	rlwinm  r6, r6, 0, 19, 17 // Remove FP
	rlwinm  r6, r6, 0, 31, 29 // Remove RI
	stw     r7,  PPC_CONTEXT_MSR(r5)
	mtmsr   r6
	mfcr    r6
	stw     r6,  PPC_CONTEXT_CR(r5)

	li      r3, 0
	blr

FUNC_END

FUNC_START __PPCContextJump

	mr      r11, r3
	b       .Lcontext_load

FUNC_END
