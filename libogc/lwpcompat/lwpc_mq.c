#include "lwpc_common.h"
#include <stdlib.h>
#include <tuxedo/mailbox.h>
#include <ogc/message.h>

MK_INLINE KMailbox* lwpc_get_mailbox(mqbox_t mqbox)
{
	if (!mqbox || mqbox == MQ_BOX_NULL) {
		return NULL;
	}

	return (KMailbox*)~mqbox;
}


s32 MQ_Init(mqbox_t* mqbox, u32 count)
{
	if (!mqbox || !count) {
		return -1;
	}

	KMailbox* mb = malloc(sizeof(*mb) + count*sizeof(uptr));
	if (!mb) {
		*mqbox = MQ_BOX_NULL;
		return -1;
	}

	KMailboxPrepare(mb, (uptr*)(mb+1), count);

	*mqbox = ~(uptr)mb;
	return 0;
}

void MQ_Close(mqbox_t mqbox)
{
	KMailbox* mb = lwpc_get_mailbox(mqbox);
	if (mb) {
		free(mb);
	}
}

BOOL MQ_Send(mqbox_t mqbox, mqmsg_t msg, u32 flags)
{
	KMailbox* mb = lwpc_get_mailbox(mqbox);
	if (!mb) {
		return FALSE;
	}

	if (true) { // PPCIsInExcpt()
		flags |= MQ_MSG_NOBLOCK;
	}

	if (flags & MQ_MSG_NOBLOCK) {
		return KMailboxTrySend(mb, (uptr)msg);
	} else {
		//KMailboxSend(mb, (uptr)msg);
		return FALSE;
	}
}

// Not implemented:
//BOOL MQ_Jam(mqbox_t mqbox, mqmsg_t msg, u32 flags);

BOOL MQ_Receive(mqbox_t mqbox, mqmsg_t* msg, u32 flags)
{
	KMailbox* mb = lwpc_get_mailbox(mqbox);
	if (!mb) {
		return FALSE;
	}

	if (PPCIsInExcpt()) {
		flags |= MQ_MSG_NOBLOCK;
	}

	uptr slot;
	bool rc;
	if (flags & MQ_MSG_NOBLOCK) {
		rc = KMailboxTryRecv(mb, &slot);
	} else {
		slot = KMailboxRecv(mb);
		rc = true;
	}

	if (rc && msg) {
		*msg = (mqmsg_t)slot;
	}

	return rc;
}
