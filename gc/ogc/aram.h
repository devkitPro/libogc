#ifndef __ARAM_H__
#define __ARAM_H__

#include <gctypes.h>

#define MRAMTOARAM		0
#define ARAMTOMRAM		1

#define ARAMINTALL		0
#define ARAMINTUSER		1
#define ARAMEXPANSION	2

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*ARCallback)(void);

ARCallback AR_RegisterCallback(ARCallback callback);
u32 AR_GetDMAStatus();
u32 AR_Init(u32 *stack_idx_array,u32 num_entries);
void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len);
u32 AR_Alloc(u32 len);
u32 AR_Free(u32 *len);
void AR_Clear(u32 flag);
BOOL AR_CheckInit();
void AR_Reset();
u32 AR_GetSize();
u32 AR_GetBaseAddress();
u32 AR_GetInternalSize();

#define AR_StartDMARead(maddr,araddr,tlen)	\
	AR_StartDMA(ARAMTOMRAM,maddr,araddr,tlen);
#define AR_StartDMAWrite(maddr,araddr,tlen)	\
	AR_StartDMA(MRAMTOARAM,maddr,araddr,tlen);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif //__ARAM_H__
