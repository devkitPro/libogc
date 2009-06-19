#include <asm.h>

	.globl __realmode
__realmode:
	clrlwi	r3,r3,2
	mtsrr0	r3
	mfmsr	r3
	rlwinm	r3,r3,0,28,25
	mtsrr1	r3
	rfi

	// complete init sequence taken from bootmii's ppc skeleton. thanks to segher
	// after a talk with dhewg we came to that point that it's good to wipe+setup BATS correctly
	.globl __configBATS
__configBATS:
	// HID0 = 00110c64:
	// bus checkstops off, sleep modes off,
	// caches off, caches invalidate,
	// store gathering off, enable data cache
	// flush assist, enable branch target cache,
	// enable branch history table
	lis		r3,0x0011
	ori		r3,r3,0x0c64
	mtspr	HID0,r3
	isync
	
	mfmsr	r4
	ori		r4,r4,MSR_FP	// MSR = 00002000 (FP on)
	mtmsr	r4
	
	ori		r3,r3,0xc000	// d-cache & i-cache enable
	mtspr	HID0,r3
	isync

#if defined(HW_RVL)
	lis		r3,0x8200		//bits set: H4A(HID4 access), SBE(2nd BAT enabled)
	mtspr	HID4,r3
	isync
#endif

	// clear all BATs
	li		r0,0
	mtspr	IBAT0U,r0; mtspr	IBAT1U,r0; mtspr	IBAT2U,r0; mtspr	IBAT3U,r0		// IBAT0...3
	mtspr	DBAT0U,r0; mtspr	DBAT1U,r0; mtspr	DBAT2U,r0; mtspr	DBAT3U,r0		// DBAT0...3
#if defined(HW_RVL)
	mtspr	IBAT4U,r0; mtspr	IBAT5U,r0; mtspr	IBAT6U,r0; mtspr	IBAT7U,r0		// IBAT4...7
	mtspr	DBAT4U,r0; mtspr	DBAT5U,r0; mtspr	DBAT6U,r0; mtspr	DBAT7U,r0		// DBAT4...7
#endif
	isync

	// clear all SRs
	lis		r0,0x8000
	mtsr	0,r0; mtsr	1,r0; mtsr	2,r0; mtsr	3,r0; mtsr	4,r0; mtsr	5,r0; mtsr	6,r0
	mtsr	7,r0; mtsr	8,r0; mtsr	9,r0; mtsr	10,r0; mtsr	11,r0; mtsr	12,r0; mtsr	13,r0
	mtsr	14,r0; mtsr	15,r0
	isync

	// set [DI]BAT0 for 256MB@80000000,
	// real 00000000, WIMG=0000, R/W
	li		r3,2
	lis		r4,0x8000
	ori		r4,r4,0x1fff
	mtspr	IBAT0L,r3
	mtspr	IBAT0U,r4
	mtspr	DBAT0L,r3
	mtspr	DBAT0U,r4
	isync
	
#if defined(HW_RVL)
	// set [DI]BAT4 for 256MB@90000000,
	// real 10000000, WIMG=0000, R/W
	addis	r3,r3,0x1000
	addis	r4,r4,0x1000
	mtspr	IBAT4L,r3
	mtspr	IBAT4U,r4
	mtspr	DBAT4L,r3
	mtspr	DBAT4U,r4
	isync
#endif
	
	// set DBAT1 for 256MB@c0000000,
	// real 00000000, WIMG=0101, R/W
	li		r3,0x2a
	lis		r4,0xc000
	ori		r4,r4,0x1fff
	mtspr	DBAT1L,r3
	mtspr	DBAT1U,r4
	isync
	
#if defined(HW_RVL)
	// set DBAT5 for 256MB@d0000000,
	// real 10000000, WIMG=0101, R/W
	addis	r3,r3,0x1000
	addis	r4,r4,0x1000
	mtspr	DBAT5L,r3
	mtspr	DBAT5U,r4
	isync
#endif

	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi
	
	.globl __configMEM1_24Mb
