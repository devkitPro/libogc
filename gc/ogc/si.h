#ifndef __SI_H__
#define __SI_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*SICallback)(s32,u32);
typedef void (*RDSTHandler)(u32,void*);

u32 SI_Busy();
u32 SI_IsChanBusy(s32 chan);
void SI_EnablePolling(u32 poll);
void SI_DisablePolling(u32 poll);
void SI_SetCommand(s32 chan,u32 cmd);
u32 SI_GetStatus(s32 chan);
u32 SI_GetResponse(s32 chan,void *buf);
u32 SI_GetResponseRaw(s32 chan);
void SI_RefreshSamplingRate();
u32 SI_Transfer(s32 chan,void *out,u32 out_len,void *in,u32 in_len,SICallback cb,u32 delay);
u32 SI_GetTypeAsync(s32 chan,SICallback cb);
u32 SI_GetType(s32 chan);
u32 SI_GetCommand(s32 chan);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
