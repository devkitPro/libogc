// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

#if defined(__gamecube__)
#define PPC_BUS_CLOCK  162000000
#define PPC_CORE_CLOCK 486000000
#elif defined(__wii__)
#define PPC_BUS_CLOCK  243000000
#define PPC_CORE_CLOCK 729000000
#endif

#define PPC_TIMER_CLOCK (PPC_BUS_CLOCK/4)

#ifndef __ASSEMBLER__
#include "../types.h"

MK_EXTERN_C_START

#if __cplusplus >= 201402L

MK_CONSTEXPR unsigned __PPCGcd(unsigned a, unsigned b)
{
	// See https://en.wikipedia.org/wiki/Binary_GCD_algorithm
	if (!a) return b;
	if (!b) return a;

	unsigned i = __builtin_ctz(a); a >>= i;
	unsigned j = __builtin_ctz(b); b >>= j;
	unsigned k = i < j ? i : j;

	for (;;) {
		if (a > b) {
			unsigned t = a;
			a = b;
			b = t;
		}

		b -= a;
		if (!b) return a << k;
		b >>= __builtin_ctz(b);
	}
}

#else

// Sadness: GCC cannot evaluate above at compile time without C++ constexpr
MK_INLINE unsigned __PPCGcd(unsigned a, unsigned b)
{
	if (a > b) return __PPCGcd(b, a);
	switch (a) {
		case 1000: switch (b) {
			case 40500000: return 1000;
			case 60750000: return 1000;
		} break;
		case 1000000: switch (b) {
			case 40500000: return 500000;
			case 60750000: return 250000;
		} break;
		case 40500000: switch (b) {
			case 1000000000: return 500000;
		} break;
		case 60750000: switch (b) {
			case 1000000000: return 250000;
		} break;
	}
	return 0;
}

#endif

MK_CONSTEXPR u64 __PPCMulDiv(u64 value, unsigned num, unsigned den)
{
	unsigned gcd = __PPCGcd(num, den);
	num /= gcd;
	den /= gcd;
	return value*num/den;
}

MK_CONSTEXPR u64 PPCNsToTicks(u64 ns)
{
	return __PPCMulDiv(ns, PPC_TIMER_CLOCK, 1000000000);
}

MK_CONSTEXPR u64 PPCUsToTicks(u64 us)
{
	return __PPCMulDiv(us, PPC_TIMER_CLOCK, 1000000);
}

MK_CONSTEXPR u64 PPCMsToTicks(u64 ms)
{
	return __PPCMulDiv(ms, PPC_TIMER_CLOCK, 1000);
}

MK_CONSTEXPR u64 PPCTicksToNs(u64 ticks)
{
	return __PPCMulDiv(ticks, 1000000000, PPC_TIMER_CLOCK);
}

MK_CONSTEXPR u64 PPCTicksToUs(u64 ticks)
{
	return __PPCMulDiv(ticks, 1000000, PPC_TIMER_CLOCK);
}

MK_CONSTEXPR u64 PPCTicksToMs(u64 ticks)
{
	return __PPCMulDiv(ticks, 1000, PPC_TIMER_CLOCK);
}

MK_EXTERN_C_END

#endif
