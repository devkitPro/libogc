#include <asm.h>

#define EXCEPTION_PROLOG			\
	mfspr    r0,912;				\
	stw      r0,GQR0_OFFSET(r4);    \
	mfspr    r0,913;				\
	stw      r0,GQR1_OFFSET(r4);    \
	mfspr    r0,914;				\
	stw      r0,GQR2_OFFSET(r4);    \
	mfspr    r0,915;				\
	stw      r0,GQR3_OFFSET(r4);    \
	mfspr    r0,916;				\
	stw      r0,GQR4_OFFSET(r4);    \
	mfspr    r0,917;				\
	stw      r0,GQR5_OFFSET(r4);    \
	mfspr    r0,918;				\
	stw      r0,GQR6_OFFSET(r4);    \
	mfspr    r0,919;				\
	stw      r0,GQR7_OFFSET(r4)
	
#define EXCEPTION_EPILOG			\
	lwz		r0,GQR0_OFFSET(r4);		\
	mtspr	912,r0;					\
	lwz		r0,GQR1_OFFSET(r4);		\
	mtspr	913,r0;					\
	lwz		r0,GQR2_OFFSET(r4);		\
	mtspr	914,r0;					\
	lwz		r0,GQR3_OFFSET(r4);		\
	mtspr	915,r0;					\
	lwz		r0,GQR4_OFFSET(r4);		\
	mtspr	916,r0;					\
	lwz		r0,GQR5_OFFSET(r4);		\
	mtspr	917,r0;					\
	lwz		r0,GQR6_OFFSET(r4);		\
	mtspr	918,r0;					\
	lwz		r0,GQR7_OFFSET(r4);		\
	mtspr	919,r0

	.globl debug_handler_start,debug_handler_end,debug_handler_patch
debug_handler_start:
	mtspr	SPRG3,r4
	lis		r4,debug_context@ha
	ori		r4,r4,debug_context@l
	clrlwi	r4,r4,2
	stw		r0,GPR0_OFFSET(r4)
	stw		sp,GPR1_OFFSET(r4)
	stw		toc,GPR2_OFFSET(r4)
	stw		r3,GPR3_OFFSET(r4)
	mfspr	r3,SPRG3
	stw		r3,GPR4_OFFSET(r4)
	stw		r5,GPR5_OFFSET(r4)
	mfcr	r3
	stw		r3,CR_OFFSET(r4)
	mflr	r3
	stw		r3,LR_OFFSET(r4)
	mfctr	r3
	stw		r3,CTR_OFFSET(r4)
	mfxer	r3
	stw		r3,XER_OFFSET(r4)
	mfmsr	r3
	stw		r3,MSR_OFFSET(r4)
	mfdar	r3
	stw		r3,DAR_OFFSET(r4)
	mfsrr0	r3
	stw		r3,SRR0_OFFSET(r4)
	mfsrr1	r3
	stw		r3,SRR1_OFFSET(r4)
	mr		r5,r3
	nop
	mfmsr	r3
	ori		r3,r3,MSR_IR|MSR_DR|MSR_FP
	mtsrr1	r3

debug_handler_patch:
	li		r3,0
	stw		r3,EXCEPTION_NUMBER(r4)
	
	rlwinm.	r5,r5,0,30,30
	lis		r5,debug_handler_transfer@ha
	ori		r5,r5,debug_handler_transfer@l
	mtsrr0	r5
	bne		1f
	lis		r5,c_default_exceptionhandler@ha
	ori		r5,r5,c_default_exceptionhandler@l
	sync
	isync
	rfi
1:
	lis		r5,c_debug_handler@ha
	ori		r5,r5,c_debug_handler@l
	sync
	isync
	rfi
debug_handler_end:

debug_handler_transfer:
	lis		r4,debug_context@ha
	ori		r4,r4,debug_context@l
	mtlr	r5

	EXCEPTION_PROLOG

	stmw r6,GPR6_OFFSET(r4)

	lis		sp,debug_stack@ha
	ori		sp,sp,debug_stack@l
	subi	sp,sp,4

	addi	r3,r4,0x08
	blrl

	lis		r4,debug_context@ha
	ori		r4,r4,debug_context@l

	lwz		r3,CR_OFFSET(r4)
	mtcr	r3
	lwz		r3,LR_OFFSET(r4)
	mtlr	r3
	lwz		r3,CTR_OFFSET(r4)
	mtctr	r3
	lwz		r3,XER_OFFSET(r4)
	mtxer	r3

	EXCEPTION_EPILOG

	lmw		r5,GPR5_OFFSET(r4)
	lwz		toc,GPR2_OFFSET(r4)
	lwz		r0,GPR0_OFFSET(r4)
	lwz		sp,GPR1_OFFSET(r4)

	mfmsr	r3
	rlwinm	r3,r3,0,17,15
	rlwinm	r3,r3,0,31,29
	mtmsr	r3
	sync
	isync

	lwz		r3,SRR0_OFFSET(r4)
	mtsrr0	r3
	lwz		r3,SRR1_OFFSET(r4)
	mtsrr1	r3
	lwz		r3,GPR3_OFFSET(r4)
	lwz		r4,GPR4_OFFSET(r4)
	sync
	isync
	rfi

	.section .bss
	.align 4
debug_context:
	.space 0x200

debug_stack:
	.space 0x2000
debug_stack_end:
