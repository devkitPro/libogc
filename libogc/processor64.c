#include <stdlib.h>
#include <stdio.h>
#include <gctypes.h>

#include "asm.h"
#include "processor.h"

s64 div2i(s64 dvd,s64 dvs)
{
	__asm__ __volatile__ (
		"	stwu     1,-16(1)\n"
	    // first we need the absolute values of both operands. we also
		// remember the signs so that we can adjust the result later.
		"	rlwinm.  9,3,0,0,0\n"
		"	beq      cr0,positive1\n"
		"	subfic   4,4,0\n"
		"	subfze   3,3\n"
		"positive1:\n"
		"	stw      9,8(1)\n"
		"	rlwinm.  9,5,0,0,0\n"
		"	beq      cr0,positive2\n"
		"	subfic   6,6,0\n"
		"	subfze   5,5\n"
		"positive2:\n"
		"	stw      9,12(1)\n"
			// count the number of leading 0s in the dividend 
		"	cmpwi	 cr0,3,0\n"		// dvd.msw == 0? 
		"	cntlzw	 0,3\n"			// R0 = dvd.msw.LZ 
		"	cntlzw	 9,4\n"			// R9 = dvd.lsw.LZ 
		"	bne		 cr0,lab1\n"	// if(dvd.msw == 0) dvd.LZ = dvd.msw.LZ 
		"	addi	 0,9,32\n"		// dvd.LZ = dvd.lsw.LZ + 32 
		"lab1:\n" 
			// count the number of leading 0s in the divisor 
		"	cmpwi	 cr0,5,0\n"		// dvd.msw == 0? 
		"	cntlzw	 9,5\n"			// R9 = dvs.msw.LZ 
		"	cntlzw	 10,6\n"		// R10 = dvs.lsw.LZ 
		"	bne		 cr0,lab2\n"	// if(dvs.msw == 0) dvs.LZ = dvs.msw.LZ 
		"	addi	 9,10,32\n"		// dvs.LZ = dvs.lsw.LZ + 32 
		"lab2:\n" 
			// determine shift amounts to minimize the number of iterations 
		"	cmpw	 cr0,0,9\n"		// compare dvd.LZ to dvs.LZ 
		"	subfic	 10,0,64\n"		// R10 = dvd.SD 
		"	bgt		 cr0,lab9\n"	// if(dvs > dvd) quotient = 0 
		"	addi	 9,9,1\n"		// ++dvs.LZ (or --dvs.SD) 
		"	subfic	 9,9,64\n"		// R9 = dvs.SD 
		"	add		 0,0,9\n"		// (dvd.LZ + dvs.SD) = left shift of dvd for 
                        			// initial dvd 
		"	subf	 9,9,10\n"		// (dvd.SD - dvs.SD) = right shift of dvd for 
                       			// initial tmp 
		"	mtctr	 9\n"			// number of iterations = dvd.SD - dvs.SD
        	// R7:R8 = R3:R4 >> R9 
		"	cmpwi	 cr0,9,32\n"	// compare R9 to 32 
		"	addi	 7,9,-32\n" 
		"	blt		 cr0,lab3\n"	// if(R9 < 32) jump to lab3 
		"	srw		 8,3,7\n"		// tmp.lsw = dvd.msw >> (R9 - 32) 
		"	li		 7,0\n"			// tmp.msw = 0 
		"	b		 lab4\n"
		"lab3:\n" 
		"	srw		 8,4,9\n"		// R8 = dvd.lsw >> R9 
		"	subfic	 7,9,32\n" 
		"	slw		 7,3,7\n"		// R7 = dvd.msw << 32 - R9 
		"	or		 8,8,7\n"		// tmp.lsw = R8 | R7 
		"	srw		 7,3,9\n"		// tmp.msw = dvd.msw >> R9 
		"lab4:\n" 
			// R3:R4 = R3:R4 << R0 
		"	cmpwi	 cr0,0,32\n"		// compare R0 to 32 
		"	addic	 9,0,-32\n" 
		"	blt		 cr0,lab5\n"		// if(R0 < 32) jump to lab5 
		"	slw		 3,4,9\n"		// dvd.msw = dvd.lsw << R9 
		"	li		 4,0\n"			// dvd.lsw = 0 
		"	b		 lab6\n" 
		"lab5:\n" 
		"	slw		 3,3,0\n"		// R3 = dvd.msw << R0 
		"	subfic	 9,0,32\n" 
		"	srw		 9,4,9\n"		// R9 = dvd.lsw >> 32 - R0 
		"	or		 3,3,9\n"		// dvd.msw = R3 | R9 
		"	slw		 4,4,0\n"		// dvd.lsw = dvd.lsw << R0 
		"lab6:\n" 
			// restoring division shift and subtract loop 
		"	li		 10,-1\n"			// R10 = -1 
		"	addic	 7,7,0\n"			// clear carry bit before loop starts 
		"lab7:\n" 
			// tmp:dvd is considered one large register 
			// each portion is shifted left 1 bit by adding it to itself 
			// adde sums the carry from the previous and creates a new carry 
		"	adde	 4,4,4\n"		// shift dvd.lsw left 1 bit 
		"	adde	 3,3,3\n"		// shift dvd.msw to left 1 bit 
		"	adde	 8,8,8\n"		// shift tmp.lsw to left 1 bit 
		"	adde	 7,7,7\n"		// shift tmp.msw to left 1 bit 
		"	subfc	 0,6,8\n"		// tmp.lsw - dvs.lsw 
		"	subfe.	 9,5,7\n"		// tmp.msw - dvs.msw 
		"	blt		 cr0,lab8\n"		// if(result < 0) clear carry bit 
		"	mr		 8,0\n"			// move lsw 
		"	mr		 7,9\n"			// move msw 
		"	addic	 0,10,1\n"		// set carry bit 
		"lab8:\n" 
		"	bdnz	 lab7\n"
       
			// write quotient 
		"	adde	 4,4,4\n"		// quo.lsw (lsb = CA) 
		"	adde	 3,3,3\n"		// quo.msw (lsb from lsw) 
       
			// now adjust result based on sign of original DVD and DVS
		"	lwz		 9,8(1)\n"
		"	lwz		 10,12(1)\n"
		"	xor.	 7,9,10\n"
		"	beq		 cr0,no_adjust\n"	// sign of DVD and DVS same, no adjust
		"	cmpwi	 cr0,9,0\n"		
			// sign of DVD and DVS opposite, we negate result
		"	subfic   4,4,0\n"
		"	subfze   3,3\n"       
		"no_adjust:\n"
		"	b		 func_end\n" 
 
		"lab9:\n" 
			// Quotient is 0 (dvs > dvd) 
		"	li		 4,0\n"			// dvd.lsw = 0 
		"	li		 3,0\n"			// dvd.msw = 0 
		"func_end:\n" 
		"	addi     1,1,16\n"
		"	blr"						// return 
		: : : "memory");

		return 0;
}

