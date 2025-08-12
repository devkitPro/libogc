#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include "../../tuxedo/ppc/intrinsics.h"
#include "asm.h"

#define __stringify(rn)								#rn
#define ATTRIBUTE_ALIGN(v)							__attribute__((aligned(v)))
#define STACK_ALIGN(type, name, cnt, alignment)		alignas(alignment) type name[cnt]

#define _sync() PPCSyncInner()
#define _nop() PPCNop()
#define ppcsync() PPCSync()

#define mfpvr() PPCMfspr(PVR)

#define mfdcr(_rn) ({u32 _rval; \
		__asm__ __volatile__ ("mfdcr %0," __stringify(_rn) \
             : "=r" (_rval)); _rval;})
#define mtdcr(rn, val)  __asm__ __volatile__ ("mtdcr " __stringify(rn) ",%0" : : "r" (val))

#define mfmsr   PPCMfmsr
#define mtmsr  PPCMtmsr

#define mfdec()   PPCMfspr(DECR)
#define mtdec(_val)  PPCMtspr(DECR, (_val))

#define mfspr PPCMfspr

#define mtspr PPCMtspr

#define mfwpar()		mfspr(WPAR)
#define mtwpar(_val)	mtspr(WPAR,_val)

#define mfmmcr0()		mfspr(MMCR0)
#define mtmmcr0(_val)	mtspr(MMCR0,_val)
#define mfmmcr1()		mfspr(MMCR1)
#define mtmmcr1(_val)	mtspr(MMCR1,_val)

#define mfpmc1()		mfspr(PMC1)
#define mtpmc1(_val)	mtspr(PMC1,_val)
#define mfpmc2()		mfspr(PMC2)
#define mtpmc2(_val)	mtspr(PMC2,_val)
#define mfpmc3()		mfspr(PMC3)
#define mtpmc3(_val)	mtspr(PMC3,_val)
#define mfpmc4()		mfspr(PMC4)
#define mtpmc4(_val)	mtspr(PMC4,_val)

#define mfhid0()		mfspr(HID0)
#define mthid0(_val)	mtspr(HID0,_val)
#define mfhid1()		mfspr(HID1)
#define mthid1(_val)	mtspr(HID1,_val)
#define mfhid2()		mfspr(HID2)
#define mthid2(_val)	mtspr(HID2,_val)
#define mfhid4()		mfspr(HID4)
#define mthid4(_val)	mtspr(HID4,_val)

static inline u16 __lhbrx(const void* base, unsigned index)
{
	return __builtin_bswap16(*(const u16*)((const char*)base + index));
}

static inline u32 __lwbrx(const void* base, unsigned index)
{
	return __builtin_bswap32(*(const u32*)((const char*)base + index));
}

static inline void __sthbrx(void* base, unsigned index, u16 value)
{
	*(u16*)((char*)base + index) = __builtin_bswap16(value);
}

static inline void __stwbrx(void* base, unsigned index, u32 value)
{
	*(u32*)((char*)base + index) = __builtin_bswap32(value);
}

static inline u32 cntlzw(u32 value)
{
	return value ? __builtin_clz(value) : 32;
}

#define _CPU_ISR_Disable( _isr_cookie ) \
	do { \
		_isr_cookie = PPCIrqLockByMsr(); \
	} while (0)

#define _CPU_ISR_Restore( _isr_cookie ) \
	PPCIrqUnlockByMsr(_isr_cookie)

#define _CPU_ISR_Flash( _isr_cookie ) \
	do { \
		PPCIrqUnlockByMsr(_isr_cookie); \
		PPCIrqLockByMsr(); \
	} while (0)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

static inline u16 bswap16(u16 val)
{
	return __builtin_bswap16(val);
}

static inline u32 bswap32(u32 val)
{
	return __builtin_bswap32(val);
}

static inline u64 bswap64(u64 val)
{
	return __builtin_bswap64(val);
}

static inline u32 read32(u32 addr)
{
	u32 x = *(vu32*)(0xc0000000 | addr);
	PPCSyncInner();
	return x;
}

static inline void write32(u32 addr, u32 x)
{
	*(vu32*)(0xc0000000 | addr) = x;
	PPCEieioInner();
}

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
