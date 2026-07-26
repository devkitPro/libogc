#include <tuxedo/thread.h>
#include <ogc/system.h>
#include <ogc/bba.h>

#include "lwp_queue.h"
#include "lwp_queue.inl"

#include "lwip/debug.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "netif/etharp.h"

#include "netif/gcif/gcif.h"

#define IFNAME0		'e'
#define IFNAME1		'0'

#define BBA_NUM_BUFFERS 32

static struct netif *gc_netif = NULL;

static lwp_queue bba_bufqueue;
static BbaBuffer bba_buffers[BBA_NUM_BUFFERS];

alignas(0x20) static u8 bba_thread_stack[0x2000];

static struct pbuf* __bba_alloc_pbuf(unsigned size)
{
	struct pbuf* p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
	if (!p) {
		LWIP_DEBUGF(NETIF_DEBUG,("__bba_alloc_pbuf: out of mem, size=%u\n",size));
		return NULL;
	}

	// Shouldn't happen
	if (p->tot_len != p->len) {
		pbuf_free(p);
		LWIP_DEBUGF(NETIF_DEBUG,("__bba_alloc_pbuf: got fragmented pbuf from lwip"));
		return NULL;
	}

	return p;
}

void bba_process(struct pbuf *p,struct netif *dev)
{
	struct eth_hdr *ethhdr = NULL;
	const s32 hlen = sizeof(struct eth_hdr);

	if(p) {
		ethhdr = p->payload;
		switch(htons(ethhdr->type)) {
			case ETHTYPE_IP:
				LWIP_DEBUGF(NETIF_DEBUG,("bba_process: passing packet up to IP layer\n"));

				etharp_ip_input(dev,p);
				pbuf_header(p,-(hlen));
				ip_input(p,dev);
				break;
			case ETHTYPE_ARP:
				/* pass p to ARP module, get ARP reply or ARP queued packet */
				LWIP_DEBUGF(NETIF_DEBUG,("bba_process: passing packet up to ARP layer\n"));
				etharp_arp_input(dev, (struct eth_addr*)dev->hwaddr, p);
				break;
			/* unsupported Ethernet packet type */
			default:
				/* free pbuf */
				pbuf_free(p);
				break;
		}
	}
}

static void __bba_buffer_cb(BbaBuffer* buf, bool ok)
{
	struct pbuf *p = buf->user;
	pbuf_free(p);
	__lwp_queue_appendI(&bba_bufqueue, (lwp_node*)buf);
}

static err_t __bba_link_tx(struct netif *dev,struct pbuf *p)
{
	BbaBuffer* buf = (BbaBuffer*)__lwp_queue_getI(&bba_bufqueue);
	if (!buf) {
		LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_tx: out of buffers\n"));
		return ERR_MEM;
	}

	if (p->len == p->tot_len) {
		// Non-fragmented pbuf: we can use it directly
		pbuf_ref(p);
	} else {
		// Fragmented pbuf: need to make a non-fragmented copy
		struct pbuf* newp = __bba_alloc_pbuf(p->tot_len);
		if (!newp) {
			__lwp_queue_appendI(&bba_bufqueue, (lwp_node*)buf);
			return ERR_MEM;
		}

		u8* ptr = newp->payload;
		while (p) {
			memcpy(ptr, p->payload, p->len);
			ptr += p->len;
			p = p->next;
		}

		p = newp;
	}

	buf->cb   = __bba_buffer_cb;
	buf->user = p;
	buf->mem  = p->payload;
	buf->size = p->tot_len;

	if (!BBA_TxEnqueue(buf)) {
		__bba_buffer_cb(buf, false);
		return ERR_IF;
	}

	return ERR_OK;
}

static err_t __bba_start_tx(struct netif *dev,struct pbuf *p,struct ip_addr *ipaddr)
{
	LWIP_DEBUGF(NETIF_DEBUG,("__bba_start_tx(%p)\n",LWP_GetSelf()));
	return etharp_output(dev,ipaddr,p);
}

static void __bba_end_rx(BbaBuffer* buf, bool ok)
{
	struct pbuf* p = buf->user;
	if (ok) {
		gc_netif->input(p, gc_netif);
	} else {
		pbuf_free(p);
	}
}

static bool __bba_begin_rx(BbaBuffer* buf, u32 size)
{
	struct pbuf* p = __bba_alloc_pbuf(size);
	if (!p) {
		return false;
	}

	buf->cb   = __bba_end_rx;
	buf->user = p;
	buf->mem  = p->payload;
	buf->size = size;

	return true;
}

err_t bba_init(struct netif *dev)
{
	if (!BBA_Init()) {
		return ERR_IF;
	}

	// Wait up to 2s for the link to be up
	u64 ref = PPCGetTickCount();
	while (BBA_GetLinkState() == BbaLinkState_Down) {
		if ((s64)(PPCGetTickCount() - ref) >= PPCMsToTicks(2000)) {
			return ERR_IF;
		}

		KThreadSleepMs(100);
	}

	dev->flags |= NETIF_FLAG_LINK_UP;
	memcpy(dev->hwaddr, g_bbaMac, 6);

	BBA_OnRxBegin(__bba_begin_rx);
	BBA_StartThread(&bba_thread_stack[sizeof(bba_thread_stack)], 0);

	return ERR_OK;
}

dev_s bba_create(struct netif *dev)
{
	LWIP_DEBUGF(NETIF_DEBUG, ("bba_create()\n"));

	__lwp_queue_initialize(&bba_bufqueue, bba_buffers, BBA_NUM_BUFFERS, sizeof(BbaBuffer));

	dev->name[0] = IFNAME0;
	dev->name[1] = IFNAME1;
	dev->output = __bba_start_tx;
	dev->linkoutput = __bba_link_tx;
	dev->mtu = 1500;
	dev->flags = NETIF_FLAG_BROADCAST;
	dev->hwaddr_len = 6;

	gc_netif = dev;
	return (void*)1;
}
