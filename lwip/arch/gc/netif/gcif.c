#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "spinlock.h"
#include "irq.h"
#include "exi.h"
#include "cache.h"
#include "ogcsys.h"
#include "lwp.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"
#include "lwip/debug.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "netif/etharp.h"

#include "netif/gcif/gcif.h"

//#define _BBA_DEBUG

#define IFNAME0		'e'
#define IFNAME1		'n'

#define GCIF_TX_TQ				8
#define GCIF_EXI_TQ				9

#define BBA_MINPKTSIZE			60

#define BBA_CID					0x04020200
	
#define BBA_CMD_IRMASKALL		0x00
#define BBA_CMD_IRMASKNONE		0xF8

#define BBA_NCRA				0x00		/* Network Control Register A, RW */
#define BBA_NCRA_RESET			(1<<0)	/* RESET */
#define BBA_NCRA_ST0			(1<<1)	/* ST0, Start transmit command/status */
#define BBA_NCRA_ST1			(1<<2)	/* ST1,  " */
#define BBA_NCRA_SR				(1<<3)	/* SR, Start Receive */

#define BBA_NCRB				0x01		/* Network Control Register B, RW */
#define BBA_NCRB_PR				(1<<0)	/* PR, Promiscuous Mode */
#define BBA_NCRB_CA				(1<<1)	/* CA, Capture Effect Mode */
#define BBA_NCRB_PM				(1<<2)	/* PM, Pass Multicast */
#define BBA_NCRB_PB				(1<<3)	/* PB, Pass Bad Frame */
#define BBA_NCRB_AB				(1<<4)	/* AB, Accept Broadcast */
#define BBA_NCRB_HBD			(1<<5)	/* HBD, reserved */
#define BBA_NCRB_RXINTC0		(1<<6)	/* RXINTC, Receive Interrupt Counter */
#define BBA_NCRB_RXINTC1		(1<<7)	/*  " */
#define BBA_NCRB_1_PACKET_PER_INT  (0<<6)	/* 0 0 */
#define BBA_NCRB_2_PACKETS_PER_INT (1<<6)	/* 0 1 */
#define BBA_NCRB_4_PACKETS_PER_INT (2<<6)	/* 1 0 */
#define BBA_NCRB_8_PACKETS_PER_INT (3<<6)	/* 1 1 */

#define BBA_LTPS 0x04		/* Last Transmitted Packet Status, RO */
#define BBA_LRPS 0x05		/* Last Received Packet Status, RO */

#define BBA_IMR 0x08		/* Interrupt Mask Register, RW, 00h */
#define   BBA_IMR_FRAGIM     (1<<0)	/* FRAGIM, Fragment Counter Int Mask */
#define   BBA_IMR_RIM        (1<<1)	/* RIM, Receive Interrupt Mask */
#define   BBA_IMR_TIM        (1<<2)	/* TIM, Transmit Interrupt Mask */
#define   BBA_IMR_REIM       (1<<3)	/* REIM, Receive Error Interrupt Mask */
#define   BBA_IMR_TEIM       (1<<4)	/* TEIM, Transmit Error Interrupt Mask */
#define   BBA_IMR_FIFOEIM    (1<<5)	/* FIFOEIM, FIFO Error Interrupt Mask */
#define   BBA_IMR_BUSEIM     (1<<6)	/* BUSEIM, BUS Error Interrupt Mask */
#define   BBA_IMR_RBFIM      (1<<7)	/* RBFIM, RX Buffer Full Interrupt Mask */

#define BBA_IR 0x09		/* Interrupt Register, RW, 00h */
#define   BBA_IR_FRAGI       (1<<0)	/* FRAGI, Fragment Counter Interrupt */
#define   BBA_IR_RI          (1<<1)	/* RI, Receive Interrupt */
#define   BBA_IR_TI          (1<<2)	/* TI, Transmit Interrupt */
#define   BBA_IR_REI         (1<<3)	/* REI, Receive Error Interrupt */
#define   BBA_IR_TEI         (1<<4)	/* TEI, Transmit Error Interrupt */
#define   BBA_IR_FIFOEI      (1<<5)	/* FIFOEI, FIFO Error Interrupt */
#define   BBA_IR_BUSEI       (1<<6)	/* BUSEI, BUS Error Interrupt */
#define   BBA_IR_RBFI        (1<<7)	/* RBFI, RX Buffer Full Interrupt */

#define BBA_BP   0x0a/*+0x0b*/	/* Boundary Page Pointer Register */
#define BBA_TLBP 0x0c/*+0x0d*/	/* TX Low Boundary Page Pointer Register */
#define BBA_TWP  0x0e/*+0x0f*/	/* Transmit Buffer Write Page Pointer Register */
#define BBA_TRP  0x12/*+0x13*/	/* Transmit Buffer Read Page Pointer Register */
#define BBA_RWP  0x16/*+0x17*/	/* Receive Buffer Write Page Pointer Register */
#define BBA_RRP  0x18/*+0x19*/	/* Receive Buffer Read Page Pointer Register */
#define BBA_RHBP 0x1a/*+0x1b*/	/* Receive High Boundary Page Pointer Register */

#define BBA_RXINTT    0x14/*+0x15*/	/* Receive Interrupt Timer Register */

#define BBA_NAFR_PAR0 0x20	/* Physical Address Register Byte 0 */
#define BBA_NAFR_PAR1 0x21	/* Physical Address Register Byte 1 */
#define BBA_NAFR_PAR2 0x22	/* Physical Address Register Byte 2 */
#define BBA_NAFR_PAR3 0x23	/* Physical Address Register Byte 3 */
#define BBA_NAFR_PAR4 0x24	/* Physical Address Register Byte 4 */
#define BBA_NAFR_PAR5 0x25	/* Physical Address Register Byte 5 */

