#include <stdlib.h>

#include "asm.h"
#include "processor.h"
#include "arqueue.h"

static u32 __ARQChunkSize;
static u32 __ARQInitFlag = 0;

static ARQRequest *__ARQReqQueueLo;
static ARQRequest *__ARQReqQueueHi;
static ARQRequest *__ARQReqQueueTemp;
static ARQRequest *__ARQReqTailLo;
static ARQRequest *__ARQReqTailHi;
static ARQRequest *__ARQReqTailTemp;
static u32 __ARQReqPendingLo;
static u32 __ARQReqPendingHi;

static ARQCallback __ARQCallbackLo = NULL;
static ARQCallback __ARQCallbackHi = NULL;

static __inline__ void __ARQPopTaskQueueHi()
{
	if(!__ARQReqQueueHi) return;
	if(__ARQReqQueueHi->type==ARQ_MRAMTOARAM) {
		AR_StartDMA(__ARQReqQueueHi->type,__ARQReqQueueHi->src,__ARQReqQueueHi->dest,__ARQReqQueueHi->len);
	} else {
		AR_StartDMA(__ARQReqQueueHi->type,__ARQReqQueueHi->dest,__ARQReqQueueHi->src,__ARQReqQueueHi->len);
	}
	__ARQCallbackHi = __ARQReqQueueHi->callback;
	__ARQReqPendingHi = (u32)__ARQReqQueueHi;
	__ARQReqQueueHi = __ARQReqQueueHi->next;
}

static void __ARQCallbackHack(u32 request)
{
}

static void __ARQServiceQueueLo()
{
	if(!__ARQReqPendingLo && __ARQReqQueueLo) {
		__ARQReqPendingLo = (u32)__ARQReqQueueLo;
		__ARQReqQueueLo = __ARQReqQueueLo->next;
	}
	
	if(__ARQReqPendingLo) {
		if(((ARQRequest*)__ARQReqPendingLo)->len<=__ARQChunkSize) {
			if(((ARQRequest*)__ARQReqPendingLo)->type==ARQ_MRAMTOARAM) {
				AR_StartDMA(((ARQRequest*)__ARQReqPendingLo)->type,((ARQRequest*)__ARQReqPendingLo)->src,((ARQRequest*)__ARQReqPendingLo)->dest,((ARQRequest*)__ARQReqPendingLo)->len);
			} else {
				AR_StartDMA(((ARQRequest*)__ARQReqPendingLo)->type,((ARQRequest*)__ARQReqPendingLo)->dest,((ARQRequest*)__ARQReqPendingLo)->src,((ARQRequest*)__ARQReqPendingLo)->len);
			}
			__ARQCallbackLo = ((ARQRequest*)__ARQReqPendingLo)->callback;
		} else {
			if(((ARQRequest*)__ARQReqPendingLo)->type==ARQ_MRAMTOARAM) {
				AR_StartDMA(((ARQRequest*)__ARQReqPendingLo)->type,((ARQRequest*)__ARQReqPendingLo)->src,((ARQRequest*)__ARQReqPendingLo)->dest,__ARQChunkSize);
			} else {
				AR_StartDMA(((ARQRequest*)__ARQReqPendingLo)->type,((ARQRequest*)__ARQReqPendingLo)->dest,((ARQRequest*)__ARQReqPendingLo)->src,__ARQChunkSize);
			}
			((ARQRequest*)__ARQReqPendingLo)->len -= __ARQChunkSize;
			((ARQRequest*)__ARQReqPendingLo)->src += __ARQChunkSize;
			((ARQRequest*)__ARQReqPendingLo)->dest += __ARQChunkSize;
		}
	}
}

static void __ARInterruptServiceRoutine()
{
	if(__ARQCallbackHi) {
		__ARQCallbackHi(__ARQReqPendingHi);
		__ARQReqPendingHi = 0;
		__ARQCallbackHi = NULL;
	} else if(__ARQCallbackLo) {
		__ARQCallbackLo(__ARQReqPendingLo);
		__ARQReqPendingLo = 0;
		__ARQCallbackLo = NULL;
	}
	__ARQPopTaskQueueHi();
	if(!__ARQReqPendingHi) __ARQServiceQueueLo();
}

