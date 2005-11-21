#ifndef __ARQUEUE_H__
#define __ARQUEUE_H__

#include <gctypes.h>
#include "aram.h"

#define ARQ_MRAMTOARAM			AR_MRAMTOARAM
#define ARQ_ARAMTOMRAM			AR_ARAMTOMRAM

#define ARQ_PRIO_LO				0
#define ARQ_PRIO_HI				1

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*ARQCallback)(u32);

typedef struct _arq_request {
	struct _arq_request *next;
	u32 owner,type,prio,src,dest,len;
	ARQCallback callback;
} ARQRequest;

void ARQ_Init();
void ARQ_Reset();
void ARQ_PostRequest(ARQRequest *req,u32 owner,u32 type,u32 prio,u32 src,u32 dest,u32 len,ARQCallback cb);
void ARQ_RemoveRequest(ARQRequest *req);
void ARQ_SetChunkSize(u32 size);
u32 ARQ_GetChunkSize();
void ARQ_FlushQueue();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
