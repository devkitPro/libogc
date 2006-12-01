#include <asm.h>

	.set	FP_SIZE,	8

	.set	FP_0, FPR0_OFFSET-8
	.set	FP_1, (FP_0 + FP_SIZE)
	.set	FP_2, (FP_1 + FP_SIZE)
	.set	FP_3, (FP_2 + FP_SIZE)
	.set	FP_4, (FP_3 + FP_SIZE)
	.set	FP_5, (FP_4 + FP_SIZE)
	.set	FP_6, (FP_5 + FP_SIZE)
	.set	FP_7, (FP_6 + FP_SIZE)
	.set	FP_8, (FP_7 + FP_SIZE)
	.set	FP_9, (FP_8 + FP_SIZE)
	.set	FP_10, (FP_9 + FP_SIZE)
	.set	FP_11, (FP_10 + FP_SIZE)
	.set	FP_12, (FP_11 + FP_SIZE)
	.set	FP_13, (FP_12 + FP_SIZE)
	.set	FP_14, (FP_13 + FP_SIZE)
	.set	FP_15, (FP_14 + FP_SIZE)
	.set	FP_16, (FP_15 + FP_SIZE)
	.set	FP_17, (FP_16 + FP_SIZE)
	.set	FP_18, (FP_17 + FP_SIZE)
	.set	FP_19, (FP_18 + FP_SIZE)
	.set	FP_20, (FP_19 + FP_SIZE)
	.set	FP_21, (FP_20 + FP_SIZE)
	.set	FP_22, (FP_21 + FP_SIZE)
	.set	FP_23, (FP_22 + FP_SIZE)
	.set	FP_24, (FP_23 + FP_SIZE)
	.set	FP_25, (FP_24 + FP_SIZE)
	.set	FP_26, (FP_25 + FP_SIZE)
	.set	FP_27, (FP_26 + FP_SIZE)
	.set	FP_28, (FP_27 + FP_SIZE)
	.set	FP_29, (FP_28 + FP_SIZE)
	.set	FP_30, (FP_29 + FP_SIZE)
	.set	FP_31, (FP_30 + FP_SIZE)
	.set	FP_FPSCR, (FP_31 + FP_SIZE)
	.set	PSFP_0, (FP_FPSCR + FP_SIZE)
	.set	PSFP_1, (PSFP_0 + FP_SIZE)
	.set	PSFP_2, (PSFP_1 + FP_SIZE)
	.set	PSFP_3, (PSFP_2 + FP_SIZE)
	.set	PSFP_4, (PSFP_3 + FP_SIZE)
	.set	PSFP_5, (PSFP_4 + FP_SIZE)
	.set	PSFP_6, (PSFP_5 + FP_SIZE)
	.set	PSFP_7, (PSFP_6 + FP_SIZE)
	.set	PSFP_8, (PSFP_7 + FP_SIZE)
	.set	PSFP_9, (PSFP_8 + FP_SIZE)
	.set	PSFP_10, (PSFP_9 + FP_SIZE)
	.set	PSFP_11, (PSFP_10 + FP_SIZE)
	.set	PSFP_12, (PSFP_11 + FP_SIZE)
	.set	PSFP_13, (PSFP_12 + FP_SIZE)
	.set	PSFP_14, (PSFP_13 + FP_SIZE)
	.set	PSFP_15, (PSFP_14 + FP_SIZE)
	.set	PSFP_16, (PSFP_15 + FP_SIZE)
	.set	PSFP_17, (PSFP_16 + FP_SIZE)
	.set	PSFP_18, (PSFP_17 + FP_SIZE)
	.set	PSFP_19, (PSFP_18 + FP_SIZE)
	.set	PSFP_20, (PSFP_19 + FP_SIZE)
	.set	PSFP_21, (PSFP_20 + FP_SIZE)
	.set	PSFP_22, (PSFP_21 + FP_SIZE)
	.set	PSFP_23, (PSFP_22 + FP_SIZE)
	.set	PSFP_24, (PSFP_23 + FP_SIZE)
	.set	PSFP_25, (PSFP_24 + FP_SIZE)
	.set	PSFP_26, (PSFP_25 + FP_SIZE)
	.set	PSFP_27, (PSFP_26 + FP_SIZE)
	.set	PSFP_28, (PSFP_27 + FP_SIZE)
	.set	PSFP_29, (PSFP_28 + FP_SIZE)
	.set	PSFP_30, (PSFP_29 + FP_SIZE)
	.set	PSFP_31, (PSFP_30 + FP_SIZE)

	.globl _cpu_context_save_fp
