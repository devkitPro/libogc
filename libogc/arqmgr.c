#include <stdlib.h>

#include "asm.h"
#include "processor.h"
#include "arqueue.h"
#include "arqmgr.h"

typedef struct _arqm_info {
	ARQRequest arqhandle;
	ARQMCallback callback;
	void *buff;
	u32 len;
	u32 aram_start;
	u32 curr_aram_offset;
	volatile BOOL poll_flag;
} ARQM_Info;

static u32 __ARQMStackLocation;
static u32 __ARQMFreeBytes;
static u32 __ARQMStackPointer[ARQM_STACKENTRIES];
static ARQM_Info __ARQMInfo[ARQM_STACKENTRIES];
static u32 __ARQMZeroBuffer[ARQM_ZEROBYTES/sizeof(u32)] ATTRIBUTE_ALIGN(32);

static void __ARQMPollCallback(u32 task)
{
	u32 i;
	ARQM_Info *ptr = NULL;
	ARQMCallback tccb;

	for(i=0;i<ARQM_STACKENTRIES;i++) {
		if((ARQRequest*)task==&__ARQMInfo[i].arqhandle) {
			ptr = &__ARQMInfo[i];
			break;
		}
	}
	if(i>=ARQM_STACKENTRIES) return;

	tccb = ptr->callback;
	ptr->callback = NULL;
	ptr->poll_flag = TRUE;
	if(tccb) tccb();
}

void ARQM_Init(u32 arambase,u32 len)
{
	u32 i;
	
	if((s32)len<=0) return;

	__ARQMStackLocation = 0;
	__ARQMStackPointer[0] = arambase;
	__ARQMFreeBytes = len;
	
	for(i=0;i<ARQM_ZEROBYTES/sizeof(u32);i++) __ARQMZeroBuffer[i] = 0;
}

u32 ARQM_PushData(void *buff,u32 len,ARQMCallback tccb)
{
	u32 rlen,level;
	ARQM_Info *ptr;

	if(!(((u32)buff)&0x1f) || (s32)len<=0) return -1;

	rlen = (len+0x1f)&~0x1f;
	if(__ARQMFreeBytes>=rlen && __ARQMStackLocation<=(ARQM_STACKENTRIES-1)) {
		ptr = &__ARQMInfo[__ARQMStackLocation];
		
		_CPU_ISR_Disable(level);
		ptr->aram_start = __ARQMStackPointer[__ARQMStackLocation];
		__ARQMStackLocation++;
		__ARQMStackPointer[__ARQMStackLocation] = ptr->aram_start+rlen;
		__ARQMFreeBytes -= rlen;
		ptr->callback = tccb;
		ptr->poll_flag = FALSE;

		ARQ_PostRequest(&ptr->arqhandle,__ARQMStackLocation-1,ARQ_MRAMTOARAM,ARQ_PRIO_HI,(u32)buff,ptr->aram_start,rlen,__ARQMPollCallback);
		_CPU_ISR_Restore(level);

		return (ptr->aram_start);
	}
	return 0;
}
