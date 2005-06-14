#ifndef __EXI_H__
#define __EXI_H__

#include <gctypes.h>

#define EXI_READ					0
#define EXI_WRITE					1
#define EXI_READWRITE				2

#define EXI_CHANNEL_0				0
#define EXI_CHANNEL_1				1
#define EXI_CHANNEL_2				2
#define EXI_CHANNEL_MAX				3

#define EXI_DEVICE_0				0
#define EXI_DEVICE_1				1
#define EXI_DEVICE_2				2
#define EXI_DEVICE_MAX				3

#define EXI_EVENT_EXI				0
#define EXI_EVENT_EXT				1
#define EXI_EVENT_TC				2

#define EXI_SPEED1MHZ				0
#define EXI_SPEED2MHZ				1
#define EXI_SPEED4MHZ				2
#define EXI_SPEED8MHZ				3
#define EXI_SPEED16MHZ				4
#define EXI_SPEED32MHZ				5

#define EXI_FLAG_DMA				0x0001
#define EXI_FLAG_IMM				0x0002
#define EXI_FLAG_SELECT				0x0004
#define EXI_FLAG_ATTACH				0x0008
#define EXI_FLAG_LOCKED				0x0010

#define EXI_MEMCARD59				0x00000004
#define EXI_MEMCARD123				0x00000008
#define EXI_MEMCARD251				0x00000010
#define EXI_MEMCARD507				0x00000020
#define EXI_MEMCARD1019				0x00000040
#define EXI_MEMCARD2043				0x00000080

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef u32 (*EXICallback)(u32,u32);

u32 EXI_ProbeEx(u32 nChn);
u32 EXI_Probe(u32 nChn);
u32 EXI_Lock(u32 nChn,u32 nDev,EXICallback unlockCB);
u32 EXI_Unlock(u32 nChn);
u32 EXI_Select(u32 nChn,u32 nDev,u32 nFrq);
u32 EXI_SelectSD(u32 nChn,u32 nDev,u32 nFrq);
u32 EXI_Deselect(u32 nChn);
u32 EXI_Sync(u32 nChn);
u32 EXI_Imm(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb);
u32 EXI_ImmEx(u32 nChn,void *pData,u32 nLen,u32 nMode);
u32 EXI_Dma(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb);
u32 EXI_GetState(u32 nChn);
u32 EXI_GetID(u32 nChn,u32 nDev,u32 *nId);
u32 EXI_Attach(u32 nChn,EXICallback ext_cb);
u32 EXI_Detach(u32 nChn);
void EXI_ProbeReset();
EXICallback EXI_RegisterEXICallback(u32 nChn,EXICallback exi_cb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