_cpu_context_save_fp:
	lhz		r4,STATE_OFFSET-8(r3)
	ori		r4,r4,0x0001
	sth		r4,STATE_OFFSET-8(r3)
	stfd	fr0, FP_0(r3)
	stfd	fr1, FP_1(r3)
	stfd	fr2, FP_2(r3)
	stfd	fr3, FP_3(r3)
	stfd	fr4, FP_4(r3)
	stfd	fr5, FP_5(r3)
	stfd	fr6, FP_6(r3)
	stfd	fr7, FP_7(r3)
	stfd	fr8, FP_8(r3)
	stfd	fr9, FP_9(r3)
	stfd	fr10, FP_10(r3)
	stfd	fr11, FP_11(r3)
	stfd	fr12, FP_12(r3)
	stfd	fr13, FP_13(r3)
	stfd	fr14, FP_14(r3)
	stfd	fr15, FP_15(r3)
	stfd	fr16, FP_16(r3)
	stfd	fr17, FP_17(r3)
	stfd	fr18, FP_18(r3)
	stfd	fr19, FP_19(r3)
	stfd	fr20, FP_20(r3)
	stfd	fr21, FP_21(r3)
	stfd	fr22, FP_22(r3)
	stfd	fr23, FP_23(r3)
	stfd	fr24, FP_24(r3)
	stfd	fr25, FP_25(r3)
	stfd	fr26, FP_26(r3)
	stfd	fr27, FP_27(r3)
	stfd	fr28, FP_28(r3)
	stfd	fr29, FP_29(r3)
	stfd	fr30, FP_30(r3)
	stfd	fr31, FP_31(r3)
	mffs	fr0
	stfd	fr0, FP_FPSCR(r3)
	lfd		fr0, FP_0(r3)
	mfspr	r4,920
	extrwi.	r4,r4,1,2
	beq		1f
	psq_st	fr0,PSFP_0(r3),0,0
	psq_st	fr1,PSFP_1(r3),0,0
	psq_st	fr2,PSFP_2(r3),0,0
	psq_st	fr3,PSFP_3(r3),0,0
	psq_st	fr4,PSFP_4(r3),0,0
	psq_st	fr5,PSFP_5(r3),0,0
	psq_st	fr6,PSFP_6(r3),0,0
	psq_st	fr7,PSFP_7(r3),0,0
	psq_st	fr8,PSFP_8(r3),0,0
	psq_st	fr9,PSFP_9(r3),0,0
	psq_st	fr10,PSFP_10(r3),0,0
	psq_st	fr11,PSFP_11(r3),0,0
	psq_st	fr12,PSFP_12(r3),0,0
	psq_st	fr13,PSFP_13(r3),0,0
	psq_st	fr14,PSFP_14(r3),0,0
	psq_st	fr15,PSFP_15(r3),0,0
	psq_st	fr16,PSFP_16(r3),0,0
	psq_st	fr17,PSFP_17(r3),0,0
	psq_st	fr18,PSFP_18(r3),0,0
	psq_st	fr19,PSFP_19(r3),0,0
	psq_st	fr20,PSFP_20(r3),0,0
	psq_st	fr21,PSFP_21(r3),0,0
	psq_st	fr22,PSFP_22(r3),0,0
	psq_st	fr23,PSFP_23(r3),0,0
	psq_st	fr24,PSFP_24(r3),0,0
	psq_st	fr25,PSFP_25(r3),0,0
	psq_st	fr26,PSFP_26(r3),0,0
	psq_st	fr27,PSFP_27(r3),0,0
	psq_st	fr28,PSFP_28(r3),0,0
	psq_st	fr29,PSFP_29(r3),0,0
	psq_st	fr30,PSFP_30(r3),0,0
	psq_st	fr31,PSFP_31(r3),0,0
1:	
	blr

	.globl _cpu_context_restore_fp
