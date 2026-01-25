// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <tuxedo/asm.inc>
#include <tuxedo/ppc/spr.h>

REFERENCE __newlib_syscalls

FUNC_START __app_start, crt0

	b .Lactual_start

	.ascii "_arg"
.Largv:
	.space 6*4

#ifdef __wii__
	// Emulators rely on finding HID4-related opcodes in code sections in order
	// to detect stray DOLs as Wii applications. Thus, we include here a dummy
	// instruction to facilitate such detection.
	mtspr HID4, 0
#endif

.Lactual_start:

	// Perform basic PPC initialization
	bl   PPCEarlyInit

	// Set up sda/sda2
	lis  r2, _SDA2_BASE_@h
	ori  r2, r2, _SDA2_BASE_@l
	lis  r13, _SDA_BASE_@h
	ori  r13, r13, _SDA_BASE_@l

	// Set up early stack (borrowing the exception stack)
	lwz  sp, __ppc_excpt_sp@sda21(0)
	subi sp, sp, 8

	// Shelter argv before clearing BSS
	bl   __CheckARGV

	// Clear BSS
	lis  r3, __sbss_start@h
	ori  r3, r3, __sbss_start@l
	li   r4, 0
	nand r14, r4, r4 // used later to mark end-of-stack
	lis  r5, __bss_end@h
	ori  r5, r5, __bss_end@l
	sub  r5, r5, r3
	bl   memset

	// Set up exception stack
	stw  r14, 0(sp)
	stw  sp, __ppc_excpt_sp@sda21(0)

	// Set up main thread stack
	lwz  sp, __ppc_main_sp@sda21(0)
	stwu r14, -8(sp)

	// Initialize tuxedo
	bl   PPCExcptInit
	bl   KThreadInit
	bl   KIrqInit

	// Initialize libogc
	bl   SYS_Init
	bl   SYS_PreMain

	// Jump to main
	lwz  r11, __system_argv@sda21(0)
	lwz  r3, 12(r11)
	lwz  r4, 16(r11)

	lis  r7, exit@h
	ori  r7, r7, exit@l
	mtlr r7

	b    main

FUNC_END

FUNC_START DEBUGPrint
	blr
FUNC_END

FUNC_START __eabi
	b __init
FUNC_END

SECT_DATA __system_argv, 2, sdata

.global __system_argv
__system_argv:
	.long .Largv
