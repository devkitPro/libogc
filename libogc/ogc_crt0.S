#include <asm.h>

	.text
	.section .init

	# crt0.s file for the GameCube V1.1 by Costis (costis@gbaemu.com)!
	#
	# Updates: Added support for clearing the BSS section so that global
	#	     variables are cleared to 0 upon start-up. 
	#
	# This is start-up code for initializing the GameCube system and hardware
	# before executing the actual user program code. It clears the GPR's,
	# initializes the FPR's, initializes the Data, Code, and L2 caches, clears
	# and initializes SPR's, and disables exceptions (interrupts).
	#
	# Have fun!!! Please e-mail any suggestions or bugs to costis@gbaemu.com.

	# Entry Point

	.globl _start
_start:
	bl		InitGPRS # Initialize the General Purpose Registers
	bl		PSInit 	# Initialize Paired Singles
	bl      FPRInit 	# Initialize the FPR's
	bl      CacheInit 	# Initialize the system caches
	bl      SystemInit # Initialize more cache aspects, clear a few SPR's, and disable interrupts.

	.extern __sbss_start, __bss_end
	# Clear the BSS section!
	lis		r3,__sbss_start@h
	ori		r3,r3,__sbss_start@l
	li		r4,0
	lis		r5,__bss_end@h
	ori		r5,r5,__bss_end@l
	sub		r5,r5,r3
	bl		memset

	//do a cache flush of whole main ram
	lis		r3,0x8000
	lis		r4,0x017f
	ori		r4,r4,0xffff
	bl		__flush_cache

	lis		r0,0
	stw		r0,0(sp)
	stwu	sp,-64(sp)

	bl		__init
	bl      SYS_Init # Branch to the user code!
	bl		__fini

eloop:
	bl __sdloader_boot
	b eloop			# If the main function returns, which should never happen then just loop endlessly.

InitGPRS:
	# Clear all of the GPR's to 0
	li      r0,0
	li      r3,0
	li      r4,0
	li      r5,0
	li      r6,0
	li      r7,0
	li      r8,0
	li      r9,0
	li      r10,0
	li      r11,0
	li      r12,0
	li      r14,0
	li      r15,0
	li      r16,0
	li      r17,0
	li      r18,0
	li      r19,0
	li      r20,0
	li      r21,0
	li      r22,0
	li      r23,0
	li      r24,0
	li      r25,0
	li      r26,0
	li      r27,0
	li      r28,0
	li      r29,0
	li      r30,0
	li      r31,0

	lis		sp,__stack_addr@h		    # we take 0x8173FFF0 as the topmost starting point for our stack,this gives us ~128Kb Stack
	ori		sp,sp,__stack_addr@l

	lis     r2,_SDA2_BASE_@h
	ori     r2,r2,_SDA2_BASE_@l  # Set the Small Data 2 (Read Only) base register.
	lis     r13,_SDA_BASE_@h
	ori     r13,r13,_SDA_BASE_@l # Set the Small Data (Read\Write) base register.	
	blr

PSInit:
	mfspr   r3,920 # (HID2)
	oris    r3,r3, 0xA000
	mtspr   920,r3 # (HID2)

	# Set the Instruction Cache invalidation bit in HID0
	mfspr   r3,1008
	ori     r3,r3,0x0800
	mtspr   1008,r3

	sync

	# Clear various Special Purpose Registers
	li      r3,0
	mtspr   912,r3
	mtspr   913,r3
	mtspr   914,r3
	mtspr   915,r3
	mtspr   916,r3
	mtspr   917,r3
	mtspr   918,r3
	mtspr   919,r3

	# Return 
	blr

FPRInit:
	# Enable the Floating Point Registers
	mfmsr   r3
	ori     r3,r3,0x2000
	mtmsr   r3
	
	# Clear all of the FPR's to 0
	lis	    r3,zfloat@ha
	lfd	    fr0,zfloat@l(r3)
	fmr     fr1,fr0
	fmr     fr2,fr0
	fmr     fr3,fr0
	fmr     fr4,fr0
	fmr     fr5,fr0
	fmr     fr6,fr0
	fmr     fr7,fr0
	fmr     fr8,fr0
	fmr     fr9,fr0
	fmr     fr10,fr0
	fmr     fr11,fr0
	fmr     fr12,fr0
	fmr     fr13,fr0
	fmr     fr14,fr0
	fmr     fr15,fr0
	fmr     fr16,fr0
	fmr     fr17,fr0
	fmr     fr18,fr0
	fmr     fr19,fr0
	fmr     fr20,fr0
	fmr     fr21,fr0
	fmr     fr22,fr0
	fmr     fr23,fr0
	fmr     fr24,fr0
	fmr     fr25,fr0
	fmr     fr26,fr0
	fmr     fr27,fr0
	fmr     fr28,fr0
	fmr     fr29,fr0
	fmr     fr30,fr0
	fmr     fr31,fr0
	mtfsf   255,fr0

	# Return
	blr