_cpu_context_restore_fp:
	lhz		r4,STATE_OFFSET-8(r3)
	clrlwi.	r4,r4,31
	beq		2f
	lfd		fr0, FP_FPSCR(r3)
	mtfsf	255, fr0
	mfspr	r4,920
	extrwi.	r4,r4,1,2
	beq		1f
	psq_l	fr0,PSFP_0(r3),0,0
	psq_l	fr1,PSFP_1(r3),0,0
	psq_l	fr2,PSFP_2(r3),0,0
	psq_l	fr3,PSFP_3(r3),0,0
	psq_l	fr4,PSFP_4(r3),0,0
	psq_l	fr5,PSFP_5(r3),0,0
	psq_l	fr6,PSFP_6(r3),0,0
	psq_l	fr7,PSFP_7(r3),0,0
	psq_l	fr8,PSFP_8(r3),0,0
	psq_l	fr9,PSFP_9(r3),0,0
	psq_l	fr10,PSFP_10(r3),0,0
	psq_l	fr11,PSFP_11(r3),0,0
	psq_l	fr12,PSFP_12(r3),0,0
	psq_l	fr13,PSFP_13(r3),0,0
	psq_l	fr14,PSFP_14(r3),0,0
	psq_l	fr15,PSFP_15(r3),0,0
	psq_l	fr16,PSFP_16(r3),0,0
	psq_l	fr17,PSFP_17(r3),0,0
	psq_l	fr18,PSFP_18(r3),0,0
	psq_l	fr19,PSFP_19(r3),0,0
	psq_l	fr20,PSFP_20(r3),0,0
	psq_l	fr21,PSFP_21(r3),0,0
	psq_l	fr22,PSFP_22(r3),0,0
	psq_l	fr23,PSFP_23(r3),0,0
	psq_l	fr24,PSFP_24(r3),0,0
	psq_l	fr25,PSFP_25(r3),0,0
	psq_l	fr26,PSFP_26(r3),0,0
	psq_l	fr27,PSFP_27(r3),0,0
	psq_l	fr28,PSFP_28(r3),0,0
	psq_l	fr29,PSFP_29(r3),0,0
	psq_l	fr30,PSFP_30(r3),0,0
	psq_l	fr31,PSFP_31(r3),0,0

1:	lfd		fr0, FP_0(r3)
	lfd		fr1, FP_1(r3)
	lfd		fr2, FP_2(r3)
	lfd		fr3, FP_3(r3)
	lfd		fr4, FP_4(r3)
	lfd		fr5, FP_5(r3)
	lfd		fr6, FP_6(r3)
	lfd		fr7, FP_7(r3)
	lfd		fr8, FP_8(r3)
	lfd		fr9, FP_9(r3)
	lfd		fr10, FP_10(r3)
	lfd		fr11, FP_11(r3)
	lfd		fr12, FP_12(r3)
	lfd		fr13, FP_13(r3)
	lfd		fr14, FP_14(r3)
	lfd		fr15, FP_15(r3)
	lfd		fr16, FP_16(r3)
	lfd		fr17, FP_17(r3)
	lfd		fr18, FP_18(r3)
	lfd		fr19, FP_19(r3)
	lfd		fr20, FP_20(r3)
	lfd		fr21, FP_21(r3)
	lfd		fr22, FP_22(r3)
	lfd		fr23, FP_23(r3)
	lfd		fr24, FP_24(r3)
	lfd		fr25, FP_25(r3)
	lfd		fr26, FP_26(r3)
	lfd		fr27, FP_27(r3)
	lfd		fr28, FP_28(r3)
	lfd		fr29, FP_29(r3)
	lfd		fr30, FP_30(r3)
	lfd		fr31, FP_31(r3)

2:	blr

	.globl _cpu_context_switch
_cpu_context_switch:
	sync
	isync

	stw sp,GPR1_OFFSET-8(r3)
	stw toc,GPR2_OFFSET-8(r3)
	stmw r13,GPR13_OFFSET-8(r3)

	mfctr	r6
	stw	r6, CTR_OFFSET-8(r3)
	mfcr	r6
	stw	r6, CR_OFFSET-8(r3)
	mflr	r7
	stw	r7, LR_OFFSET-8(r3)
	mfmsr	r8
	stw	r8, MSR_OFFSET-8(r3)
	mfdar	r8
	stw	r8, DAR_OFFSET-8(r3)

	mfspr	r6,913
	stw	r6,GQR1_OFFSET-8(r3)
	mfspr	r6,914
	stw	r6,GQR2_OFFSET-8(r3)
	mfspr	r6,915
	stw	r6,GQR3_OFFSET-8(r3)
	mfspr	r6,916
	stw	r6,GQR4_OFFSET-8(r3)
	mfspr	r6,917
	stw	r6,GQR5_OFFSET-8(r3)
	mfspr	r6,918
	stw	r6,GQR6_OFFSET-8(r3)
	mfspr	r6,919
	stw	r6,GQR7_OFFSET-8(r3)

	lwz	r6, GQR1_OFFSET-8(r4)
	mtspr	913,r6
	lwz	r6, GQR2_OFFSET-8(r4)
	mtspr	914,r6
	lwz	r6, GQR3_OFFSET-8(r4)
	mtspr	915,r6
	lwz	r6, GQR4_OFFSET-8(r4)
	mtspr	916,r6
	lwz	r6, GQR5_OFFSET-8(r4)
	mtspr	917,r6
	lwz	r6, GQR6_OFFSET-8(r4)
	mtspr	918,r6
	lwz	r6, GQR7_OFFSET-8(r4)
	mtspr	919,r6

	lwz	r6, CTR_OFFSET-8(r4)
	mtctr	r6
	lwz	r6, CR_OFFSET-8(r4)
	mtcrf	255, r6
	lwz	r7, LR_OFFSET-8(r4)
	mtlr	r7
	lwz	r8, MSR_OFFSET-8(r4)
	mtmsr	r8

	lmw r13,GPR13_OFFSET-8(r4)
	lwz toc,GPR2_OFFSET-8(r4)
	lwz sp,GPR1_OFFSET-8(r4)

	blr

	.globl _cpu_context_switch_ex