#define BBA_NWAYC 0x30		/* NWAY Configuration Register, RW, 84h */
#define   BBA_NWAYC_FD       (1<<0)	/* FD, Full Duplex Mode */
#define   BBA_NWAYC_PS100    (1<<1)	/* PS100/10, Port Select 100/10 */
#define   BBA_NWAYC_ANE      (1<<2)	/* ANE, Autonegotiation Enable */
#define   BBA_NWAYC_ANS_RA   (1<<3)	/* ANS, Restart Autonegotiation */
#define   BBA_NWAYC_LTE      (1<<7)	/* LTE, Link Test Enable */

#define BBA_NWAYS 0x31
#define   BBA_NWAYS_LS10	 (1<<0)
#define   BBA_NWAYS_LS100	 (1<<1)
#define   BBA_NWAYS_LPNWAY   (1<<2)
#define   BBA_NWAYS_ANCLPT	 (1<<3)
#define   BBA_NWAYS_100TXF	 (1<<4)
#define   BBA_NWAYS_100TXH	 (1<<5)
#define   BBA_NWAYS_10TXF	 (1<<6)
#define   BBA_NWAYS_10TXH	 (1<<7)

#define BBA_GCA 0x32		/* GMAC Configuration A Register, RW, 00h */
#define   BBA_GCA_ARXERRB    (1<<3)	/* ARXERRB, Accept RX packet with error */

#define BBA_MISC 0x3d		/* MISC Control Register 1, RW, 3ch */
#define   BBA_MISC_BURSTDMA  (1<<0)
#define   BBA_MISC_DISLDMA   (1<<1)

#define BBA_TXFIFOCNT 0x3e/*0x3f*/	/* Transmit FIFO Counter Register */
#define BBA_WRTXFIFOD 0x48/*-0x4b*/	/* Write TX FIFO Data Port Register */

#define BBA_MISC2 0x50		/* MISC Control Register 2, RW, 00h */
#define   BBA_MISC2_HBRLEN0		(1<<0)	/* HBRLEN, Host Burst Read Length */
#define   BBA_MISC2_HBRLEN1		(1<<1)	/*  " */
#define   BBA_MISC2_RUNTSIZE	(1<<2)	/*  " */
#define   BBA_MISC2_DREQBCTRL	(1<<3)	/*  " */
#define   BBA_MISC2_RINTSEL		(1<<4)	/*  " */
#define   BBA_MISC2_ITPSEL		(3<<5)	/*  " */
#define   BBA_MISC2_AUTORCVR	(1<<7)	/* Auto RX Full Recovery */

#define BBA_RX_STATUS_BF      (1<<0)
#define BBA_RX_STATUS_CRC     (1<<1)
#define BBA_RX_STATUS_FAE     (1<<2)
#define BBA_RX_STATUS_FO      (1<<3)
#define BBA_RX_STATUS_RW      (1<<4)
#define BBA_RX_STATUS_MF      (1<<5)
#define BBA_RX_STATUS_RF      (1<<6)
#define BBA_RX_STATUS_RERR    (1<<7)

#define BBA_TX_STATUS_CC0     (1<<0)
#define BBA_TX_STATUS_CC1     (1<<1)
#define BBA_TX_STATUS_CC2     (1<<2)
#define BBA_TX_STATUS_CC3     (1<<3)
#define  BBA_TX_STATUS_CCMASK (0x0f)
#define BBA_TX_STATUS_CRSLOST (1<<4)
#define BBA_TX_STATUS_UF      (1<<5)
#define BBA_TX_STATUS_OWC     (1<<6)
#define BBA_TX_STATUS_OWN     (1<<7)
#define BBA_TX_STATUS_TERR    (1<<7)

#define BBA_TX_MAX_PACKET_SIZE 1518	/* 14+1500+4 */
#define BBA_RX_MAX_PACKET_SIZE 1536	/* 6 pages * 256 bytes */

#define BBA_INIT_TLBP	0x00
#define BBA_INIT_BP	0x01
#define BBA_INIT_RHBP	0x0f
#define BBA_INIT_RWP	BBA_INIT_BP
#define BBA_INIT_RRP	BBA_INIT_BP

#define BBA_NAPI_WEIGHT 16

#define X(a,b)  b,a
struct bba_descr {
	u32 X(X(next_packet_ptr:12, packet_len:12), status:8);
} __attribute((packed));

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))


struct bba_priv {
	u8 revid;
	u16 devid;
	u8 acstart;
	err_t state;
	struct eth_addr *ethaddr;
};

static u32 net_arp_ticks = 0;
static wd_cntrl arp_time_cntrl;

static lwpq_t wait_exi_queue;
static lwpq_t wait_tx_queue;

static struct netif *gc_netif = NULL;

static vu32* const _siReg = (u32*)0xCC006400;
static const struct eth_addr ethbroadcast = {{0xffU,0xffU,0xffU,0xffU,0xffU,0xffU}};

