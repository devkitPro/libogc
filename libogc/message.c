#include <stdlib.h>
#include <lwp_messages.h>
#include <lwp_wkspace.h>
#include "message.h"

typedef struct _mqbox {
	u32 id;
	mq_cntrl mqcntrl;
} mq_box;

static u32 mqbox_ids = 0;

u32 MQ_Init(mq_box_t *mqbox,u32 count)
{
	mq_attr attr;
	mq_box *ret = NULL;

	if(!mqbox) return -1;
	*mqbox = 0;
	
	__lwp_thread_dispatchdisable();

	ret = (mq_box*)__lwp_wkspace_allocate(sizeof(mq_box));
	if(!ret) {
		__lwp_thread_dispatchenable();
		return MQ_ERROR_TOOMANY;
	}

	attr.mode = LWP_MQ_FIFO;
	if(!__lwpmq_initialize(&ret->mqcntrl,&attr,count,sizeof(mqmsg))) {
		__lwp_wkspace_free(ret);
		__lwp_thread_dispatchenable();
		return MQ_ERROR_TOOMANY;
	}

	ret->id = ++mqbox_ids;
	*mqbox = (mq_box_t)ret;
	__lwp_thread_dispatchenable();
	return MQ_ERROR_SUCCESSFUL;
}

void MQ_Close(mq_box_t mqbox)
{
	mq_box *mbox = (mq_box*)mqbox;

	if(!mbox) return;
	
	__lwp_thread_dispatchdisable();
	__lwpmq_close(&mbox->mqcntrl,0);
	__lwp_wkspace_free(mbox);
	__lwp_thread_dispatchenable();
}

BOOL MQ_Send(mq_box_t mqbox,mqmsg msg,u32 flags)
{
	BOOL ret = FALSE;
	mq_box *mbox = (mq_box*)mqbox;
	u32 wait = (flags==MQ_MSG_BLOCK)?TRUE:FALSE;

	if(!mbox) return FALSE;

	__lwp_thread_dispatchdisable();
	if(__lwpmq_submit(&mbox->mqcntrl,msg,sizeof(mqmsg),mbox->id,LWP_MQ_SEND_REQUEST,wait,LWP_THREADQ_NOTIMEOUT)==LWP_MQ_STATUS_SUCCESSFUL) ret = TRUE;
	__lwp_thread_dispatchenable();
	return ret;
}

BOOL MQ_Receive(mq_box_t mqbox,mqmsg *msg,u32 flags)
{
	BOOL ret = FALSE;
	mq_box *mbox = (mq_box*)mqbox;
	u32 tmp,wait = (flags==MQ_MSG_BLOCK)?TRUE:FALSE;

	if(!mbox) return FALSE;

	__lwp_thread_dispatchdisable();
	if(__lwpmq_seize(&mbox->mqcntrl,mbox->id,(void*)msg,&tmp,wait,LWP_THREADQ_NOTIMEOUT)==LWP_MQ_STATUS_SUCCESSFUL) ret = TRUE;
	__lwp_thread_dispatchenable();
	return ret;
}

BOOL MQ_Jam(mq_box_t mqbox,mqmsg msg,u32 flags)
{
	BOOL ret = FALSE;
	mq_box *mbox = (mq_box*)mqbox;
	u32 wait = (flags==MQ_MSG_BLOCK)?TRUE:FALSE;

	if(!mbox) return FALSE;

	__lwp_thread_dispatchdisable();
	if(__lwpmq_submit(&mbox->mqcntrl,msg,sizeof(mqmsg),mbox->id,LWP_MQ_SEND_URGENT,wait,LWP_THREADQ_NOTIMEOUT)==LWP_MQ_STATUS_SUCCESSFUL) ret = TRUE;
	__lwp_thread_dispatchenable();
	return ret;
}
