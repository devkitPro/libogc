#include <ogc/machine/asm.h>
#include "stubasm.h"

#define		CTX_SAVE	0x80004004
#define		RET_ADDR	0x80004000

#define		CACHE_SCRATCH	0x81000000

	.extern __PSInit
	.extern __CacheInit
	.extern __SystemInit
	.extern DCDisable
	.extern ICDisable
	.extern L2Disable
	.extern DCFlashInvalidate
	.extern ICFlashInvalidate
	.extern L2GlobalInvalidate
	.extern DCFlushRangeNoSync
	.extern ICInvalidateRange

	.globl __distub_take_plunge
	.globl __distub_enable_caches
	.globl __distub_disable_caches

	.text

__distub_take_plunge:
	# save LR + r31
	mflr	r0
	stw	r0, 4(sp)
	stwu	sp, -16(sp)
	stw	r31, 12(sp)

	# save context address
	lis	r4, CTX_SAVE@h
	stw	r3, CTX_SAVE@l(r4)

	# save MSR
	mfmsr	r4
	stw	r4, MSR_OFF(r3)

	# disable interrupts
	rlwinm	r4,r4,0,17,15
	mtmsr	r4

	# save GPRs
	stw	r1, R1_OFF(r3)
	stw	r2, R2_OFF(r3)
	stw	r13, R13_OFF(r3)
	stw	r14, R14_OFF(r3)
	stw	r15, R15_OFF(r3)
	stw	r16, R16_OFF(r3)
	stw	r17, R17_OFF(r3)
	stw	r18, R18_OFF(r3)
	stw	r19, R19_OFF(r3)
	stw	r20, R20_OFF(r3)
	stw	r21, R21_OFF(r3)
	stw	r22, R22_OFF(r3)
	stw	r23, R23_OFF(r3)
	stw	r24, R24_OFF(r3)
	stw	r25, R25_OFF(r3)
	stw	r26, R26_OFF(r3)
	stw	r27, R27_OFF(r3)
	stw	r28, R28_OFF(r3)
	stw	r29, R29_OFF(r3)
	stw	r30, R30_OFF(r3)
	stw	r31, R31_OFF(r3)

	# save SPRGx
	mfspr	r4, SPRG0
	stw	r4, SPRG0_OFF(r3)
	mfspr	r4, SPRG1
	stw	r4, SPRG1_OFF(r3)
	mfspr	r4, SPRG2
	stw	r4, SPRG2_OFF(r3)
	mfspr	r4, SPRG3
	stw	r4, SPRG3_OFF(r3)

	# save HIDx
	mfspr	r4, HID0
	stw	r4, HID0_OFF(r3)
	mfspr	r4, HID1
	stw	r4, HID1_OFF(r3)
	mfspr	r4, HID2
	stw	r4, HID2_OFF(r3)
	mfspr	r4, HID4
	stw	r4, HID4_OFF(r3)

	# save WPAR
	mfspr	r4, WPAR
	stw	r4, WPAR_OFF(r3)

	# save low MEM1
	li	r5, MEM1_WORDS
	mtctr	r5

	addi	r6, r3, MEM1_OFF - 4
	lis	r5,0x8000
	addi	r5, r5, -4
1:
	lwzu	r7, 4(r5)
	stwu	r7, 4(r6)
	bdnz	1b

	# flush and disable caches
	bl	__distub_disable_caches

	# set up return address for stub
	lis	r5, __distub_return@h
	ori	r5, r5, __distub_return@l
	lis	r4, RET_ADDR@h
	stw	r5, RET_ADDR@l(r4)

	sync

	# take the plunge
__distub_wait:
	b	__distub_wait

__distub_return:
	# get the context address
	lis	r4, CTX_SAVE@h
	lwz	r3, CTX_SAVE@l(r4)

	# restore GPRs
	lwz	r1, R1_OFF(r3)
	lwz	r2, R2_OFF(r3)
	lwz	r13, R13_OFF(r3)
	lwz	r14, R14_OFF(r3)
	lwz	r15, R15_OFF(r3)
	lwz	r16, R16_OFF(r3)
	lwz	r17, R17_OFF(r3)
	lwz	r18, R18_OFF(r3)
	lwz	r19, R19_OFF(r3)
	lwz	r20, R20_OFF(r3)
	lwz	r21, R21_OFF(r3)
	lwz	r22, R22_OFF(r3)
	lwz	r23, R23_OFF(r3)
	lwz	r24, R24_OFF(r3)
	lwz	r25, R25_OFF(r3)
	lwz	r26, R26_OFF(r3)
	lwz	r27, R27_OFF(r3)
	lwz	r28, R28_OFF(r3)
	lwz	r29, R29_OFF(r3)
	lwz	r30, R30_OFF(r3)
	lwz	r31, R31_OFF(r3)

	# initialize system partially
	bl	__PSInit
	bl	__CacheInit
	bl	__SystemInit

	# get the context address (again), this time into r31
	lis	r4, CTX_SAVE@h
	lwz	r31, CTX_SAVE@l(r4)

	# save IOS version
	lis	r5, 0x8000
	lwz	r3, 0x3140(r5)

	# restore low MEM1
	li	r5, MEM1_WORDS
	mtctr	r5

	addi	r6, r31, MEM1_OFF - 4
	lis	r5,0x8000
	addi	r5, r5, -4