static u8 cur_rcv_buffer0[BBA_RX_MAX_PACKET_SIZE] ATTRIBUTE_ALIGN(32);
static u8 cur_rcv_buffer1[BBA_RX_MAX_PACKET_SIZE] ATTRIBUTE_ALIGN(32);
static u8 cur_snd_buffer[BBA_TX_MAX_PACKET_SIZE] ATTRIBUTE_ALIGN(32);
static u8 cur_rcv_postbuffer[BBA_TX_MAX_PACKET_SIZE] ATTRIBUTE_ALIGN(32);
static u32 cur_rcv_postdmalen = 0;
static u32 cur_rcv_postimmlen = 0;
static u32 cur_rcv_postlen = 0;
static u32 cur_snd_dmalen = 0;
static u32 cur_snd_immlen = 0;
static u32 cur_snd_len = 0;
static u32 cur_rcv_len0 = 0;
static u32 cur_rcv_len1 = 0;
static u16 rrp = 0,rwp = 0;
static struct pbuf *cur_rcv_buf = NULL;
static struct bba_descr cur_descr;

static err_t __bba_link_tx(struct netif *dev,struct pbuf *p);

static u32 __bba_link_rxbuffer1();
static u32 __bba_link_rxsub1();
static u32 __bba_link_rxsub0(u32 pos);
static u32 __bba_link_rx();
static u32 __bba_rx_err(u8 status);

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern void udelay(int us);
extern unsigned int timespec_to_interval(const struct timespec *);

/* new functions */
#define bba_select()		EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_2,EXI_SPEED32MHZ)
#define bba_deselect()		EXI_Deselect(EXI_CHANNEL_0)

#define bba_in12(reg)		((bba_in8(reg)&0xff)|((bba_in8((reg)+1)&0x0f)<<8))
#define bba_out12(reg,val)	do { \
									bba_out8((reg),(val)&0xff); \
									bba_out8((reg)+1,((val)&0x0f00)>>8); \
							} while(0)

static void bba_cmd_ins(u32 reg,void *val,u32 len);
static void bba_cmd_outs(u32 reg,void *val,u32 len);
static void bba_ins(u32 reg,void *val,u32 len);
static void bba_outs(u32 reg,void *val,u32 len);

static __inline__ void bba_cmd_insnosel(u32 reg,void *val,u32 len)
{
	u16 req;
	req = reg<<8;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_READ);
}

static __inline__ void bba_cmd_ins(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_cmd_insnosel(reg,val,len);
	bba_deselect();
}

static inline void bba_cmd_outsnosel(u32 reg,void *val,u32 len)
{
	u16 req;
	req = (reg<<8)|0x4000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_WRITE);
}

static void bba_cmd_outs(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_cmd_outsnosel(reg,val,len);
	bba_deselect();
}

static inline u8 bba_cmd_in8(u32 reg)
{
	u8 val;
	bba_cmd_ins(reg,&val,sizeof(val));
	return val;
}

static inline u8 bba_cmd_in8_slow(u32 reg)
{
	u8 val;
	bba_select();
	bba_cmd_insnosel(reg,&val,sizeof(val));
	udelay(200);			//usleep doesn't work on this amount, decrementer is based on 10ms, wait is 200us
	bba_deselect();
	return val;
}

static inline void bba_cmd_out8(u32 reg,u8 val)
{
	bba_cmd_outs(reg,&val,sizeof(val));
}

static inline u8 bba_in8(u32 reg)
{
	u8 val;
	bba_ins(reg,&val,sizeof(val));
	return val;
}

static inline void bba_out8(u32 reg,u8 val)
{
	bba_outs(reg,&val,sizeof(val));
}

static inline void bba_insnosel(u32 reg,void *val,u32 len)
{
	u32 req;
	req = (reg<<8)|0x80000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_READ);
}

static void bba_ins(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_insnosel(reg,val,len);
	bba_deselect();
}

static inline void bba_outsnoselect(u32 reg,void *val,u32 len)
{
	u32 req;
	req = (reg<<8)|0xC0000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_WRITE);
}

static void bba_outs(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_outsnoselect(reg,val,len);
	bba_deselect();
}

static inline void bba_insregister(u32 reg)
{
	u32 req;
	req = (reg<<8)|0x80000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
}

static inline void bba_insdata(void *val,u32 len)
{
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_READ);
}

static inline void bba_insdmadata(void *val,u32 len,u32 (*dmasubrcv)())
{
	EXI_Dma(EXI_CHANNEL_0,val,len,EXI_READ,dmasubrcv);
}

static inline void bba_outsregister(u32 reg)
{
	u32 req;
	req = (reg<<8)|0xC0000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
}

static inline void bba_outsdata(void *val,u32 len)
{
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_WRITE);
}

static inline void bba_outsdmadata(void *val,u32 len,u32 (*dmasubsnd)())
{
	EXI_Dma(EXI_CHANNEL_0,val,len,EXI_WRITE,dmasubsnd);
}

static u32 __bba_exi_unlock()
{
	LWP_WakeThread(wait_exi_queue);
	return 1;
}

static u32 __bba_exi_wait()
{
	u32 level,ret = 0;
	
	_CPU_ISR_Disable(level);
	do {
		if((ret=EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,__bba_exi_unlock))==1) break;
		LWP_SleepThread(wait_exi_queue);
	} while(ret==0);
	_CPU_ISR_Restore(level);
	return ret;
}

static u32 __bba_tx_wake(struct bba_priv *priv)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(priv->state==ERR_TXPENDING) {
		priv->state = ERR_OK;
		LWP_WakeThread(wait_tx_queue);
		LWIP_DEBUGF(NETIF_DEBUG,("__bba_tx_wake(%p,%p)\n",priv,LWP_GetSelf()));
	}
	_CPU_ISR_Restore(level);
	return 1;
}