_cpu_context_switch_ex:
	sync
	isync

	stw sp,GPR1_OFFSET-8(r3)
	stw toc,GPR2_OFFSET-8(r3)
	stmw r5,GPR5_OFFSET-8(r3)

	mfctr	r6
	stw	r6, CTR_OFFSET-8(r3)
	mfcr	r6
	stw	r6, CR_OFFSET-8(r3)
	mflr	r7
	stw	r7, LR_OFFSET-8(r3)
	mfxer	r7
	stw	r8, XER_OFFSET-8(r3)
	mfmsr	r8
	stw	r8, MSR_OFFSET-8(r3)
	mfdar	r8
	stw	r8, DAR_OFFSET-8(r3)
	mfsrr0	r8
	stw	r8, SRR0_OFFSET-8(r3)
	mfsrr1	r8
	stw	r8, SRR1_OFFSET-8(r3)

	mfspr	r6,913
	stw	r6,GQR1_OFFSET-8(r3)
	mfspr	r6,914
	stw	r6,GQR2_OFFSET-8(r3)
	mfspr	r6,915
	stw	r6,GQR3_OFFSET-8(r3)
	mfspr	r6,916
	stw	r6,GQR4_OFFSET-8(r3)
	mfspr	r6,917
	stw	r6,GQR5_OFFSET-8(r3)
	mfspr	r6,918
	stw	r6,GQR6_OFFSET-8(r3)
	mfspr	r6,919
	stw	r6,GQR7_OFFSET-8(r3)

	lwz	r6, GQR1_OFFSET-8(r4)
	mtspr	913,r6
	lwz	r6, GQR2_OFFSET-8(r4)
	mtspr	914,r6
	lwz	r6, GQR3_OFFSET-8(r4)
	mtspr	915,r6
	lwz	r6, GQR4_OFFSET-8(r4)
	mtspr	916,r6
	lwz	r6, GQR5_OFFSET-8(r4)
	mtspr	917,r6
	lwz	r6, GQR6_OFFSET-8(r4)
	mtspr	918,r6
	lwz	r6, GQR7_OFFSET-8(r4)
	mtspr	919,r6

	lwz	r6, CTR_OFFSET-8(r4)
	mtctr	r6
	lwz	r6, CR_OFFSET-8(r4)
	mtcrf	255, r6
	lwz	r7, LR_OFFSET-8(r4)
	mtlr	r7
	lwz	r7, XER_OFFSET-8(r4)
	mtxer	r7
	lwz	r8, MSR_OFFSET-8(r4)
	mtmsr	r8
	lwz	r8, SRR0_OFFSET-8(r4)
	mtsrr0	r8
	lwz	r8, SRR1_OFFSET-8(r4)
	mtsrr1	r8

	lmw r5,GPR5_OFFSET-8(r4)
	lwz toc,GPR2_OFFSET-8(r4)
	lwz sp,GPR1_OFFSET-8(r4)

	blr

	.globl _cpu_context_save
_cpu_context_save:
	sync
	isync

	stw sp,GPR1_OFFSET-8(r3)
	stw toc,GPR2_OFFSET-8(r3)
	stmw r13,GPR13_OFFSET-8(r3)

	mfctr	r6
	stw	r6, CTR_OFFSET-8(r3)
	mfcr	r6
	stw	r6, CR_OFFSET-8(r3)
	mflr	r7
	stw	r7, LR_OFFSET-8(r3)
	mfmsr	r8
	stw	r8, MSR_OFFSET-8(r3)

	mfspr	r6,913
	stw	r6,GQR1_OFFSET-8(r3)
	mfspr	r6,914
	stw	r6,GQR2_OFFSET-8(r3)
	mfspr	r6,915
	stw	r6,GQR3_OFFSET-8(r3)
	mfspr	r6,916
	stw	r6,GQR4_OFFSET-8(r3)
	mfspr	r6,917
	stw	r6,GQR5_OFFSET-8(r3)
	mfspr	r6,918
	stw	r6,GQR6_OFFSET-8(r3)
	mfspr	r6,919
	stw	r6,GQR7_OFFSET-8(r3)

	blr

	.globl _cpu_context_restore
