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

	.globl default_exceptionhandler_start,default_exceptionhandler_end,default_exceptionhandler_patch
default_exceptionhandler_start:
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
	ori		r3,r3,MSR_IR|MSR_DR
	mtsrr1	r3
	
default_exceptionhandler_patch:
	li		r3,0
	stw		r3,EXCEPTION_NUMBER(r4)

	rlwinm.	r5,r5,0,30,30
	lis		r5,default_exceptionhandler_transfer@h
	ori		r5,r5,default_exceptionhandler_transfer@l
	mtsrr0	r5
	bne		1f
	lis		r5,c_default_exceptionhandler@h
	ori		r5,r5,c_default_exceptionhandler@l
	rfi
1:	
	lis			r5,_exceptionhandlertable@h
	ori			r5,r5,_exceptionhandlertable@l
	clrlwi		r5,r5,2
	clrlslwi	r3,r3,24,2
	lwzx		r5,r3,r5
	rfi
default_exceptionhandler_end:

default_exceptionhandler_transfer:
	stwu	sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address
	mtlr	r5								//move passed handler func adress to LR

	stmw r6,GPR6_OFFSET(sp)

	addi	r3,sp,0x08
	blrl
	
	.global systemcall_handler_start,systemcall_handler_end
systemcall_handler_start:
	mfspr  r9,1008
	ori    r10,r9,0x0008
	mtspr  1008,r10
	isync
	sync
	mtspr  1008,r9
	rfi
systemcall_handler_end:

	.extern _cpu_context_save_fp,_cpu_context_restore_fp
	.globl fpu_exception_handler
fpu_exception_handler:
	mfmsr	r5
	ori		r5,r5,MSR_FP
	mtmsr	r5
	isync
	lwz		r5,SRR1_OFFSET-8(r3)
	ori		r5,r5,MSR_FP
	mtsrr1	r5
	lwz		r5,_thr_allocated_fpctx@sda21(0)
	stw		r3,_thr_allocated_fpctx@sda21(0)
	cmpw	r5,r3
	beq		2f
	cmpwi	r5,0x0000
	beq		1f
	bl		_cpu_contextfp_dump
	bl		_cpu_context_save_fp
1:	bl		_cpu_context_restore_fp
2:	lwz		r5,CR_OFFSET-8(r3)
	mtcr	r5
	lwz		r5,LR_OFFSET-8(r3)
	mtlr	r5
	lwz		r5,SRR0_OFFSET-8(r3)
	mtsrr0	r5
	lwz		r5,CTR_OFFSET-8(r3)
	mtctr	r5
	lwz		r5,XER_OFFSET-8(r3)
	mtxer	r5
	lwz		r5,GPR5_OFFSET-8(r3)
	lwz		r4,GPR4_OFFSET-8(r3)
	lwz		r3,GPR3_OFFSET-8(r3)
	addi	sp,sp,EXCEPTION_FRAME_END
	rfi

