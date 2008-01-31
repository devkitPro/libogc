#ifndef __CAST_H__
#define __CAST_H__

#include <gctypes.h>

#define	GQR0			912
#define	GQR1			913
#define	GQR2			914
#define	GQR3			915
#define	GQR4			916
#define	GQR5			917
#define	GQR6			918
#define	GQR7			919

#define GQR_TYPEF32		0
#define GQR_TYPEU8		4
#define GQR_TYPEU16		5
#define GQR_TYPES8		6
#define GQR_TYPES16		7

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GEKKO

// set a quantization register independent with type and scale
#define __stringify(rn)					#rn					
#define CAST_SetGQR(_reg,_type,_scale)														\
{																							\
	register u32 _val = ((((_scale)&0x3f)<<24)|((_type)<<16)|(((_scale)&0x3f)<<8)|(_type));	\
	asm volatile (																			\
		"mtspr	"__stringify((_reg)) ",%0\n"												\
		: : "r"(_val)																		\
	);																						\
}

// does a default init
static __inline__ void CAST_Init()
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


/******************************************************************/
/*																  */
/* cast from int to float										  */
/*																  */
/******************************************************************/

#define castu8f32(in,out)			({ asm volatile("psq_l		%0, 0(%1), 1, 2" : "=f"(*(out)) : "b"(in)); })
#define castu16f32(in,out)			({ asm volatile("psq_l		%0, 0(%1), 1, 3" : "=f"(*(out)) : "b"(in)); })
#define casts8f32(in,out)			({ asm volatile("psq_l		%0, 0(%1), 1, 4" : "=f"(*(out)) : "b"(in)); })
#define casts16f32(in,out)			({ asm volatile("psq_l		%0, 0(%1), 1, 5" : "=f"(*(out)) : "b"(in)); })


/******************************************************************/
/*																  */
/* cast from float to int										  */
/*																  */
/******************************************************************/

#define castf32u8(in,out)			({ asm volatile("psq_st		%1, 0(%0), 1, 2" :: "b"(out), "f"(*(in)) : "memory"); })
#define castf32u16(in,out)			({ asm volatile("psq_st		%1, 0(%0), 1, 3" :: "b"(out), "f"(*(in)) : "memory"); })
#define castf32s8(in,out)			({ asm volatile("psq_st		%1, 0(%0), 1, 4" :: "b"(out), "f"(*(in)) : "memory"); })
#define castf32s16(in,out)			({ asm volatile("psq_st		%1, 0(%0), 1, 5" :: "b"(out), "f"(*(in)) : "memory"); })


#endif //GEKKO

#ifdef __cplusplus
}
#endif

#endif