u64 shl2i(u64 val,u32 shift)
{
	__asm__ __volatile__ (
		"subfic		8,5,32\n"
		"subic		9,5,32\n"
		"slw		3,3,5\n"
		"srw		10,4,8\n"
		"or			3,3,10\n"
		"slw		10,4,9\n"
		"or			3,3,10\n"			// high word
		"slw		4,4,5\n"			// low word
		"blr"
		: : : "memory");

	return 0;
}

u64 shr2i(u64 val,u32 shift)
{
	__asm__ __volatile__ (
		"	subfic	8,5,32\n"
		"	subic.	9,5,32\n"
		"	srw		4,4,5\n"
		"	slw		10,3,8\n"
		"	or		4,4,10\n"
		"	sraw	10,3,9\n"
		"	ble		around\n"
		"	or		4,4,10\n"			// low word
		"around:\n"
		"	sraw	3,3,5\n"			// high word
		"	blr"
		: : : "memory");

	return 0;
}

u64 shr2u(u64 val,u32 shift)
{
	__asm__ __volatile__ (
		"subfic		8,5,32\n"
		"subic		9,5,32\n"
		"srw		4,4,5\n"
		"slw		10,3,8\n"
		"or			4,4,10\n"
		"srw		10,3,9\n"
		"or			4,4,10\n"			// low word
		"srw		3,3,5\n"			// high word
		"blr"
		: : : "memory"
	);

	return 0;
}
