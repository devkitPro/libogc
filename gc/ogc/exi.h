#ifndef __EXI_H__
#define __EXI_H__

#include <gctypes.h>

#define EXI_READ					0
#define EXI_WRITE					1

#define EXI_CHANNEL_0				0
#define EXI_CHANNEL_1				1
#define EXI_CHANNEL_2				2

#define EXI_DEVICE_0				0
#define EXI_DEVICE_1				1
#define EXI_DEVICE_2				2

#define EXI_EVENT_EXI				0
#define EXI_EVENT_EXT				1
#define EXI_EVENT_TC				2

#define EXI_SPEED1MHZ				0
#define EXI_SPEED2MHZ				1
#define EXI_SPEED4MHZ				2
#define EXI_SPEED8MHZ				3
#define EXI_SPEED16MHZ				4
#define EXI_SPEED32MHZ				5

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef u32 (*EXICallback)(u32,void *);

u32 EXI_Probe(u32 nChn);
u32 EXI_Lock(u32 nChn,u32 nDev,EXICallback unlockCB);
u32 EXI_Unlock(u32 nChn);
u32 EXI_Select(u32 nChn,u32 nDev,u32 nFrq);
u32 EXI_Deselect(u32 nChn);
u32 EXI_Sync(u32 nChn);
u32 EXI_Imm(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tccb);
u32 EXI_ImmEx(u32 nChn,void *pData,u32 nLen,u32 nMode);
u32 EXI_Dma(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tccb);
u32 EXI_GetState(u32 nChn);
u32 EXI_GetId(u32 nChn,u32 nDev,u32 *nId);
EXICallback EXI_RegisterEXICallback(u32 nChn,EXICallback cb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