static u32 __bba_tx_stop(struct bba_priv *priv)
{
	u32 level;
	err_t state;

	_CPU_ISR_Disable(level);
	state = priv->state;
	while(priv->state==ERR_TXPENDING) {
		if(LWP_SleepThread(wait_tx_queue)==-1) break;
	}
	priv->state = ERR_TXPENDING;
	_CPU_ISR_Restore(level);

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_tx_stop(%p,%s,%p)\n",priv,state==ERR_TXPENDING?"ERR_TXPENDING":"ERR_OK",LWP_GetSelf()));
	return 1;
}

static __inline__ u32 __linkstate()
{
	u8 nways = 0;

	nways = bba_in8(BBA_NWAYS);
	if(nways&BBA_NWAYS_LS10 || nways&BBA_NWAYS_LS100) return 1;
	return 0;
}

static u32 __bba_getlink_state_async()
{
	u32 level,ret;


	_CPU_ISR_Disable(level);
	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL)==0) {
		LWIP_ERROR(("__bba_getlink_state_async(exi allready locked)\n"));
		_CPU_ISR_Restore(level);
		return 0;
	}
	_CPU_ISR_Restore(level);

	ret = __linkstate();
	EXI_Unlock(EXI_CHANNEL_0);
	return ret;
}

static u32 __bba_read_cid()
{
	u16 cmd = 0;
	u32 cid = 0;

	bba_select();
	EXI_Imm(EXI_CHANNEL_0,&cmd,2,EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_Imm(EXI_CHANNEL_0,&cid,4,EXI_READ,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	bba_deselect();

	return cid;
}
static void __bba_reset()
{
	bba_out8(0x60,0x00);
	udelay(10000);
	bba_cmd_in8_slow(0x0F);
	udelay(10000);
	bba_out8(BBA_NCRA,BBA_NCRA_RESET);
	bba_out8(BBA_NCRA,0x00);
}

static void __bba_recv_init()
{
	bba_out8(BBA_NCRB,(BBA_NCRB_AB|BBA_NCRB_CA));
	bba_out8(BBA_MISC2,(BBA_MISC2_AUTORCVR));

	bba_out12(BBA_TLBP, BBA_INIT_TLBP);
	bba_out12(BBA_BP,BBA_INIT_BP);
	bba_out12(BBA_RWP,BBA_INIT_RWP);
	bba_out12(BBA_RRP,BBA_INIT_RRP);
	bba_out12(BBA_RHBP,BBA_INIT_RHBP);

	bba_out8(BBA_NCRA,BBA_NCRA_SR);
	bba_out8(BBA_GCA,BBA_GCA_ARXERRB);
}

static u32 __bba_link_postrxsub1()
{
	struct bba_priv *priv = (struct bba_priv*)gc_netif->state;

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_postrxsub1(imm: %d)\n",cur_rcv_postimmlen));
	priv->state = ERR_TXPENDING;
	if(cur_rcv_postimmlen) bba_outsdata(cur_rcv_postbuffer+cur_rcv_postdmalen,cur_rcv_postimmlen);
	bba_deselect();

	bba_out8(BBA_NCRA,((bba_in8(BBA_NCRA)|BBA_NCRA_ST1)));		//&~BBA_NCRA_ST0
	EXI_Unlock(EXI_CHANNEL_0);

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_postrxsub1(rx interrupt close)\n"));
	return ERR_OK;
}

static u32 __bba_link_postrxsub0()
{
	u32 len,level;
	struct bba_priv *priv = (struct bba_priv*)gc_netif->state;

	_CPU_ISR_Disable(level);
	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,__bba_link_postrxsub0)==0) {
		LWIP_ERROR(("__bba_link_postrxsub0(exi allready locked)\n"));
		_CPU_ISR_Restore(level);
		return ERR_OK;
	}
	priv->state = ERR_TXPENDING;
	_CPU_ISR_Restore(level);

	if(!__linkstate()) {
		LWIP_ERROR(("__bba_link_postrxsub0(error link state)\n"));
		priv->state = ERR_OK;
		EXI_Unlock(EXI_CHANNEL_0);
		return ERR_ABRT;
	}

	while((bba_in8(BBA_NCRA)&(BBA_NCRA_ST0|BBA_NCRA_ST1)));

	bba_out12(BBA_TXFIFOCNT,cur_rcv_postlen);
	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_postrxsub0(sndlen: %d)\n",cur_rcv_postlen));

	len = cur_rcv_postlen;
	if(len<BBA_MINPKTSIZE) len = BBA_MINPKTSIZE;

	cur_rcv_postdmalen = len&~0x1f;
	cur_rcv_postimmlen = len&0x1f;
	DCStoreRange(cur_rcv_postbuffer,cur_rcv_postdmalen);

	bba_select();
	bba_outsregister(BBA_WRTXFIFOD);
	bba_outsdmadata(cur_rcv_postbuffer,cur_rcv_postdmalen,__bba_link_postrxsub1);

	return ERR_TXPENDING;
}

static u32 __bba_tx_err(u8 status)
{
	u32 errors;

	errors = 0;
	if(status&BBA_TX_STATUS_TERR) {
		errors++;
		if(status&BBA_TX_STATUS_CCMASK) errors++;
		if(status&BBA_TX_STATUS_CRSLOST) errors++;
		if(status&BBA_TX_STATUS_UF) errors++;
		if(status&BBA_TX_STATUS_OWC) errors++;
	}
	if(errors) LWIP_ERROR(("bba_tx_err(%02x)\n",status));

	return errors;
}

