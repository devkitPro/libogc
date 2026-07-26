#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ogc/lwp_watchdog.h>
#include <ogc/bba.h>

#include "bba.h"
#include "uip_pbuf.h"
#include "uip_netif.h"
#include "uip_arp.h"

#define IFNAME0					'e'
#define IFNAME1					't'

#if UIP_LOGGING == 1
#include <stdio.h>
#define UIP_LOG(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

static s64 bba_arp_tmr = 0;
static struct uip_pbuf* bba_rxqueue = NULL;
static bool bba_in_tx = false;
static BbaBuffer bba_buf;

static struct uip_pbuf* __bba_alloc_pbuf(unsigned size)
{
	struct uip_pbuf* p = uip_pbuf_alloc(UIP_PBUF_RAW,size,UIP_PBUF_POOL);
	if (!p) {
		UIP_LOG("__bba_alloc_pbuf: out of mem\n");
		return NULL;
	}

	// Shouldn't happen
	if (p->tot_len != p->len) {
		uip_pbuf_free(p);
		UIP_LOG("__bba_alloc_pbuf: got fragmented pbuf from uIP");
		return NULL;
	}

	return p;
}

static void bba_process(struct uip_pbuf *p,struct uip_netif *dev)
{
	struct uip_eth_hdr *ethhdr = NULL;
	const s32 ethhlen = sizeof(struct uip_eth_hdr);

	if(p) {
		ethhdr = p->payload;
		switch(htons(ethhdr->type)) {
			case UIP_ETHTYPE_IP:
				uip_arp_ipin(dev,p);
				uip_pbuf_header(p,-(ethhlen));
				dev->input(p,dev);
				break;
			case UIP_ETHTYPE_ARP:
				uip_arp_arpin(dev,(struct uip_eth_addr*)dev->hwaddr,p);
				break;
			default:
				uip_pbuf_free(p);
				break;
		}
	}
}

static void __bba_end_rx(BbaBuffer* buf, bool ok)
{
	struct uip_pbuf* p = buf->user;
	if (ok) {
		if (bba_rxqueue) {
			uip_pbuf_queue(bba_rxqueue, p);
		} else {
			bba_rxqueue = p;
		}
	} else {
		uip_pbuf_free(p);
	}
}

static bool __bba_begin_rx(BbaBuffer* buf, u32 size)
{
	struct uip_pbuf* p = __bba_alloc_pbuf(size);
	if (!p) {
		return false;
	}

	buf->cb   = __bba_end_rx;
	buf->user = p;
	buf->mem  = p->payload;
	buf->size = size;

	return true;
}

static s8_t __bba_start_tx(struct uip_netif *dev,struct uip_pbuf *p,struct uip_ip_addr *ipaddr)
{
	return uip_arp_out(dev,ipaddr,p);
}

static void __bba_buffer_cb(BbaBuffer* buf, bool ok)
{
	struct uip_pbuf *p = buf->user;
	uip_pbuf_free(p);
	bba_in_tx = false;
}

static s8_t __bba_link_tx(struct uip_netif *dev,struct uip_pbuf *p)
{
	while (bba_in_tx) {
		BBA_Poll();
	}

	if (p->len == p->tot_len) {
		// Non-fragmented pbuf: we can use it directly
		uip_pbuf_ref(p);
	} else {
		// Fragmented pbuf: need to make a non-fragmented copy
		struct uip_pbuf* newp = __bba_alloc_pbuf(p->tot_len);
		if (!newp) {
			return UIP_ERR_MEM;
		}

		u8* ptr = newp->payload;
		while (p) {
			memcpy(ptr, p->payload, p->len);
			ptr += p->len;
			p = p->next;
		}

		p = newp;
	}

	bba_buf.cb   = __bba_buffer_cb;
	bba_buf.user = p;
	bba_buf.mem  = p->payload;
	bba_buf.size = p->tot_len;
	bba_in_tx    = true;

	if (!BBA_TxEnqueue(&bba_buf)) {
		__bba_buffer_cb(&bba_buf, false);
		return UIP_ERR_IF;
	}

	return UIP_ERR_OK;
}

s8_t uip_bba_init(struct uip_netif *dev)
{
	s8_t ret;
	s32_t cnt;

	if (!BBA_Init()) {
		return UIP_ERR_IF;
	}

	cnt = 0;
	do {
		udelay(500);
		cnt++;
		ret = BBA_GetLinkState() != BbaLinkState_Down;
	} while (!ret && cnt<10000);
	if(!ret) return UIP_ERR_IF;

	dev->flags |= UIP_NETIF_FLAG_LINK_UP;
	memcpy(dev->hwaddr, g_bbaMac, 6);

	BBA_OnRxBegin(__bba_begin_rx);

	uip_netif_setup(dev);
	uip_arp_init();

	bba_arp_tmr = gettime();

	return UIP_ERR_OK;
}

uipdev_s uip_bba_create(struct uip_netif *dev)
{
	dev->name[0] = IFNAME0;
	dev->name[1] = IFNAME1;

	dev->output = __bba_start_tx;
	dev->linkoutput = __bba_link_tx;
	dev->mtu = 1500;
	dev->flags = UIP_NETIF_FLAG_BROADCAST;
	dev->hwaddr_len = 6;

	return (void*)1;
}

void uip_bba_poll(struct uip_netif *dev)
{
	u64 now = gettime();
	if(diff_msec(bba_arp_tmr,now)>=UIP_ARP_TMRINTERVAL) {
		uip_arp_timer();
		bba_arp_tmr += PPCMsToTicks(UIP_ARP_TMRINTERVAL);
	}

	BBA_Poll();

	while (bba_rxqueue) {
		struct uip_pbuf* p = bba_rxqueue;
		bba_rxqueue = uip_pbuf_dequeue(p);
		bba_process(p, dev);
	}
}