__configMEM1_24Mb:
	// set [DI]BAT0 for 16Mb@80000000
	// real 00000000, WIMG=0000, R/W
	li		r7,0
	li		r4,2
	lis		r3,0x8000
	addi	r3,r3,0x1ff
	mtspr	IBAT0U,r7
	mtspr	IBAT0L,r4
	mtspr	IBAT0U,r3
	isync
	mtspr	DBAT0U,r7
	mtspr	DBAT0L,r4
	mtspr	DBAT0U,r3
	isync
	
	// set [DI]BAT2 for 8Mb@81000000
	// real 01000000, WIMG=0000, R/W
	lis		r3,0x8100
	addi	r3,r3,0x00ff
	addis	r4,r4,0x0100
	mtspr	IBAT2U,r7
	mtspr	IBAT2L,r4
	mtspr	IBAT2U,r3
	isync
	mtspr	DBAT2U,r7
	mtspr	DBAT2L,r4
	mtspr	DBAT2U,r3
	isync
	
	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi

	.globl __configMEM1_48Mb
__configMEM1_48Mb:
	// set [DI]BAT0 for 32Mb@80000000
	// real 00000000, WIMG=0000, R/W
	li		r7,0
	li		r4,2
	lis		r3,0x8000
	addi	r3,r3,0x3ff
	mtspr	IBAT0U,r7
	mtspr	IBAT0L,r4
	mtspr	IBAT0U,r3
	isync
	mtspr	DBAT0U,r7
	mtspr	DBAT0L,r4
	mtspr	DBAT0U,r3
	isync
	
	// set [DI]BAT2 for 16Mb@82000000
	// real 02000000, WIMG=0000, R/W
	lis		r3,0x8200
	addi	r3,r3,0x01ff
	addis	r4,r4,0x0200
	mtspr	IBAT2U,r7
	mtspr	IBAT2L,r4
	mtspr	IBAT2U,r3
	isync
	mtspr	DBAT2U,r7
	mtspr	DBAT2L,r4
	mtspr	DBAT2U,r3
	isync

	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi

#if defined(HW_RVL)
	.globl __configMEM2_64Mb
__configMEM2_64Mb:
	// set [DI]BAT4 for 64Mb@90000000
	// real 10000000, WIMG=0000, R/W
	li		r7,0
	lis		r4,0x1000
	addi	r5,r4,2
	lis		r3,0x9000
	addi	r3,r3,0x7ff
	mtspr	IBAT4U,r7
	mtspr	IBAT4L,r5
	mtspr	IBAT4U,r3
	isync
	mtspr	DBAT4U,r7
	mtspr	DBAT4L,r5
	mtspr	DBAT4U,r3
	isync

	// set DBAT5 for 64Mb@d0000000
	// real 10000000, WIMG=0101, R/W
	addi	r5,r4,0x2a
	addis	r3,r3,0x4000
	mtspr	DBAT5U,r7
	mtspr	DBAT5L,r5
	mtspr	DBAT5U,r3
	isync

	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi	

	.globl __configMEM2_128Mb
__configMEM2_128Mb:
	// set [DI]BAT4 for 128Mb@90000000
	// real 10000000, WIMG=0000, R/W
	li		r7,0
	lis		r4,0x1000
	addi	r5,r4,2
	lis		r3,0x9000
	addi	r3,r3,0xfff
	mtspr	IBAT4U,r7
	mtspr	IBAT4L,r5
	mtspr	IBAT4U,r3
	isync
	mtspr	DBAT4U,r7
	mtspr	DBAT4L,r5
	mtspr	DBAT4U,r3
	isync
	
	// set DBAT5 for 128Mb@d0000000
	// real 10000000, WIMG=0101, R/W
	addi	r5,r4,0x2a
	addis	r3,r3,0x4000
	mtspr	DBAT5U,r7
	mtspr	DBAT5L,r6
	mtspr	DBAT5U,r5
	isync

	mfmsr	r3
	ori		r3,r3,MSR_DR|MSR_IR
	mtsrr1	r3
	mflr	r3
	mtsrr0	r3
	rfi	
#endif

	.extern ICFlashInvalidate
	.globl __PSInit
__PSInit:
	mflr	r0
	stw		r0,4(sp)
	stwu	sp,-8(sp)
	mfspr	r3,HID2
	oris	r3,r3,0xA000
	mtspr	HID2,r3
	bl		ICFlashInvalidate
	sync
	li		r3,0
	mtspr	GQR0,r3
	lwz		r0,12(sp)
	addi	sp,sp,8
	mtlr	r0
	blr

	.extern ICEnable
	.extern DCEnable
	.extern L2Init
	.extern L2Enable
	.globl __CacheInit
