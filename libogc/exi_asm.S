#include <asm.h>

	.globl __exi_imm
	//r3 = channel,r4 = buffer,r5 = len(bytes),r6 = type
__exi_imm:
	mulli	r3,r3,0x14
	oris	r3,r3,0xCC00
	subi	r5,r5,1
	slwi	r7,r5,4
	mtctr	r6
	slwi	r6,r6,2
	or		r6,r6,r7
	ori		r6,r6,1
	lwz		r7,0(r4)
	stw		r7,0x6810(r3)
	stw		r6,0x680C(r3)
1:
	lwz		r6,0x680C(r3)
	andi.	r6,r6,1
	bne		1b
	bdz		3f
	lwz		r6,0x6810(r3)
	subfic	r7,r5,3
	slwi	r7,r7,3
	srw		r6,r6,r7
2:
	stbx	r6,r4,r5
	srwi	r6,r6,8
	subic.	r5,r5,1
	bge		2b
3:
	blr