void ARQ_Init()
{
	if(__ARQInitFlag) return;

	__ARQReqQueueLo = NULL;
	__ARQReqQueueHi = NULL;
	
	__ARQChunkSize = 4096;
	AR_RegisterCallback(__ARInterruptServiceRoutine);

	__ARQReqPendingLo = 0;
	__ARQReqPendingHi = 0;

	__ARQCallbackLo = NULL;
	__ARQCallbackHi = NULL;

	__ARQInitFlag = 1;
}

void ARQ_Reset()
{
	__ARQInitFlag = 0;
}

void ARQ_SetChunkSize(u32 size)
{
	if(!(size&0x1f)) __ARQChunkSize = size;
	else __ARQChunkSize = (size+31)&~31;
}

u32 ARQ_GetChunkSize()
{
	return __ARQChunkSize;
}

void ARQ_FlushQueue()
{
	u32 level;

	_CPU_ISR_Disable(level);
	
	__ARQReqQueueHi = NULL;
	__ARQReqTailHi = NULL;
	__ARQReqQueueLo = NULL;
	__ARQReqTailLo = NULL;
	if(!__ARQCallbackLo) __ARQReqPendingLo = 0;

	_CPU_ISR_Restore(level);
}

void ARQ_PostRequest(ARQRequest *req,u32 owner,u32 type,u32 prio,u32 src,u32 dest,u32 len,ARQCallback cb)
{
	u32 level;

	req->next = NULL;
	req->type = type;
	req->owner = owner;
	req->src = src;
	req->dest = dest;
	req->len = len;
	req->prio = prio;
	req->callback = __ARQCallbackHack;
	if(cb) req->callback = cb;
	
	_CPU_ISR_Disable(level);
	if(prio==ARQ_PRIO_LO) { 
		if(__ARQReqQueueLo) __ARQReqTailLo->next = req;
		else __ARQReqQueueLo = req;
		__ARQReqTailLo = req;
	} else {
		if(__ARQReqQueueHi) __ARQReqTailHi->next = req;
		else __ARQReqQueueHi = req;
		__ARQReqTailHi = req;
	}

	if(!__ARQReqPendingLo && !__ARQReqPendingHi) {
		if(__ARQReqQueueHi) {
			if(__ARQReqQueueHi->type==ARQ_MRAMTOARAM) {
				AR_StartDMA(__ARQReqQueueHi->type,__ARQReqQueueHi->src,__ARQReqQueueHi->dest,__ARQReqQueueHi->len);
			} else {
				AR_StartDMA(__ARQReqQueueHi->type,__ARQReqQueueHi->dest,__ARQReqQueueHi->src,__ARQReqQueueHi->len);
			}		
			__ARQCallbackHi = __ARQReqQueueHi->callback;
			__ARQReqPendingHi = (u32)__ARQReqQueueHi;
			__ARQReqQueueHi = __ARQReqQueueHi->next;
		}
		if(!__ARQReqPendingHi) __ARQServiceQueueLo();
	}
	_CPU_ISR_Restore(level);
}

void ARQ_RemoveRequest(ARQRequest *req)
{
	u32 level;
	ARQRequest *p;

	_CPU_ISR_Disable(level);
	__ARQReqQueueTemp = NULL;
	__ARQReqTailTemp = NULL;
	
	p = __ARQReqQueueHi;
	while(p!=NULL) {
		if(p!=req) {
			if(__ARQReqQueueTemp) __ARQReqTailTemp->next = p;
			else __ARQReqQueueTemp = p;
			__ARQReqTailTemp = p;
		}
		p = p->next;
	}

	__ARQReqQueueHi = __ARQReqQueueTemp;
	__ARQReqTailHi = __ARQReqTailTemp;
	
	__ARQReqQueueTemp = NULL;
	__ARQReqTailTemp = NULL;
	p = __ARQReqQueueLo;
	while(p!=NULL) {
		if(p!=req) {
			if(__ARQReqQueueTemp) __ARQReqTailTemp->next = p;
			else __ARQReqQueueTemp = p;
			__ARQReqTailTemp = p;
		}
		p = p->next;
	}

	__ARQReqQueueLo = __ARQReqQueueTemp;
	__ARQReqTailLo = __ARQReqTailTemp;
	
	if(__ARQReqPendingLo && __ARQReqPendingLo==(u32)req && __ARQCallbackLo==NULL) __ARQReqPendingLo = 0;
	_CPU_ISR_Restore(level);
}
