#ifndef __CAST_H__
#define __CAST_H__

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GEKKO

static inline void CAST_Init()
{
	__asm__ __volatile__ (
		"li		3,0x0004\n\
		 oris	3,3,0x0004\n\
		 mtspr	914,3\n\
		 li		3,0x0005\n\
		 oris	3,3,0x0005\n\
		 mtspr	915,3\n\
		 li		3,0x0006\n\
		 oris	3,3,0x0006\n\
		 mtspr	916,3\n\
		 li		3,0x0007\n\
		 oris	3,3,0x0007\n\
		 mtspr	917,3\n"
		 : : : "r3"
	);
}

#define castu82f32(in,out)		({asm volatile("psq_l	%0,0(%1),1,2" : "=f"(*(out)) : "b" (in));})
#define castu162f32(in,out)		({asm volatile("psq_l	%0,0(%1),1,3" : "=f"(*(out)) : "b" (in));})
#define casts82f32(in,out)		({asm volatile("psq_l	%0,0(%1),1,4" : "=f"(*(out)) : "b" (in));})
#define casts162f32(in,out)		({asm volatile("psq_l	%0,0(%1),1,5" : "=f"(*(out)) : "b" (in));})

#define castf322u8(in,out)		({asm volatile("psq_st	%1,0(%0),1,2" :: "b"(out),"f" (*(in)) : "memory");})
#define castf322u16(in,out)		({asm volatile("psq_st	%1,0(%0),1,3" :: "b"(out),"f" (*(in)) : "memory");})
#define castf322s8(in,out)		({asm volatile("psq_st	%1,0(%0),1,4" :: "b"(out),"f" (*(in)) : "memory");})
#define castf322s16(in,out)		({asm volatile("psq_st	%1,0(%0),1,5" :: "b"(out),"f" (*(in)) : "memory");})

#endif //GEKKO

#ifdef __cplusplus
}
#endif

#endif
