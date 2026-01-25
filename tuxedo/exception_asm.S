// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <tuxedo/asm.inc>
#include <tuxedo/ppc/spr.h>
#include <tuxedo/ppc/context.h>

FUNC_START_SIZED PPCExcptEntryGeneral

	// Backup GPR11/GPR12/CR
	mtsprg0 r11
	mtsprg1 r12
	mfcr    r11
	stw     r11, __ppc_excpt_buf@l+PPC_CONTEXT_CR(0)

	// Backup saved PC/MSR
	mfsrr0  r11
	mfsrr1  r12
	stw     r11, __ppc_excpt_buf@l+PPC_CONTEXT_PC(0)
	stw     r12, __ppc_excpt_buf@l+PPC_CONTEXT_MSR(0)

	// Reenable address translation later
	mfmsr   r11
	ori     r11, r11, MSR_IR|MSR_DR
	mtsrr1  r11

	// Set exception ID
	addi    r11, 0, 0 // this is intended to be patched

	// Check for nested exceptions
	andi.   r12, r12, MSR_RI
	beq     .Lnested_exception

	// Retrieve and set address of exception handler
	li      r12, __ppc_excpt_table@l
	rlwimi  r12, r11, 2, 20, 29
	lwz     r12, 0(r12)
	mtsrr0  r12
	rfi

.Lnested_exception:
	lis     r12, PPCExcptDefaultHandler@h
	ori     r12, r12, PPCExcptDefaultHandler@l
	mtsrr0  r12
	rfi

FUNC_END

FUNC_START_SIZED PPCExcptEntryFpu

	// Backup GPR11/GPR12/CR
	mtsprg0 r11
	mtsprg1 r12
	mfcr    r11
	stw     r11, __ppc_excpt_buf@l+PPC_CONTEXT_CR(0)

	// Backup saved MSR, enabling FPU in it
	mfsrr1  r12
	ori     r12, r12, MSR_FP
	stw     r12, __ppc_excpt_buf@l+PPC_CONTEXT_MSR(0)

	// Skip fast path if this is a nested exception
	andi.   r11, r12, MSR_RI
	beq     .Lfpu_slow_path

	// Skip fast path if cur FPU context != cur CPU context
	mfsprg2 r11
	mfsprg3 r12
	cmplw   cr2, r11, r12
	bne     cr2, .Lfpu_slow_path

	// Restore saved MSR
	lwz     r12, __ppc_excpt_buf@l+PPC_CONTEXT_MSR(0)
	mtsrr1  r12

	// Restore CR/GPR11/GPR12 and return
	lwz     r11, __ppc_excpt_buf@l+PPC_CONTEXT_CR(0)
	mtcr    r11
	mfsprg0 r11
	mfsprg1 r12
	rfi

.Lfpu_slow_path:
	// Backup and clear GQR0
	mfspr   r11, GQR0
	stw     r11, __ppc_excpt_buf@l+PPC_CONTEXT_LR(0)
	li      r11, 0
	mtspr   GQR0, r11

	// Backup saved PC
	mfsrr0  r11
	stw     r11, __ppc_excpt_buf@l+PPC_CONTEXT_PC(0)

	// Set address of FPU handler
	lis     r11, PPCExcptFpuHandler@h
	ori     r11, r11, PPCExcptFpuHandler@l
	mtsrr0  r11

	// Reenable address translation and FPU later
	mfmsr   r12
	ori     r12, r12, MSR_FP|MSR_IR|MSR_DR
	mtsrr1  r12

	// Load saved PC/MSR and jump to handler
	lwz     r11, __ppc_excpt_buf@l+PPC_CONTEXT_PC(0)
	lwz     r12, __ppc_excpt_buf@l+PPC_CONTEXT_MSR(0)
	rfi

FUNC_END

FUNC_START PPCExcptFpuHandler, text, local

	// Restore saved PC/MSR
	mtsrr0  r11
	mtsrr1  r12

	// Retrieve current context and FPU owner
	mfsprg2 r11
	mfsprg3 r12

	// Skip FPU save if FPU owner is NULL
	cmpwi   cr2, r12, 0
	beq-    cr2, .Lfpu_check_nested

	// Save FPU context
	.rept   32
	stfd    \+, PPC_CONTEXT_FPR+8*\+(r12)
	psq_st  \+, PPC_CONTEXT_PS +8*\+(r12), 0, 0
	.endr
	mffs    f0
	stfd    f0, PPC_CONTEXT_FPSCR(r12)

.Lfpu_check_nested:
	// If this is a nested exception, clear out FPU owner and skip FPU restore
	bne+    .Lfpu_restore
	li      r11, 0
	b       .Lfpu_return