static err_t __bba_post_send()
{
	u8 *buf;
	err_t ret = ERR_OK;
	struct pbuf *p,*tmp,*q = NULL;
	struct eth_hdr *ethhdr = NULL;
	struct bba_priv *priv = (struct bba_priv*)gc_netif->state;
	
	while(cur_rcv_buf && priv->state!=ERR_TXPENDING) {
		p = cur_rcv_buf;
		cur_rcv_buf = pbuf_dechain(cur_rcv_buf);
		ethhdr = p->payload;
		switch(htons(ethhdr->type)) {
			case ETHTYPE_IP:
				LWIP_DEBUGF(NETIF_DEBUG,("__bba_post_send: passing packet up to IP layer\n"));

				q = etharp_ip_input(gc_netif,p);
				pbuf_header(p,-14);
				gc_netif->input(p, gc_netif);
				break;
			case ETHTYPE_ARP:
				/* pass p to ARP module, get ARP reply or ARP queued packet */
				LWIP_DEBUGF(NETIF_DEBUG,("__bba_post_send: passing packet up to ARP layer\n"));
				q = etharp_arp_input(gc_netif, priv->ethaddr, p);
				break;
			/* unsupported Ethernet packet type */
			default:
				/* free pbuf */
				pbuf_free(p);
				p = NULL;
				break;
		}


		if(q!=NULL) {
			LWIP_DEBUGF(NETIF_DEBUG,("__bba_post_send: q!=NULL\n"));
			buf = cur_rcv_postbuffer;
			priv->state = ERR_TXPENDING;
			for(tmp=q;tmp!=NULL;tmp=tmp->next) {
				memcpy(buf,tmp->payload,tmp->len);
				buf += tmp->len;
			}

			cur_rcv_postlen = q->tot_len;
			ret = __bba_link_postrxsub0();
			pbuf_free(q);
		}
	}
	return ret;
}

static u32 __bba_link_txsub1()
{
	struct bba_priv *priv = (struct bba_priv*)gc_netif->state;

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_txsub1(imm: %d)\n",cur_snd_immlen));
	priv->state = ERR_TXPENDING;
	if(cur_snd_immlen) bba_outsdata(cur_snd_buffer+cur_snd_dmalen,cur_snd_immlen);
	bba_deselect();

	bba_out8(BBA_NCRA,((bba_in8(BBA_NCRA)|BBA_NCRA_ST1)));		//&~BBA_NCRA_ST0
	EXI_Unlock(EXI_CHANNEL_0);
	return ERR_OK;
}

static u32 __bba_link_txsub0()
{
	u32 level,len;
	struct bba_priv *priv = (struct bba_priv*)gc_netif->state;

	_CPU_ISR_Disable(level);
	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,__bba_link_txsub0)==0) {
		LWIP_ERROR(("__bba_link_txsub0(exi allready locked)\n"));
		_CPU_ISR_Restore(level);
		return ERR_OK;
	}
	priv->state = ERR_TXPENDING;
	_CPU_ISR_Restore(level);

	if(!__linkstate()) {
		LWIP_ERROR(("__bba_link_txsub0(error link state)\n"));
		__bba_tx_wake(priv);
		EXI_Unlock(EXI_CHANNEL_0);
		return ERR_ABRT;
	}

	while((bba_in8(BBA_NCRA)&(BBA_NCRA_ST0|BBA_NCRA_ST1)));

	bba_out12(BBA_TXFIFOCNT,cur_snd_len);
	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_txsub0(%d,%p)\n",cur_snd_len,LWP_GetSelf()));

	len = cur_snd_len;
	if(len<BBA_MINPKTSIZE) len = BBA_MINPKTSIZE;

	cur_snd_dmalen = len&~0x1f;
	cur_snd_immlen = len&0x1f;
	DCStoreRange(cur_snd_buffer,cur_snd_dmalen);

	bba_select();
	bba_outsregister(BBA_WRTXFIFOD);
	bba_outsdmadata(cur_snd_buffer,cur_snd_dmalen,__bba_link_txsub1);

	return ERR_OK;
}

static err_t __bba_link_tx(struct netif *dev,struct pbuf *p)
{
	u32 level;
	struct pbuf *tmp;
	struct bba_priv *priv = (struct bba_priv*)dev->state;

	_CPU_ISR_Disable(level);
	__bba_tx_stop(priv);
	if(p->tot_len>BBA_TX_MAX_PACKET_SIZE) {
		LWIP_ERROR(("__bba_link_tx(%d,%p) pkt_size\n",p->tot_len,LWP_GetSelf()));
		__bba_tx_wake(priv);
		_CPU_ISR_Restore(level);
		return ERR_PKTSIZE;
	}

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_tx(%d,%p)\n",p->tot_len,LWP_GetSelf()));

	cur_snd_len = 0;
	for(tmp=p;tmp!=NULL;tmp=tmp->next) {
		memcpy(cur_snd_buffer+cur_snd_len,tmp->payload,tmp->len);
		cur_snd_len += tmp->len;
	}
	_CPU_ISR_Restore(level);

	return __bba_link_txsub0();
}

static err_t bba_start_tx(struct netif *dev,struct pbuf *p,struct ip_addr *ipaddr)
{
	err_t ret = ERR_OK;

	LWIP_DEBUGF(NETIF_DEBUG,("bba_start_tx(%p)\n",LWP_GetSelf()));

	p = etharp_output(dev,ipaddr,p);
	if(p) ret = __bba_link_tx(dev,p);

	return ret;
}

