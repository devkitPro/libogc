#include <tuxedo/thread.h>
#include <tuxedo/mailbox.h>

static KThrQueue s_mailboxRecvQueue;

bool KMailboxTrySend(KMailbox* mb, uptr message)
{
	PPCIrqState st = PPCIrqLockByMsr();

	if (mb->pending_slots == mb->num_slots) {
		PPCIrqUnlockByMsr(st);
		return false;
	}

	unsigned next_slot = mb->cur_slot + mb->pending_slots++;
	if (next_slot >= mb->num_slots) {
		next_slot -= mb->num_slots;
	}

	mb->slots[next_slot] = message;
	if (mb->recv_waiters) {
		mb->recv_waiters --;
		KThrQueueUnblockOneByValue(&s_mailboxRecvQueue, (uptr)mb);
	}

	PPCIrqUnlockByMsr(st);
	return true;
}

bool KMailboxTryRecv(KMailbox* mb, uptr* out)
{
	PPCIrqState st = PPCIrqLockByMsr();

	if (!mb->pending_slots) {
		PPCIrqUnlockByMsr(st);
		return false;
	}

	*out = mb->slots[mb->cur_slot++];
	mb->pending_slots --;
	if (mb->cur_slot >= mb->num_slots) {
		mb->cur_slot -= mb->num_slots;
	}

	PPCIrqUnlockByMsr(st);
	return true;
}

uptr KMailboxRecv(KMailbox* mb)
{
	PPCIrqState st = PPCIrqLockByMsr();

	if (!mb->pending_slots) {
		mb->recv_waiters ++;
		KThrQueueBlock(&s_mailboxRecvQueue, (uptr)mb);
	}

	uptr message = mb->slots[mb->cur_slot++];
	mb->pending_slots --;
	if (mb->cur_slot >= mb->num_slots) {
		mb->cur_slot -= mb->num_slots;
	}

	PPCIrqUnlockByMsr(st);
	return message;
}