CacheInit:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -16(sp)
	stw     r31, 12(sp)
	stw     r30, 8(sp)

	mfspr   r3,1008 # (HID0)
	rlwinm  r0,r3, 0, 16, 16
	cmplwi  r0, 0x0000 # Check if the Instruction Cache has been enabled or not.
	bne     ICEnabled

	# If not, then enable it.
	isync
	mfspr   r3, 1008
	ori     r3, r3, 0x8000
	mtspr   1008, r3

ICEnabled:
	mfspr   r3, 1008 # bl       PPCMfhid0
	rlwinm  r0, r3, 0, 17, 17
	cmplwi  r0, 0x0000 # Check if the Data Cache has been enabled or not.
	bne     DCEnabled
	
	# If not, then enable it.
	sync
	mfspr   r3, 1008
	ori     r3, r3, 0x4000
	mtspr   1008, r3
	
DCEnabled:
	
	mfspr   r3, 1017 # (L2CR)
	clrrwi  r0, r3, 31 # Clear all of the bits except 31
	cmplwi  r0, 0x0000
	bne     L2GISkip # Skip the L2 Global Cache Invalidation process if it has already been done befor.

	# Store the current state of the MSR in r30
	mfmsr   r3
	mr      r30,r3
	
	sync
	
	# Enable Instruction and Data Address Translation
	li      r3, 48
	mtmsr   r3

	sync
	sync

	# Disable the L2 Global Cache.
	mfspr   r3, 1017 # (L2CR
	clrlwi  r3, r3, 1
	mtspr   1017, r3 # (L2CR)
	sync

	# Invalidate the L2 Global Cache.
	bl      L2GlobalInvalidate

	# Restore the previous state of the MSR from r30
	mr      r3, r30
	mtmsr   r3

	# Enable the L2 Global Cache and disable the L2 Data Only bit and the L2 Global Invalidate Bit.
	mfspr   r3, 1017 # (L2CR)
	oris    r0, r3, 0x8000
	rlwinm  r3, r0, 0, 11, 9
	mtspr   1017, r3 # (L2CR)


L2GISkip:
	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 20(sp)
	lwz     r31, 12(sp)
	lwz     r30, 8(sp)
	addi    sp, sp, 16
	mtlr    r0
	blr

L2GlobalInvalidate:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -16(sp)
	stw     r31, 12(sp)
	sync

	# Disable the L2 Cache.
	mfspr   r3, 1017  # bl       PPCMf1017
	clrlwi  r3, r3, 1
	mtspr   1017, r3 # bl       PPCMt1017

	sync

	# Initiate the L2 Cache Global Invalidation process.
	mfspr   r3, 1017  # (L2CR)
	oris    r3, r3, 0x0020
	mtspr   1017, r3 # (L2CR)

	# Wait until the L2 Cache Global Invalidation has been completed.
L2GICheckComplete:
	mfspr   r3, 1017 # (L2CR)
	clrlwi  r0, r3, 31
	cmplwi  r0, 0x0000
	bne     L2GICheckComplete
	
	# Clear the L2 Data Only bit and the L2 Global Invalidate Bit.
	mfspr   r3, 1017  # (L2CR)
	rlwinm  r3, r3, 0, 11, 9
	mtspr   1017, r3 # (L2CR)

	# Wait until the L2 Cache Global Invalidation status bit signifies that it is ready.
L2GDICheckComplete:
	mfspr   r3, 1017  # (L2CR)
	clrlwi  r0, r3, 31
	cmplwi  r0, 0x0000
	bne     L2GDICheckComplete

	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 20(sp)
	lwz     r31, 12(sp)
	addi    sp, sp, 16
	mtlr    r0
	blr

SystemInit:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -24(sp)
	stw     r31, 20(sp)
	stw     r30, 16(sp)
	stw     r29, 12(sp)

	# Disable interrupts!
	mfmsr   r3
	rlwinm  r4,r3,0,17,15
	mtmsr   r4

	# Clear various SPR's
	li      r3,0
	mtspr   952, r3
	mtspr   956, r3
	mtspr   953, r3
	mtspr   954, r3
	mtspr   957, r3
	mtspr   958, r3

	# Disable Speculative Bus Accesses to non-guarded space from both caches.
	mfspr   r3, 1008 # (HID0)
	ori     r3, r3, 0x0200
	mtspr   1008, r3

	# Set the Non-IEEE mode in the FPSCR
	mtfsb1  29
	
	mfspr   r3,920 # (HID2)
	rlwinm  r3, r3, 0, 2, 0
	mtspr   920,r3 # (HID2)

	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 28(sp)
	lwz     r31,20(sp)
	lwz     r30,16(sp)
	lwz     r29,12(sp)
	addi    sp, sp, 24
	mtlr    r0
	blr

__flush_cache:
	lis		r5, 0xFFFF
	ori		r5, r5, 0xFFF1
	and		r5, r5, r3
	subf	r3, r5, r3
	add		r4, r4, r3
1:
	dcbst	r0, r5
	sync
	icbi	r0, r5
	addic	r5, r5, 8
	subic.	r4, r4, 8
	bge		1b
	isync
	blr

	.section .data
	.balign	8
zfloat:
	.double 0.0