static u32 __bba_rx_err(u8 status)
{
	u32 errors;
	
	errors = 0;
	if(status&BBA_RX_STATUS_RERR) {
		errors++;
		if(status&BBA_RX_STATUS_BF) errors++;
		if(status&BBA_RX_STATUS_CRC) errors++;
		if(status&BBA_RX_STATUS_FO) errors++;
		if(status&BBA_RX_STATUS_RW) errors++;
		if(status&BBA_RX_STATUS_FAE) errors++;
	}
	if(errors) {
		LWIP_ERROR(("bba_rx_err(%d)\n",errors));
		bba_out8(BBA_NCRA,bba_in8(BBA_NCRA)&~BBA_NCRA_SR);
		bba_out12(BBA_RWP,BBA_INIT_RWP);
		bba_out12(BBA_RRP,BBA_INIT_RRP);
		bba_out8(BBA_NCRA,bba_in8(BBA_NCRA)|BBA_NCRA_SR);
	}
	
	return errors;
}

static u32 __bba_link_rxbuffer1()
{
	u32 len,ret = ERR_OK;
	void *pc;
	u8 *rcv_buf0,*rcv_buf1;
	struct pbuf *tmp,*p = NULL;

	bba_deselect();

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_rxbuffer1(%d,%d)\n",cur_rcv_len0,cur_rcv_len1));

	rcv_buf0 = cur_rcv_buffer0;
	rcv_buf1 = cur_rcv_buffer1;
	p = pbuf_alloc(PBUF_LINK,cur_rcv_len0+cur_rcv_len1,PBUF_POOL);
	for(tmp=p;tmp!=NULL;tmp=tmp->next) {
		pc = tmp->payload;
		len = tmp->len;
		if(cur_rcv_len0>0 && cur_rcv_len0<=len) {
			DCInvalidateRange(rcv_buf0,((cur_rcv_len0+31)&~31));
			memcpy(pc,rcv_buf0,cur_rcv_len0);
			pc += cur_rcv_len0;
			len -= cur_rcv_len0;
			cur_rcv_len0 = 0;
		} else if(cur_rcv_len0>0 && cur_rcv_len0>len) {
			DCInvalidateRange(rcv_buf0,((len+31)&~31));
			memcpy(pc,rcv_buf0,len);
			cur_rcv_len0 -= len;
			rcv_buf0 += len;
			pc += len;
		}
		if(!cur_rcv_len0 && (cur_rcv_len1>0 && cur_rcv_len1<=len)) {
			DCInvalidateRange(rcv_buf1,((cur_rcv_len1+31)&~31));
			memcpy(pc,rcv_buf1,cur_rcv_len1);
			pc += cur_rcv_len1;
			len -= cur_rcv_len1;
			cur_rcv_len1 = 0;
		} else if(!cur_rcv_len0 && (cur_rcv_len1>0 && cur_rcv_len1>len)) {
			DCInvalidateRange(rcv_buf1,((len+31)&~31));
			memcpy(pc,rcv_buf1,len);
			cur_rcv_len1 -= len;
			rcv_buf1 += len;
			pc += len;
		}

	}
	
	if(cur_rcv_buf) {
		pbuf_chain(cur_rcv_buf,p);
	} else {
		cur_rcv_buf = p;
	}
	
	rrp = cur_descr.next_packet_ptr;
	ret = __bba_link_rx();

	return ret;
}

static u32 __bba_link_rxsub1()
{
	u32 rcv_len = (cur_rcv_len1+31)&~31;

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_rxsub1()\n"));

	bba_deselect();

	rrp = BBA_INIT_RRP;
	
	bba_select();
	bba_insregister((rrp<<8));
	bba_insdmadata(cur_rcv_buffer1,rcv_len,__bba_link_rxbuffer1);
	return ERR_OK;
}

static u32 __bba_link_rxsub0(u32 pos)
{
	u32 rcv_len = (cur_rcv_len0+31)&~31;

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_rxsub0(%04x,%d,%d)\n",pos,cur_rcv_len0,cur_rcv_len1));
	
	bba_select();
	bba_insregister(pos);
	if(cur_rcv_len1) {
		bba_insdmadata(cur_rcv_buffer0,rcv_len,__bba_link_rxsub1);
	} else {
		bba_insdmadata(cur_rcv_buffer0,rcv_len,__bba_link_rxbuffer1);
	}
	
	return ERR_OK;
}

static u32 __bba_link_rx()
{
	u32 top,pos,pkt_status,size,ret;

	cur_rcv_len0 = 0;
	cur_rcv_len1 = 0;
	
	while(rrp!=rwp) {
		LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_rx(%04x,%04x)\n",rrp,rwp));
		bba_ins(rrp<<8,&cur_descr,sizeof(cur_descr));
		le32_to_cpus((u32*)&cur_descr);
		
		size = cur_descr.packet_len - 4;
		pkt_status = cur_descr.status;
		if(size>(BBA_RX_MAX_PACKET_SIZE+4) || (pkt_status&(BBA_RX_STATUS_RERR|BBA_RX_STATUS_FAE))) {
			LWIP_ERROR(("__bba_link_rx(size>BBA_RX_MAX_PACKET_SIZE || (pkt_status&(BBA_RX_STATUS_RERR|BBA_RX_STATUS_FAE))\n"));
			__bba_rx_err(pkt_status);

			bba_out8(BBA_IR,BBA_IR_RI);
			bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
			EXI_Unlock(EXI_CHANNEL_0);
			return ERR_PKTSIZE;
		}

		pos = (rrp<<8)+4;
		top = (BBA_INIT_RHBP+1)<<8;
		if((pos+size)<top) {
			cur_rcv_len0 = size;
			cur_rcv_len1 = 0;
		} else {
			cur_rcv_len0 = top - pos;
			cur_rcv_len1 = size - cur_rcv_len0;
		}
		
		ret = __bba_link_rxsub0(pos);
		return ret;
	}
	bba_out12(BBA_RRP,rwp);

	LWIP_DEBUGF(NETIF_DEBUG,("__bba_link_rx(rx interrupt close)\n"));
	bba_out8(BBA_IR,BBA_IR_RI);
	bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
	EXI_Unlock(EXI_CHANNEL_0);

	__bba_post_send();

	return ERR_OK;
}

