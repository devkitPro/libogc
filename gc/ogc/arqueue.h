/*-------------------------------------------------------------

$Id: arqueue.h,v 1.5 2005-11-21 12:41:27 shagkur Exp $

arqueue.h -- ARAM task request queue implementation

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

$Log: not supported by cvs2svn $

-------------------------------------------------------------*/


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
