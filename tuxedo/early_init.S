#include <tuxedo/asm.inc>
#include <tuxedo/ppc/spr.h>

FUNC_START PPCEarlyInit
	// Ensure the return address is virtual
	mflr   r3
	oris   r3, r3, 0x8000
	mtlr   r3

	// Prepare address of initialization function
	lis    r3, (PPCEarlyInitImpl-0x80000000)@h
	ori    r3, r3, (PPCEarlyInitImpl-0x80000000)@l
	mtsrr0 r3

	// Prepare real mode MSR
	li     r3, MSR_FP|MSR_ME|MSR_RI
	mtsrr1 r3

	// Jump to real mode!
	rfi
FUNC_END

FUNC_START PPCEarlyInitImpl, text, local

	// Clear r0
	li    r0, 0

	// Initialize HID0 (and disable L1 caches)
	lis   r3, (HID0_DPM|HID0_NHR)>>16
	ori   r3, r3, HID0_ICFI|HID0_DCFI|HID0_SPD|HID0_DCFA|HID0_BTIC|HID0_BHT
	mtspr HID0, r3
	isync

	// Disable the L2 cache
	sync
	mtspr L2CR, r0
	sync

	// Initialize HID2
	lis   r3, (HID2_LSQE|HID2_PSE)>>16
	mtspr HID2, r3
	isync

#ifdef __wii__
	// Initialize HID4
	lis   r3, (HID4_H4A|HID4_SBE|HID4_ST0|HID4_LPE|HID4_L2CFI)>>16
	mtspr HID4, r3
	isync
#endif

	// Start invalidating the L2 cache
	lis   r3, L2CR_L2CE>>16
	mtspr L2CR, r3

	// Clear all BATs
	mtspr IBAT0U, r0; mtspr IBAT1U, r0; mtspr IBAT2U, r0; mtspr IBAT3U, r0
	mtspr DBAT0U, r0; mtspr DBAT1U, r0; mtspr DBAT2U, r0; mtspr DBAT3U, r0
#ifdef __wii__
	mtspr IBAT4U, r0; mtspr IBAT5U, r0; mtspr IBAT6U, r0; mtspr IBAT7U, r0
	mtspr DBAT4U, r0; mtspr DBAT5U, r0; mtspr DBAT6U, r0; mtspr DBAT7U, r0
#endif

	// Configure all SRs to be direct-store segments:
	// This effectively disables the segments, as the Gecko/Broadway do not
	// support direct-store & instead generate a DSI upon access. Note that
	// cache operations are treated as NOP instead.
	lis   r3, 0x8000
	.rept 16
	mtsr  \+, r3
	.endr

	// Set [DI]BAT0 for 256MB@80000000, real 00000000, WIMG=0000, R/W
	li    r3, 2
	lis   r4, 0x8000
	ori   r4, r4, 0x1fff
	mtspr IBAT0L, r3
	mtspr IBAT0U, r4
	mtspr DBAT0L, r3
	mtspr DBAT0U, r4

#ifdef __wii__
	// Set [DI]BAT4 for 256MB@90000000, real 10000000, WIMG=0000, R/W
	addis r3, r3, 0x1000
	addis r4, r4, 0x1000
	mtspr IBAT4L, r3
	mtspr IBAT4U, r4
	mtspr DBAT4L, r3
	mtspr DBAT4U, r4
#endif

	// Set DBAT1 for 256MB@c0000000, real 00000000, WIMG=0101, R/W
	li    r3, 0x2a
	lis   r4, 0xc000
	ori   r4, r4, 0x1fff
	mtspr DBAT1L, r3
	mtspr DBAT1U, r4

#ifdef __wii__
	// Set DBAT5 for 256MB@d0000000, real 10000000, WIMG=0101, R/W
	addis r3, r3, 0x1000
	addis r4, r4, 0x1000
	mtspr DBAT5L, r3
	mtspr DBAT5U, r4
#endif

	// Clear all GQRs
	mtspr GQR0, r0; mtspr GQR1, r0; mtspr GQR2, r0; mtspr GQR3, r0
	mtspr GQR4, r0; mtspr GQR5, r0; mtspr GQR6, r0; mtspr GQR7, r0

	// Clear performance monitor SPRs
	mtspr MMCR0, r0; mtspr MMCR1, r0
	mtspr PMC1,  r0; mtspr PMC2,  r0
	mtspr PMC3,  r0; mtspr PMC4,  r0

	// Wait for L2 to finish invalidating, and enable L2
1:	mfspr r3, L2CR
	andi. r3, r3, L2CR_L2IP
	bne   1b
	lis   r3, L2CR_L2E>>16
	mtspr L2CR, r3

	// Reenable L1 caches
	mfspr r3, HID0
	ori   r3, r3, HID0_ICE|HID0_DCE
	mtspr HID0, r3
	isync

	// Initialize FPSCR (enabling non-IEEE mode)
	// Sadness: There is no way to load a constant into a FPR without going through memory
	stw    r0, __ppc_excpt_buf@l+0(0)
	stw    r0, __ppc_excpt_buf@l+4(0)
	lfd    f0, __ppc_excpt_buf@l(0)
	mtfsf  0xff, f0
	mtfsb1 29 // NI

	mflr   r3
	mtsrr0 r3

	mfmsr  r3
	ori    r3, r3, MSR_DR|MSR_IR
	mtsrr1 r3

	rfi

FUNC_END

FUNC_START PPCHotReset, text, global, 0, 5
	// Load this entire function into L1 ICache using a jump sled
	b      1f

	// Enable address broadcast (outer-shared domain)
9:	mfspr  r8, HID0
	ori    r8, r8, HID0_ABE
	mtspr  HID0, r8
	isync
	sync

	//     (jump sled cont.)
	nop
	b      2f
1:	b      3f

	// Perform a delay of ~108us
2:	mftb   r5
4:	mftb   r6
	subf   r7, r5, r6
	cmplwi r7, 0x1124
	blt    4b

	//     (jump sled cont.)
	nop
	b      5f
3:	b      6f

	// Write to PI reset register
5:	lis    r8, 0xcc00
	ori    r8, r8, 0x3000
	li     r4, 3
	stw    r4, 0x24(r8)
	stw    r3, 0x24(r8)

	//     (jump sled cont.)
	nop
	b      7f
6:	b      8f

	// Infinite loop
7:	nop
	b      7b

	//     (jump sled cont.)
8:	b      9b
FUNC_END