static err_t bba_start_rx()
{
	u32 ret;

	LWIP_DEBUGF(NETIF_DEBUG,("bba_start_rx()\n"));

	rwp = bba_in12(BBA_RWP);
	rrp = bba_in12(BBA_RRP);

	ret = __bba_link_rx();
	return ret;
}

static inline void bba_interrupt(struct netif *dev)
{
	u8 ir,imr,status;
	struct bba_priv *priv = (struct bba_priv*)dev->state;
	
	ir = bba_in8(BBA_IR);
	imr = bba_in8(BBA_IMR);
	status = ir&imr;

	LWIP_DEBUGF(NETIF_DEBUG,("bba_interrupt(status(%02x))\n",status));

	if(status&BBA_IR_FRAGI) {
		bba_out8(BBA_IR,BBA_IR_FRAGI);
	}
	if(status&BBA_IR_RI) {
		if(cur_rcv_buf==NULL) {
			bba_start_rx();
			return;
		}
	}
	if(status&BBA_IR_REI) {
		__bba_rx_err(bba_in8(BBA_LRPS));
		bba_out8(BBA_IR,BBA_IR_REI);
	}
	if(status&BBA_IR_TI) {
		__bba_tx_wake(priv);
		__bba_tx_err(bba_in8(BBA_LTPS));
		bba_out8(BBA_IR,BBA_IR_TI);
	}
	if(status&BBA_IR_TEI) {
		__bba_tx_wake(priv);
		__bba_tx_err(bba_in8(BBA_LTPS));
		bba_out8(BBA_IR,BBA_IR_TEI);
	}
	if(status&BBA_IR_FIFOEI) {
		__bba_tx_wake(priv);
		bba_out8(BBA_IR,BBA_IR_FIFOEI);
	}
	if(status&BBA_IR_BUSEI) {
		bba_out8(BBA_IR,BBA_IR_BUSEI);
	}
	if(status&BBA_IR_RBFI) {
		if(cur_rcv_buf==NULL) {
			bba_start_rx();
			return;
		}
	}
	bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
	EXI_Unlock(EXI_CHANNEL_0);

	__bba_post_send();
}

static err_t __bba_init(struct netif *dev)
{
	struct bba_priv *priv = (struct bba_priv*)dev->state;

	if(!priv) return ERR_IF;

	LWIP_DEBUGF(NETIF_DEBUG,("initializing BBA...\n"));
	
	__bba_reset();

	bba_ins(BBA_NAFR_PAR0,priv->ethaddr->addr, 6);
	LWIP_DEBUGF(NETIF_DEBUG,("MAC ADDRESS %02x:%02x:%02x:%02x:%02x:%02x\n", 
		priv->ethaddr->addr[0], priv->ethaddr->addr[1], priv->ethaddr->addr[2], 
		priv->ethaddr->addr[3], priv->ethaddr->addr[4], priv->ethaddr->addr[5]));
	
	priv->revid = bba_cmd_in8(0x01);
	
	bba_cmd_outs(0x04,&priv->devid,2);
	bba_cmd_out8(0x05,priv->acstart);

	bba_out8(0x5b, (bba_in8(0x5b)&~0x80));
	bba_out8(0x5e, 0x01);
	bba_out8(0x5c, (bba_in8(0x5c)|0x04));

	bba_out8(BBA_NCRB, 0x00);

	__bba_recv_init();
	
	bba_out8(BBA_IR,0xFF);
	bba_out8(BBA_IMR,0xFF&~BBA_IMR_FIFOEIM);

	bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);

	return ERR_OK;
}

static err_t bba_init_one(struct netif *dev)
{
	struct bba_priv *priv = (struct bba_priv*)dev->state;
	
	if(!priv) return ERR_IF;

	priv->revid = 0x00;
	priv->devid = 0xD107;
	priv->acstart = 0x4E;
	
	__bba_init(dev);
	
	return ERR_OK;
}

static err_t bba_probe(struct netif *dev)
{
	u32 cid;
	err_t ret;

	cid = __bba_read_cid();
	if(cid!=BBA_CID) return ERR_NODEV;
	
	ret = bba_init_one(dev);
	return ret;
}

static u32 bba_calc_response(struct bba_priv *priv,u32 val)
{
	u8 revid_0, revid_eth_0, revid_eth_1;
	revid_0 = priv->revid;
	revid_eth_0 = _SHIFTR(priv->devid,8,8);
	revid_eth_1 = priv->devid&0xff;

	u8 i0, i1, i2, i3;
	i0 = (val & 0xff000000) >> 24;
	i1 = (val & 0x00ff0000) >> 16;
	i2 = (val & 0x0000ff00) >> 8;
	i3 = (val & 0x000000ff);

	u8 c0, c1, c2, c3;
	c0 = ((i0 + i1 * 0xc1 + 0x18 + revid_0) ^ (i3 * i2 + 0x90)
	    ) & 0xff;
	c1 = ((i1 + i2 + 0x90) ^ (c0 + i0 - 0xc1)
	    ) & 0xff;
	c2 = ((i2 + 0xc8) ^ (c0 + ((revid_eth_0 + revid_0 * 0x23) ^ 0x19))
	    ) & 0xff;
	c3 = ((i0 + 0xc1) ^ (i3 + ((revid_eth_1 + 0xc8) ^ 0x90))
	    ) & 0xff;

	return ((c0 << 24) | (c1 << 16) | (c2 << 8) | c3);
}