.Lfpu_restore:
	// Skip FPU restore if current context is NULL
	cmpwi   cr2, r11, 0
	beq-    cr2, .Lfpu_return

	// Load FPU context
	lfd     f0, PPC_CONTEXT_FPSCR(r11)
	mtfsf   0xff, f0
	.rept   32
	psq_l   \+, PPC_CONTEXT_PS +8*\+(r11), 0, 0
	lfd     \+, PPC_CONTEXT_FPR+8*\+(r11)
	.endr

.Lfpu_return:
	// Set new FPU owner
	mtsprg3 r11

	// Restore CR/GQR0/GPR11/GPR12 and return
	lis     r12, __ppc_excpt_buf@ha
	lwz     r11, __ppc_excpt_buf@l+PPC_CONTEXT_CR(r12)
	mtcr    r11
	lwz     r11, __ppc_excpt_buf@l+PPC_CONTEXT_LR(r12)
	mtspr   GQR0, r11
	mfsprg0 r11
	mfsprg1 r12
	rfi

FUNC_END

FUNC_START PPCExcptDefaultHandler

	// Set up exception stack if needed
	mr      r12, sp
	beq-    .Lalready_in_excpt
	lis     sp, __ppc_excpt_sp@ha
	lwz     sp, __ppc_excpt_sp@l(sp)

.Lalready_in_excpt:
	// Push new stack frame + context struct
	stwu    sp, -(8+PPC_CONTEXT_FPSCR)(sp)

	// Save GPR0-31 except for 1,11,12
	.rept   32
	.if     \+!=1 && \+!=11 && \+!=12
	stw     \+, 8+PPC_CONTEXT_GPR+4*\+(sp)
	.endif
	.endr

	// Save GPR1,11,12
	mfsprg0 r4
	mfsprg1 r5
	stw     r12, 8+PPC_CONTEXT_GPR+4*1(sp)
	stw     r4, 8+PPC_CONTEXT_GPR+4*11(sp)
	stw     r5, 8+PPC_CONTEXT_GPR+4*12(sp)

	// Save PC,MSR,CR
	lis     r10, __ppc_excpt_buf@ha
	lwz     r3, __ppc_excpt_buf@l+PPC_CONTEXT_PC(r10)
	lwz     r4, __ppc_excpt_buf@l+PPC_CONTEXT_MSR(r10)
	lwz     r5, __ppc_excpt_buf@l+PPC_CONTEXT_CR(r10)
	stw     r3, 8+PPC_CONTEXT_PC(sp)
	stw     r4, 8+PPC_CONTEXT_MSR(sp)
	stw     r5, 8+PPC_CONTEXT_CR(sp)

	// Save LR,CTR,XER
	mflr    r3
	mfctr   r4
	mfxer   r5
	stw     r3, 8+PPC_CONTEXT_LR(sp)
	stw     r4, 8+PPC_CONTEXT_CTR(sp)
	stw     r5, 8+PPC_CONTEXT_XER(sp)

	// Reload SDA/SDA2
	lis     r2, _SDA2_BASE_@h
	ori     r2, r2, _SDA2_BASE_@l
	lis     r13, _SDA_BASE_@h
	ori     r13, r13, _SDA_BASE_@l

	// Force spilling FP regs into current thread context
	li      r3, 4
	stfiwx  f0, r3, sp

	// Jump to C exception handler
	mr      r3, r11
	addi    r4, sp, 8
	lwz     r11, PPCExcptCurPanicFn@sda21(0)
	mtctr   r11
	bctrl

	// Reload PC,MSR,CR,LR,CTR,XER
	lwz     r3, 8+PPC_CONTEXT_PC(sp)
	mtsrr0  r3
	lwz     r3, 8+PPC_CONTEXT_MSR(sp)
	mtsrr1  r3
	lwz     r3, 8+PPC_CONTEXT_CR(sp)
	mtcr    r3
	lwz     r3, 8+PPC_CONTEXT_LR(sp)
	mtlr    r3
	lwz     r3, 8+PPC_CONTEXT_CTR(sp)
	mtctr   r3
	lwz     r3, 8+PPC_CONTEXT_XER(sp)
	mtxer   r3

	// Reload GPRs and return
	lwz     r0, 8+PPC_CONTEXT_GPR+4*0(sp)
	lmw     r2, 8+PPC_CONTEXT_GPR+4*2(sp)
	lwz     r1, 8+PPC_CONTEXT_GPR+4*1(sp)
	rfi

FUNC_END
