#include <asm.h>

	.globl VIDEO_ClearFrameBuffer
	//r3 = dst, r4 = length(bytes),r5 = color
VIDEO_ClearFrameBuffer:
	addi	r0,r4,0
	srwi.	r0,r0,8
	beq		2f
	mtctr	r0
	subi	r3,r3,4
1:	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)

	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	bdnz	1b
2:	blr

	.globl __video_clearif
__video_clearif:
	li		r3,0
	lis		r8,0xCC00
	lhz		r9,0x2030(r8)
	rlwinm. r11,r9,0,16,16
	beq		1f
	rlwinm	r9,r9,0,17,15
	sth		r9,0x2030(r8)
	ori		r3,r3,1
1:	lhz		r9,0x2034(r8)
	rlwinm. r11,r9,0,16,16
	beq		2f
	rlwinm	r9,r9,0,17,15
	sth		r9,0x2034(r8)
	ori		r3,r3,2
2:	lhz		r9,0x2038(r8)
	rlwinm. r11,r9,0,16,16
	beq		3f
	rlwinm	r9,r9,0,17,15
	sth		r9,0x2038(r8)
	ori		r3,r3,4
3:	lhz		r9,0x203C(r8)
	rlwinm. r11,r9,0,16,16
	beqlr
	rlwinm	r9,r9,0,17,15
	sth		r9,0x203C(r8)
	ori		r3,r3,8
	blr
