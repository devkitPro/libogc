/*-------------------------------------------------------------

$Id: message.c,v 1.7 2005-11-21 12:35:32 shagkur Exp $

message.c -- Thread subsystem II

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


#include <stdlib.h>
#include <lwp_messages.h>
#include <lwp_wkspace.h>
#include "message.h"

s32 MQ_Init(mq_box_t *mqbox,u32 count)
{
	mq_attr attr;
	mq_cntrl *ret = NULL;

	if(!mqbox) return -1;
	*mqbox = 0;
	
	__lwp_thread_dispatchdisable();

	ret = __lwpmq_allocmqueue();
	if(!ret) {
		__lwp_thread_dispatchenable();
		return MQ_ERROR_TOOMANY;
	}

	attr.mode = LWP_MQ_FIFO;
	if(!__lwpmq_initialize(ret,&attr,count,sizeof(mqmsg))) {
		__lwpmq_freemqueue(ret);
		__lwp_thread_dispatchenable();
		return MQ_ERROR_TOOMANY;
	}

	*mqbox = (mq_box_t)ret;
	__lwp_thread_dispatchenable();
	return MQ_ERROR_SUCCESSFUL;
}

void MQ_Close(mq_box_t mqbox)
{
	mq_cntrl *mbox = (mq_cntrl*)mqbox;

	if(!mbox) return;
	
	__lwp_thread_dispatchdisable();
	__lwpmq_close(mbox,0);
	__lwpmq_freemqueue(mbox);
	__lwp_thread_dispatchenable();
}

BOOL MQ_Send(mq_box_t mqbox,mqmsg msg,u32 flags)
{
	BOOL ret = FALSE;
	mq_cntrl *mbox = (mq_cntrl*)mqbox;
	u32 wait = (flags==MQ_MSG_BLOCK)?TRUE:FALSE;

	if(!mbox) return FALSE;

	__lwp_thread_dispatchdisable();
	if(__lwpmq_submit(mbox,msg,sizeof(mqmsg),(u32)mbox,LWP_MQ_SEND_REQUEST,wait,LWP_THREADQ_NOTIMEOUT)==LWP_MQ_STATUS_SUCCESSFUL) ret = TRUE;
	__lwp_thread_dispatchenable();
	return ret;
}

BOOL MQ_Receive(mq_box_t mqbox,mqmsg *msg,u32 flags)
{
	BOOL ret = FALSE;
	mq_cntrl *mbox = (mq_cntrl*)mqbox;
	u32 tmp,wait = (flags==MQ_MSG_BLOCK)?TRUE:FALSE;

	if(!mbox) return FALSE;

	__lwp_thread_dispatchdisable();
	if(__lwpmq_seize(mbox,(u32)mbox,(void*)msg,&tmp,wait,LWP_THREADQ_NOTIMEOUT)==LWP_MQ_STATUS_SUCCESSFUL) ret = TRUE;
	__lwp_thread_dispatchenable();
	return ret;
}

BOOL MQ_Jam(mq_box_t mqbox,mqmsg msg,u32 flags)
{
	BOOL ret = FALSE;
	mq_cntrl *mbox = (mq_cntrl*)mqbox;
	u32 wait = (flags==MQ_MSG_BLOCK)?TRUE:FALSE;

	if(!mbox) return FALSE;

	__lwp_thread_dispatchdisable();
	if(__lwpmq_submit(mbox,msg,sizeof(mqmsg),(u32)mbox,LWP_MQ_SEND_URGENT,wait,LWP_THREADQ_NOTIMEOUT)==LWP_MQ_STATUS_SUCCESSFUL) ret = TRUE;
	__lwp_thread_dispatchenable();
	return ret;
}
