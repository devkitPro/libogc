#include <asm.h>

#define EXCEPTION_PROLOG			\
	mfspr    r0,912;				\
	stw      r0,GQR0_OFFSET(sp);    \
	mfspr    r0,913;				\
	stw      r0,GQR1_OFFSET(sp);    \
	mfspr    r0,914;				\
	stw      r0,GQR2_OFFSET(sp);    \
	mfspr    r0,915;				\
	stw      r0,GQR3_OFFSET(sp);    \
	mfspr    r0,916;				\
	stw      r0,GQR4_OFFSET(sp);    \
	mfspr    r0,917;				\
	stw      r0,GQR5_OFFSET(sp);    \
	mfspr    r0,918;				\
	stw      r0,GQR6_OFFSET(sp);    \
	mfspr    r0,919;				\
	stw      r0,GQR7_OFFSET(sp)
	
#define EXCEPTION_EPILOG			\
	lwz		r4,GQR0_OFFSET(sp);		\
	mtspr	912,r4;					\
	lwz		r4,GQR1_OFFSET(sp);		\
	mtspr	913,r4;					\
	lwz		r4,GQR2_OFFSET(sp);		\
	mtspr	914,r4;					\
	lwz		r4,GQR3_OFFSET(sp);		\
	mtspr	915,r4;					\
	lwz		r4,GQR4_OFFSET(sp);		\
	mtspr	916,r4;					\
	lwz		r4,GQR5_OFFSET(sp);		\
	mtspr	917,r4;					\
	lwz		r4,GQR6_OFFSET(sp);		\
	mtspr	918,r4;					\
	lwz		r4,GQR7_OFFSET(sp);		\
	mtspr	919,r4


	.globl irqhandler_start,irqhandler_end
irqhandler_start:
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
	mr		r5,r3
	nop
	mfmsr	r3
	ori		r3,r3,MSR_IR|MSR_DR|MSR_RI
	mtsrr1	r3
	
	li		r3,4
	stw		r3,EXCEPTION_NUMBER(r4)

	rlwinm.	r5,r5,0,30,30
	lis		r5,irqhandler_transfer@h
	ori		r5,r5,irqhandler_transfer@l
	mtsrr0	r5
	bne		1f
	lis		r5,c_default_exceptionhandler@h
	ori		r5,r5,c_default_exceptionhandler@l
	rfi
1:	
	lis		r5,c_irqdispatcher@h
	ori		r5,r5,c_irqdispatcher@l
	rfi
irqhandler_end:

irqhandler_transfer:
	stwu	sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address
	mtlr	r5								//move passed handler func adress to LR

	EXCEPTION_PROLOG

	stw r6,GPR6_OFFSET(sp)
	stw r7,GPR7_OFFSET(sp)
	stw r8,GPR8_OFFSET(sp)
	stw r9,GPR9_OFFSET(sp)
	stw r10,GPR10_OFFSET(sp)
	stw r11,GPR11_OFFSET(sp)
	stw r12,GPR12_OFFSET(sp)
	stw r13,GPR13_OFFSET(sp)
	stw r14,GPR14_OFFSET(sp)
	stw r15,GPR15_OFFSET(sp)

	addi	r14,sp,0
	lis		r15,_thread_dispatch_disable_level@ha
	
	mfspr	r3,SPRG0
	cmpwi	r3,0
	bne		nested
	mfspr	sp,SPRG1

nested:
	addi	r3,r3,1
	lwz		r6,_thread_dispatch_disable_level@l(r15)
	mtspr	SPRG0,r3
	addi	r6,r6,1
	stw		r6,_thread_dispatch_disable_level@l(r15)

	addi	r3,r14,0x08
	blrl
	
	mfspr	r4,SPRG0
	lwz		r3,_thread_dispatch_disable_level@l(r15)
	addi	r4,r4,-1
	addic.	r3,r3,-1
	mtspr	SPRG0,r4
	stw		r3,_thread_dispatch_disable_level@l(r15)
	addi	sp,r14,0
	bne		easy_exit

	lis		r4,_context_switch_want@ha
	lwz		r5,_context_switch_want@l(r4)
	cmpwi	r5,0
	beq		easy_exit

switch:
	bl		__thread_dispatch

easy_exit:
	lwz		r4,CR_OFFSET(sp)
	mtcr	r4
	lwz		r4,LR_OFFSET(sp)
	mtlr	r4
	lwz		r4,CTR_OFFSET(sp)
	mtctr	r4
	lwz		r4,XER_OFFSET(sp)
	mtxer	r4

	EXCEPTION_EPILOG

	lwz		r15,GPR15_OFFSET(sp)
	lwz		r14,GPR14_OFFSET(sp)
	lwz		r13,GPR13_OFFSET(sp)
	lwz		r12,GPR12_OFFSET(sp)
	lwz		r11,GPR11_OFFSET(sp)
	lwz		r10,GPR10_OFFSET(sp)
	lwz		r9,GPR9_OFFSET(sp)
	lwz		r8,GPR8_OFFSET(sp)
	lwz		r7,GPR7_OFFSET(sp)
	lwz		r6,GPR6_OFFSET(sp)
	lwz		r5,GPR5_OFFSET(sp)

	mfmsr	r4
	rlwinm	r4,r4,0,19,17
	rlwinm	r4,r4,0,31,29
	mtmsr	r4
	isync

	lwz		r0,GPR0_OFFSET(sp)
	lwz		toc,GPR2_OFFSET(sp)

	lwz		r4,SRR0_OFFSET(sp)
	mtsrr0	r4
	lwz		r4,SRR1_OFFSET(sp)
	mtsrr1	r4

	lwz		r4,GPR4_OFFSET(sp)
	lwz		r3,GPR3_OFFSET(sp)
	addi	sp,sp,EXCEPTION_FRAME_END
	rfi
