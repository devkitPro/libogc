#pragma once

.cfi_sections .debug_frame

.macro SECT_TEXT name, align=2, section=text
	.section .\section\().\name, "ax", %progbits
	.align   \align
.endm

.macro SECT_RODATA name, align=2, section=rodata
	.section .\section\().\name, "a"
	.align   \align
.endm

.macro SECT_DATA name, align=2, section=data
	.section .\section\().\name, "aw"
	.align   \align
.endm

.macro SECT_BSS name, align=2, section=bss
	.section .\section\().\name, "aw", %nobits
	.align   \align
.endm

.macro FUNC_START name, section=text, linkage=global, wantsizeheader=0, align=2
	SECT_TEXT \name, \align, \section
	.\linkage \name
	.type     \name, %function
.if \wantsizeheader
	.long     3135f-\name
.endif
\name:
	.cfi_startproc
	.size     \name, 3135f-\name
.endm

.macro FUNC_START_SIZED name, section=text, linkage=global
	FUNC_START \name, \section, \linkage, 1
.endm

.macro FUNC_END
3135:
	.cfi_endproc
.endm

.macro REFERENCE symbol
	.section .rodata.ref.\symbol, "a"
	.align   2
	.word    \symbol
.endm