__CacheInit:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -16(sp)
	stw     r31, 12(sp)

	mfspr   r3,HID0 # (HID0)
	rlwinm  r0,r3, 0, 16, 16
	cmplwi  r0, 0x0000 # Check if the Instruction Cache has been enabled or not.
	bne     ICEnabled

	bl		ICEnable
ICEnabled:
	mfspr   r3, HID0 # bl       PPCMfhid0
	rlwinm  r0, r3, 0, 17, 17
	cmplwi  r0, 0x0000 # Check if the Data Cache has been enabled or not.
	bne     DCEnabled
	
	bl		DCEnable	
DCEnabled:
	
	mfspr   r3, L2CR # (L2CR)
	clrrwi  r0, r3, 31 # Clear all of the bits except 31
	cmplwi  r0, 0x0000
	bne     L2Enabled

	bl		L2Init
	bl		L2Enable

L2Enabled:
	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 20(sp)
	lwz     r31, 12(sp)
	addi    sp, sp, 16
	mtlr    r0
	blr

	.globl __SystemInit
__SystemInit:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -24(sp)
	stw     r31, 20(sp)
	stw     r30, 16(sp)
	stw     r29, 12(sp)

	# Disable interrupts!
	mfmsr   r3
	rlwinm  r4,r3,0,17,15
	rlwinm  r4,r4,0,26,24
	mtmsr   r4

	# Clear various SPR's
	li      r3,0
	mtspr   952, r3
	mtspr   956, r3
	mtspr   953, r3
	mtspr   954, r3
	mtspr   957, r3
	mtspr   958, r3

#if defined(HW_RVL)
	lis		r3,0x8390		//bits set: H4A(HID4 access), SBE(2nd BAT enabled),  SR0(store 0), LPE(PS LE exception), L2CFI(L2 castout prior to L2 inv. flash)
	mtspr	HID4,r3
#endif

	# Disable Speculative Bus Accesses to non-guarded space from both caches.
	mfspr   r3, HID0
	ori     r3, r3, 0x0200
	mtspr   HID0, r3

	# Set the Non-IEEE mode in the FPSCR
	mtfsb1  29
	
	mfspr   r3,HID2 # (HID2)
	rlwinm  r3, r3, 0, 2, 0
	mtspr   HID2,r3 # (HID2)

	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 28(sp)
	lwz     r31,20(sp)
	lwz     r30,16(sp)
	lwz     r29,12(sp)
	addi    sp, sp, 24
	mtlr    r0
	blr

	.globl __flush_cache
__flush_cache:
	lis		r5,0xffff
	ori		r5,r5,0xfff1
	and		r5,r5,r3
	subf	r3,r5,r3
	add		r4,r4,r3
1:	dcbst	r0,r5
	sync
	icbi	r0,r5
	addic	r5,r5,8
	subic.	r4,r4,8
	bge		1b
	isync
	blr

	.globl __reset
__reset:
	b		1f
9:	mfspr	r8,HID0
	ori		r8,r8,0x0008
	mtspr	HID0,r8
	isync
	sync
	nop
	b		2f
1:	b		3f
2:	mftb	r5
4:	mftb	r6
	subf	r7,r5,r6
	cmplwi	r7,0x1124
	blt		4b
	nop
	b		5f
3:	b		6f
5:	lis		r8,0xCC00
	ori		r8,r8,0x3000
	li		r4,3
	stw		r4,36(r8)
	stw		r3,36(r8)
	nop
	b		7f
6:	b		8f
7:	nop
	b		7b
8:	b		9b	

	.globl SYS_SwitchFiber
SYS_SwitchFiber:
	mflr	r0
	mr		r9,sp
	stwu	r9,-8(r8)
	mr		sp,r8
	stw		r0,4(r9)
	mtlr	r7
	blrl
	lwz		r5,0(sp)
	lwz		r0,4(r5)
	mtlr	r0
	mr		sp,r5
	blr

	.globl InitBATS
InitBATS:
	mflr	r31
	lis		r3,__configBATS@h
	ori		r3,r3,__configBATS@l
	bl		__realmode
	mtlr	r31
	blr
	
