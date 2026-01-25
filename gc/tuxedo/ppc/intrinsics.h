// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "spr.h"

MK_EXTERN_C_START

#define PPCDebugPrint(...) __PPCDebugPrintImpl(0, __VA_ARGS__)

typedef u32 PPCIrqState;

MK_INLINE void PPCCompilerBarrier(void)
{
	__asm__ __volatile__("" ::: "memory");
}

MK_INLINE void PPCNop(void)
{
	__asm__ __volatile__("nop" ::: "memory");
}

MK_INLINE void PPCSyncInner(void)
{
	__asm__ __volatile__("sync" ::: "memory");
}

MK_INLINE void PPCIsync(void)
{
	__asm__ __volatile__("isync" ::: "memory");
}

MK_INLINE void PPCEieioInner(void)
{
	__asm__ __volatile__("eieio" ::: "memory");
}

MK_INLINE u32 PPCMftb(void)
{
	u32 ret;
	__asm__ __volatile__("mftb %0" : "=r"(ret));
	return ret;
}

MK_INLINE u32 PPCMftbu(void)
{
	u32 ret;
	__asm__ __volatile__("mftbu %0" : "=r"(ret));
	return ret;
}

MK_INLINE void PPCMtmsr(u32 value)
{
	__asm__ __volatile__("mtmsr %0" :: "r"(value) : "memory");
}

MK_INLINE u32 PPCMfmsr(void)
{
	u32 ret;
	__asm__ __volatile__("mfmsr %0" : "=r"(ret));
	return ret;
}

MK_INLINE void PPCMtspr(unsigned spr, u32 value)
{
	__asm__ __volatile__("mtspr %0, %1" :: "I"(spr), "r"(value) : "memory");
}

MK_INLINE u32 PPCMfspr(unsigned spr)
{
	u32 ret;
	__asm__ __volatile__("mfspr %0, %1" : "=r"(ret) : "I"(spr));
	return ret;
}

MK_INLINE void PPCDcba(uptr addr)
{
	__asm__ __volatile__("dcba 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbf(uptr addr)
{
	__asm__ __volatile__("dcbf 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbi(uptr addr)
{
	__asm__ __volatile__("dcbi 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbst(uptr addr)
{
	__asm__ __volatile__("dcbst 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbt(uptr addr)
{
	__asm__ __volatile__("dcbt 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbtst(uptr addr)
{
	__asm__ __volatile__("dcbtst 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbz(uptr addr)
{
	__asm__ __volatile__("dcbz 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCDcbz_l(uptr addr)
{
	__asm__ __volatile__("dcbz_l 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE void PPCIcbi(uptr addr)
{
	__asm__ __volatile__("icbi 0, %0" :: "r"(addr) : "memory");
}

MK_INLINE PPCIrqState PPCIrqLockByMsr(void)
{
	u32 msr = PPCMfmsr();
	PPCMtmsr(msr &~ (MSR_EE|MSR_FP));
	return msr & MSR_EE;
}

MK_INLINE void PPCIrqUnlockByMsr(PPCIrqState st)
{
	u32 msr = PPCMfmsr() &~ MSR_EE;
	PPCMtmsr(msr | st);
}

MK_INLINE bool PPCIsInExcpt(void)
{
	return (PPCMfmsr() & MSR_RI) == 0;
}

MK_INLINE void PPCSync(void)
{
	PPCIrqState st = PPCIrqLockByMsr();
	u32 hid0 = PPCMfspr(HID0);
	PPCMtspr(HID0, hid0 | HID0_ABE);
	PPCIsync();
	PPCSyncInner();
	PPCMtspr(HID0, hid0);
	PPCIsync();
	PPCIrqUnlockByMsr(st);
}

MK_INLINE u64 PPCGetTickCount(void)
{
	u32 hi, lo, hi2;
	do {
		hi = PPCMftbu();
		lo = PPCMftb();
		hi2 = PPCMftbu();
	} while (hi != hi2);

	return ((u64)hi << 32) | lo;
}

void PPCHotReset(u32 code);

void __PPCDebugPrintImpl(int, const char* fmt, ...) __asm__("DEBUGPrint") __attribute__((format(printf, 2, 3)));

MK_EXTERN_C_END