static u32 bba_event_handler(u32 nChn,u32 nDev)
{
	u8 status;
	struct bba_priv *priv = (struct bba_priv*)gc_netif->state;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,bba_event_handler)==0) {
		LWIP_ERROR(("bba_event_handler(exi allready locked)\n"));
		return 1;
	}

	status = bba_cmd_in8(0x03);
	bba_cmd_out8(0x02,BBA_CMD_IRMASKALL);

	LWIP_DEBUGF(NETIF_DEBUG,("bba_event_handler(status(%02x))\n",status));

	if(status&0x80) {
		LWIP_DEBUGF(NETIF_DEBUG,("bba_event_handler(bba_interrupt(%02x))\n",status));
		bba_cmd_out8(0x03,0x80);
		bba_interrupt(gc_netif);
		return 1;
	}
	if(status&0x40) {
		LWIP_DEBUGF(NETIF_DEBUG,("bba_event_handler(bba_reset(%02x))\n",status));
		bba_cmd_out8(0x03, 0x40);
		__bba_init(gc_netif);
		bba_cmd_out8(0x02, BBA_CMD_IRMASKNONE);
		EXI_Unlock(EXI_CHANNEL_0);
		return 1;
	}
	if(status&0x20) {
		LWIP_DEBUGF(NETIF_DEBUG,("bba_event_handler(unknown(%02x))\n",status));
		bba_cmd_out8(0x03, 0x20);
		bba_cmd_out8(0x02, BBA_CMD_IRMASKNONE);
		EXI_Unlock(EXI_CHANNEL_0);
		return 1;
	}
	if(status&0x10) {
		u32 response,challange;
		LWIP_DEBUGF(NETIF_DEBUG,("bba_event_handler(challange/response(%02x))\n",status));
		bba_cmd_out8(0x05,priv->acstart);
		bba_cmd_ins(0x08,&challange,sizeof(challange));
		response = bba_calc_response(priv,challange);
		bba_cmd_outs(0x09,&response,sizeof(response));

		bba_cmd_out8(0x03, 0x10);
		bba_cmd_out8(0x02, BBA_CMD_IRMASKNONE);
		EXI_Unlock(EXI_CHANNEL_0);
		return 1;
	}
	if(status&0x08) {
		LWIP_DEBUGF(NETIF_DEBUG,("bba_event_handler(challange/response status(%02x))\n",bba_cmd_in8(0x0b)));
		bba_cmd_out8(0x03, 0x08);
		bba_cmd_out8(0x02, BBA_CMD_IRMASKNONE);
		EXI_Unlock(EXI_CHANNEL_0);
		return 1;
	}
	LWIP_ERROR(("GCIF - EXI - ?? %02x\n", status));
	bba_interrupt(gc_netif);
	return 1;
}

static void __arp_timer(void *arg)
{
	etharp_tmr();
	__lwp_wd_insert_ticks(&arp_time_cntrl,net_arp_ticks);
}

err_t bba_init(struct netif *dev)
{
	err_t ret;
	struct timespec tb;
	struct bba_priv *priv;

	LWIP_DEBUGF(NETIF_DEBUG, ("bba_init()\n"));

	priv = mem_malloc(sizeof(struct bba_priv));
	if(!priv) {
		LWIP_ERROR(("bba_init: out of memory for bba_priv\n"));
		return ERR_MEM;
	}

	dev->name[0] = IFNAME0;
	dev->name[1] = IFNAME1;
	dev->output = bba_start_tx;
	dev->linkoutput = __bba_link_tx;
	dev->state = priv;
	dev->mtu = 1536;
	dev->flags = NETIF_FLAG_BROADCAST;
	dev->hwaddr_len = 6;

	priv->ethaddr = (struct eth_addr*)&(dev->hwaddr[0]);
	priv->state = ERR_OK;
	
	gc_netif = dev;

	LWP_InitQueue(&wait_exi_queue);
	LWP_InitQueue(&wait_tx_queue);	

	_siReg[15] = (_siReg[15]&~0x0001);
	
	__bba_exi_wait();

	ret = bba_probe(dev);
	if(ret!=ERR_OK) {
		EXI_Unlock(EXI_CHANNEL_0);
		return ret;
	}
	LWIP_DEBUGF(NETIF_DEBUG, ("bba_init(call EXI_RegisterEXICallback())\n"));
	EXI_RegisterEXICallback(EXI_CHANNEL_2,bba_event_handler);

	EXI_Unlock(EXI_CHANNEL_0);

	do {
		udelay(20000);
		LWIP_DEBUGF(NETIF_DEBUG, ("bba_init(wait link state)\n"));
	} while(!__bba_getlink_state_async());

	etharp_init();

	tb.tv_sec = ARP_TMR_INTERVAL/TB_MSPERSEC;
	tb.tv_nsec = 0;
	net_arp_ticks = timespec_to_interval(&tb);
	__lwp_wd_initialize(&arp_time_cntrl,__arp_timer,NULL);
	__lwp_wd_insert_ticks(&arp_time_cntrl,net_arp_ticks);

	return ERR_OK;
}