1:
	lwzu	r7, 4(r6)
	stwu	r7, 4(r5)
	bdnz	1b

	# restore IOS version
	lis	r5, 0x8000
	stw	r3, 0x3140(r5)

	# flush low MEM1
	lis	r3, 0x8000
	li	r4, (MEM1_WORDS * 4)
        bl	DCFlushRangeNoSync
	lis	r3, 0x8000
	li	r4, (MEM1_WORDS * 4)
        bl	ICInvalidateRange
	sync

	# restore SPRGx
	lwz	r4, SPRG0_OFF(r31)
	mtspr	SPRG0, r4
	lwz	r4, SPRG1_OFF(r31)
	mtspr	SPRG1, r4
	lwz	r4, SPRG2_OFF(r31)
	mtspr	SPRG2, r4
	lwz	r4, SPRG3_OFF(r31)
	mtspr	SPRG3, r4

	# restore HIDx
	lwz	r4, HID0_OFF(r31)
	mtspr	HID0, r4
	lwz	r4, HID1_OFF(r31)
	mtspr	HID1, r4
	lwz	r4, HID2_OFF(r31)
	mtspr	HID2, r4
	lwz	r4, HID4_OFF(r31)
	mtspr	HID4, r4

	# restore WPAR
	lwz	r4, WPAR_OFF(r31)
	mtspr	WPAR, r4

	# restore MSR (and interrupts)
	lwz	r4, MSR_OFF(r31)
	mtmsr	r4

	# we grab the LR and r31 back from the (now configured) stack
	lwz	r0, 20(sp)
	lwz	r31, 12(sp)
	addi	sp, sp, 16
	mtlr	r0
	blr

__distub_ledon:
	# tray on!
	lis	r3, 0xcd80
	lwz	r4, 0xc0(r3)
	ori	r4, r4, 0x20
	stw	r4, 0xc0(r3)
	blr

__distub_ledoff:
	# tray on!
	lis	r3, 0xcd80
	lwz	r4, 0xc0(r3)
	li	r5, 0x20
	andc	r4, r4, r5
	stw	r4, 0xc0(r3)
	blr

__distub_ledblink:
	# tray blink!
	lis	r3, 0xcd80
	lwz	r4, 0xc0(r3)
	xori	r4, r4, 0x20
	stw	r4, 0xc0(r3)
	blr

__distub_disable_caches:
	# save LR + r31
	mflr	r0
	stw	r0, 4(sp)
	stwu	sp, -16(sp)
	stw	r31, 12(sp)

	# disable interrupts
	mfmsr	r31
	rlwinm	r4,r31,0,17,15
	mtmsr	r4

	sync

	# Flush and disable L1/L2 data caches

	# Load cache
	lis	r2,0x0001
	mtctr	r2
	lis	r4,CACHE_SCRATCH@h
	ori	r4,r4,CACHE_SCRATCH@l
1:
	lwz	r6,0(r4)
	addi	r4,r4,0x20	# Move to next block
	bdnz	1b		# decrement ctr, branch if necessary
	isync

	# Flush cache
	lis	r2,0x0001
	mtctr	r2
	lis	r4,CACHE_SCRATCH@h
	ori	r4,r4,CACHE_SCRATCH@l
	sync
1:
	dcbf	r0,r4
	addi	r4,r4,0x20	# move to next block
	bdnz	1b		# decrement & branch as appropriate

	bl	DCDisable
	bl	ICDisable
	bl	L2Disable
	bl	DCFlashInvalidate
	bl	ICFlashInvalidate
	bl	L2GlobalInvalidate

	# restore interrupts
	mtmsr	r31

	# restore LR + r31
	lwz	r0, 20(sp)
	lwz	r31, 12(sp)
	addi	sp, sp, 16
	mtlr	r0
	blr

/*
__distub_enable_caches:
	# save LR + r31
	mflr	r0
	stw	r0, 4(sp)
	stwu	sp, -16(sp)
	stw	r31, 12(sp)

	# disable interrupts
	mfmsr	r31
	rlwinm	r4,r31,0,17,15
	mtmsr	r4

	# initialize caches
	bl	__CacheInit

	# reenable interrupts
	mtmsr	r31

	# restore LR + r31
	lwz	r0, 20(sp)
	lwz	r31, 12(sp)
	addi	sp, sp, 16
	mtlr	r0
	blr*/

