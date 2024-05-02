#include <asm.h>

/* We can use up to 32 registers, but registers starting from fr14 need to be
 * saved and restored at the end on the function call, so we try to avoid them.
 * We try to optimize the registers usage in the following way:
 *
 * The Dxx_Dxx registers are used to accumulate the value of the resulting
 * matrix. We compute them in row-major order, after which the register is
 * stored to the destination variable and can be reused. That's why we just
 * need to keep two D registers per row.
 *
 * The Axx_Axx values can also be disposed after a row has been computed, so
 * two registers per row could also be enough. However, to prevent data hazard
 * while processing row N we do preload the two A registers used in row N+1.
 * So, during the processing of a row, we can have up to four active A
 * registers.
 *
 * The only values that are needed throught the computation are the
 * elements of the B matrix, so to avoid unnnecessary re-loading we keep
 * them in registers all the time. We therefore don't reuse B registers. */

#define A00_A01		fr10
#define A02_A03		fr11
#define A10_A11		fr12
#define A12_A13		fr13
#define A20_A21		fr10
#define A22_A23		fr11
#define A30_A31     fr12
#define A32_A33     fr13

#define B00_B01		fr0
#define B02_B03		fr1
#define B10_B11		fr2
#define B12_B13		fr3
#define B20_B21		fr4
#define B22_B23		fr5
#define B30_B31     fr6
#define B32_B33     fr7

#define D00_D01		fr8
#define D02_D03		fr9
#define D10_D11		fr8
#define D12_D13		fr9
#define D20_D21		fr8
#define D22_D23		fr9
#define D30_D31		fr8
#define D32_D33		fr9

	.globl	ps_guMtx44Concat
	//r3 = mtxA, r4 = mtxB, r5 = mtxAB
ps_guMtx44Concat:
	/* First row. This block is longer than the others below due to the fact
	 * that we must also load all the B matrix into registers.
	 * The code is less readable than what it could be because we intertwine
	 * the instructions in order to avoid data hazards.
	 */
	psq_l		A00_A01,0(r3),0,0
	psq_l		B00_B01,0(r4),0,0
	psq_l		B02_B03,8(r4),0,0
	psq_l		B10_B11,16(r4),0,0
	ps_muls0	D00_D01,B00_B01,A00_A01
	psq_l		A02_A03,8(r3),0,0
	ps_muls0	D02_D03,B02_B03,A00_A01
	psq_l		B12_B13,24(r4),0,0
	ps_madds1	D00_D01,B10_B11,A00_A01,D00_D01
	psq_l		B20_B21,32(r4),0,0
	ps_madds1	D02_D03,B12_B13,A00_A01,D02_D03
	psq_l		B22_B23,40(r4),0,0
	ps_madds0	D00_D01,B20_B21,A02_A03,D00_D01
	psq_l		B30_B31,48(r4),0,0
	ps_madds0	D02_D03,B22_B23,A02_A03,D02_D03
	psq_l		B32_B33,56(r4),0,0
	ps_madds1	D00_D01,B30_B31,A02_A03,D00_D01
	psq_l		A10_A11,16(r3),0,0
	ps_madds1	D02_D03,B32_B33,A02_A03,D02_D03
	psq_st		D00_D01,0(r5),0,0
	psq_l		A12_A13,24(r3),0,0
	psq_st		D02_D03,8(r5),0,0

    // Second row
	ps_muls0	D10_D11,B00_B01,A10_A11
	ps_muls0	D12_D13,B02_B03,A10_A11
	ps_madds0	D10_D11,B20_B21,A12_A13,D10_D11
	ps_madds0	D12_D13,B22_B23,A12_A13,D12_D13
	ps_madds1	D10_D11,B10_B11,A10_A11,D10_D11
	ps_madds1	D12_D13,B12_B13,A10_A11,D12_D13
	psq_l		A20_A21,32(r3),0,0
	ps_madds1	D10_D11,B30_B31,A12_A13,D10_D11
	psq_l		A22_A23,40(r3),0,0
	ps_madds1	D12_D13,B32_B33,A12_A13,D12_D13
	psq_st		D10_D11,16(r5),0,0
	psq_st		D12_D13,24(r5),0,0

    // Third row
	ps_muls0	D20_D21,B00_B01,A20_A21
	ps_muls0	D22_D23,B02_B03,A20_A21
	ps_madds0	D20_D21,B20_B21,A22_A23,D20_D21
	ps_madds0	D22_D23,B22_B23,A22_A23,D22_D23
	ps_madds1	D20_D21,B10_B11,A20_A21,D20_D21
	ps_madds1	D22_D23,B12_B13,A20_A21,D22_D23
	psq_l		A30_A31,48(r3),0,0
	ps_madds1	D20_D21,B30_B31,A22_A23,D20_D21
	psq_l		A32_A33,56(r3),0,0
	ps_madds1	D22_D23,B32_B33,A22_A23,D22_D23
	psq_st		D20_D21,32(r5),0,0
	psq_st		D22_D23,40(r5),0,0

    // Fourth row
	ps_muls0	D30_D31,B00_B01,A30_A31
	ps_muls0	D32_D33,B02_B03,A30_A31
	ps_madds0	D30_D31,B20_B21,A32_A33,D30_D31
	ps_madds0	D32_D33,B22_B23,A32_A33,D32_D33
	ps_madds1	D30_D31,B10_B11,A30_A31,D30_D31
	ps_madds1	D32_D33,B12_B13,A30_A31,D32_D33
	ps_madds1	D30_D31,B30_B31,A32_A33,D30_D31
	ps_madds1	D32_D33,B32_B33,A32_A33,D32_D33
	psq_st		D30_D31,48(r5),0,0
	psq_st		D32_D33,56(r5),0,0

	blr

	.section .sdata
	.balign 16
