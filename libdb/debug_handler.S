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

	.extern c_debug_handler
	.globl debug_handler_start,debug_handler_end,debug_handler_patch
debug_handler_start:
	mtspr	SPRG3,r4
	clrlwi	r4,sp,2			//make sp physical and move new value to r4
	stwu	r4,-EXCEPTION_FRAME_END(r4)
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
	nop
	mfmsr	r3
	ori		r3,r3,MSR_IR|MSR_DR|MSR_RI		//MSR_FP
	mtsrr1	r3

debug_handler_patch:
	li		r3,0
	stw		r3,EXCEPTION_NUMBER(r4)
	
	lis		r5,debug_handler_transfer@h
	ori		r5,r5,debug_handler_transfer@l
	mtsrr0	r5
	lis		r5,c_debug_handler@h
	ori		r5,r5,c_debug_handler@l
	rfi

debug_handler_end:

debug_handler_transfer:
	stwu	sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address
	mtlr	r5								//move passed handler func adress to LR

	EXCEPTION_PROLOG

	stmw r6,GPR6_OFFSET(sp)
/*	
	addi	r14,sp,0

	lis		sp,__debugstack@h
	ori		sp,sp,__debugstack@l
	
	lis		r0,0
	stw		r0,0(sp)
	stwu	sp,-16(sp)

	addi	r3,r14,0x08
*/
	addi	r3,sp,0x08
	blrl

//	addi	sp,r14,0

	lwz		r5,CR_OFFSET(sp)
	mtcr	r5
	lwz		r5,LR_OFFSET(sp)
	mtlr	r5
	lwz		r5,SRR1_OFFSET(sp)
	mtsrr1	r5
	lwz		r5,SRR0_OFFSET(sp)
	mtsrr0	r5
	lwz		r5,CTR_OFFSET(sp)
	mtctr	r5
	lwz		r5,XER_OFFSET(sp)
	mtxer	r5

	EXCEPTION_EPILOG

	lmw		r6,GPR6_OFFSET(sp)
	lwz		toc,GPR2_OFFSET(sp)
	lwz		r0,GPR0_OFFSET(sp)

	mfmsr	r4
	rlwinm	r4,r4,0,19,17
	rlwinm	r4,r4,0,31,29
	mtmsr	r4
	isync

	lwz		r5,GPR5_OFFSET(sp)
	lwz		r4,GPR4_OFFSET(sp)
	lwz		r3,GPR3_OFFSET(sp)
	addi	sp,sp,EXCEPTION_FRAME_END
	rfi

	.globl __set_ibar
__set_ibar:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	clrrwi	r3,r3,2
	mtspr	1010,r3
	isync
	sync
	mtmsr	r4
	blr

	.globl __enable_ibar
__enable_ibar:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	mfspr	r3,1010
	ori		r3,r3,0x0003
	mtspr	1010,r3
	isync
	sync
	mtmsr	r4
	blr

	.globl __disable_ibar
__disable_ibar:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	mfspr	r3,1010
	clrrwi	r3,r3,2
	mtspr	1010,r3
	isync
	sync
	mtmsr	r4
	blr

	.globl __clr_ibar
__clr_ibar:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	mtspr	1010,0
	isync
	sync
	mtmsr	r4
	blr

	.section .bss
	.balign 8

	.globl __debugstack_end,__debugstack
__debugstack_end:
	.space 0x4000
__debugstack:
