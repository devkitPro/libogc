#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <gctypes.h>

#define MQ_ERROR_SUCCESSFUL		0
#define MQ_ERROR_TOOMANY		5

#define MQ_MSG_BLOCK			0
#define MQ_MSG_NOBLOCK			1


#ifdef __cplusplus
extern "C" {
#endif

typedef void* mq_box_t;
typedef void* mqmsg;

u32 MQ_Init(mq_box_t *mqbox,u32 count);
void MQ_Close(mq_box_t mqbox);
BOOL MQ_Send(mq_box_t mqbox,mqmsg msg,u32 flags);
BOOL MQ_Jam(mq_box_t mqbox,mqmsg msg,u32 flags);
BOOL MQ_Receive(mq_box_t mqbox,mqmsg *msg,u32 flags);

#ifdef __cplusplus
	}
#endif

#endif