_cpu_context_restore:
	lwz sp,GPR1_OFFSET-8(r3)
	lwz toc,GPR2_OFFSET-8(r3)
	lmw r13,GPR13_OFFSET-8(r3)

	lwz	r6, CTR_OFFSET-8(r3)
	mtctr	r6
	lwz	r6, CR_OFFSET-8(r3)
	mtcrf	255, r6
	lwz	r7, LR_OFFSET-8(r3)
	mtlr	r7
	lwz	r8, MSR_OFFSET-8(r3)
	mtmsr	r8

	lwz	r6, GQR1_OFFSET-8(r3)
	mtspr	913,r6
	lwz	r6, GQR2_OFFSET-8(r3)
	mtspr	914,r6
	lwz	r6, GQR3_OFFSET-8(r3)
	mtspr	915,r6
	lwz	r6, GQR4_OFFSET-8(r3)
	mtspr	916,r6
	lwz	r6, GQR5_OFFSET-8(r3)
	mtspr	917,r6
	lwz	r6, GQR6_OFFSET-8(r3)
	mtspr	918,r6
	lwz	r6, GQR7_OFFSET-8(r3)
	mtspr	919,r6

	blr

	.globl _cpu_context_save_ex
_cpu_context_save_ex:
	sync
	isync

	stw		r0,GPR0_OFFSET-8(r3)
	stw		sp,GPR1_OFFSET-8(r3)
	stw		toc,GPR2_OFFSET-8(r3)
	stmw	r3,GPR3_OFFSET-8(r3)

	mfctr	r6
	stw		r6, CTR_OFFSET-8(r3)
	mfcr	r6
	stw		r6, CR_OFFSET-8(r3)
	mflr	r7
	stw		r7, LR_OFFSET-8(r3)
	mfxer	r7
	stw		r8, XER_OFFSET-8(r3)
	mfmsr	r8
	stw		r8, MSR_OFFSET-8(r3)
	mfdar	r8
	stw		r8, DAR_OFFSET-8(r3)
	mfsrr0	r8
	stw		r8, SRR0_OFFSET-8(r3)
	mfsrr1	r8
	stw		r8, SRR1_OFFSET-8(r3)

	mfspr	r6,913
	stw	r6,GQR1_OFFSET-8(r3)
	mfspr	r6,914
	stw	r6,GQR2_OFFSET-8(r3)
	mfspr	r6,915
	stw	r6,GQR3_OFFSET-8(r3)
	mfspr	r6,916
	stw	r6,GQR4_OFFSET-8(r3)
	mfspr	r6,917
	stw	r6,GQR5_OFFSET-8(r3)
	mfspr	r6,918
	stw	r6,GQR6_OFFSET-8(r3)
	mfspr	r6,919
	stw	r6,GQR7_OFFSET-8(r3)

	blr

	.globl _cpu_context_restore_ex
_cpu_context_restore_ex:

	lwz		r6, CTR_OFFSET-8(r3)
	mtctr	r6
	lwz		r6, CR_OFFSET-8(r3)
	mtcrf	255, r6
	lwz		r7, LR_OFFSET-8(r3)
	mtlr	r7
	lwz		r7, XER_OFFSET-8(r3)
	mtxer	r7
	lwz		r8, MSR_OFFSET-8(r3)
	mtmsr	r8
	lwz		r8, SRR0_OFFSET-8(r3)
	mtsrr0	r8
	lwz		r8, SRR1_OFFSET-8(r3)
	mtsrr1	r8

	lwz	r6, GQR1_OFFSET-8(r3)
	mtspr	913,r6
	lwz	r6, GQR2_OFFSET-8(r3)
	mtspr	914,r6
	lwz	r6, GQR3_OFFSET-8(r3)
	mtspr	915,r6
	lwz	r6, GQR4_OFFSET-8(r3)
	mtspr	916,r6
	lwz	r6, GQR5_OFFSET-8(r3)
	mtspr	917,r6
	lwz	r6, GQR6_OFFSET-8(r3)
	mtspr	918,r6
	lwz	r6, GQR7_OFFSET-8(r3)
	mtspr	919,r6

	lwz		sp,GPR1_OFFSET-8(r3)
	lwz		toc,GPR2_OFFSET-8(r3)
	lmw		r4,GPR4_OFFSET-8(r3)
	lwz		r3,GPR3_OFFSET-8(r3)

	blr
