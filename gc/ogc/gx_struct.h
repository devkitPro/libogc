#ifndef __GX_STRUCT_H__
#define __GX_STRUCT_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _gx_rmodeobj {
	u32 viTVMode;
	u16 fbWidth;
	u16 efbHeight;
	u16 xfbHeight;
	u16 viXOrigin;	
	u16 viYOrigin;	
	u16 viWidth;
	u16 viHeight;
	u8  xfbMode;
	u8  field_rendering;
	u8  aa;
	u8  sample_pattern[12][2];
	u8  vfilter[7];
} GXRModeObj;

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
