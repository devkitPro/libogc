#include <asm.h>

	.globl __realmode
__realmode:
	clrlwi	r3,r3,2
	mtsrr0	r3
	mfmsr	r3
	rlwinm	r3,r3,0,28,25
	mtsrr1	r3
	rfi

	.globl __config24Mb
__config24Mb:
	li		r7,0
	lis		r4,0
	addi	r4,r4,2
	lis		r3,0x8000
	addi	r3,r3,0x1ff
	lis		r6,0x100
	addi	r6,r6,2
	lis		r5,0x8100
	addi	r5,r5,0xff
	isync
	mtspr	DBAT0U,r7
	mtspr	DBAT0L,r4
	mtspr	DBAT0U,r3
	isync
	mtspr	IBAT0U,r7
	mtspr	IBAT0L,r4
	mtspr	IBAT0U,r3
	isync
	mtspr	DBAT2U,r7
	mtspr	DBAT2L,r6
	mtspr	DBAT2U,r5
	isync
	mtspr	IBAT2U,r7
	mtspr	IBAT2L,r6
	mtspr	IBAT2U,r5
	isync
	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi

	.globl __config48Mb
__config48Mb:
	li		r7,0
	lis		r4,0
	addi	r4,r4,2
	lis		r3,0x8000
	addi	r3,r3,0x3ff
	lis		r6,0x200
	addi	r6,r6,2
	lis		r5,0x8200
	addi	r5,r5,0x1ff
	isync
	mtspr	DBAT0U,r7
	mtspr	DBAT0L,r4
	mtspr	DBAT0U,r3
	isync
	mtspr	IBAT0U,r7
	mtspr	IBAT0L,r4
	mtspr	IBAT0U,r3
	isync
	mtspr	DBAT2U,r7
	mtspr	DBAT2L,r6
	mtspr	DBAT2U,r5
	isync
	mtspr	IBAT2U,r7
	mtspr	IBAT2L,r6
	mtspr	IBAT2U,r5
	isync
	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi

