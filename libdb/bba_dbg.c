#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "exi.h"
#include "uIP/uip_arp.h"

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

struct bba_priv {
	u8 revid;
	u16 devid;
	u8 acstart;
	struct uip_eth_addr ethaddr;
};

#define X(a,b)  b,a
struct bba_descr {
	u32 X(X(next_packet_ptr:12, packet_len:12), status:8);
} __attribute((packed));

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

/* new functions */
#define bba_select()		EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_2,EXI_SPEED32MHZ)
#define bba_deselect()		EXI_Deselect(EXI_CHANNEL_0)

#define bba_in12(reg)		((bba_in8(reg)&0xff)|((bba_in8((reg)+1)&0x0f)<<8))
#define bba_out12(reg,val)	do { \
									bba_out8((reg),(val)&0xff); \
									bba_out8((reg)+1,((val)&0x0f00)>>8); \
							} while(0)

static struct bba_priv bba_device;
static vu32* const _siReg = (u32*)0xCC006400;

static void bba_cmd_ins(u32 reg,void *val,u32 len);
static void bba_cmd_outs(u32 reg,void *val,u32 len);
static void bba_ins(u32 reg,void *val,u32 len);
static void bba_outs(u32 reg,void *val,u32 len);

extern void udelay(int us);

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

static s32 __bba_init(struct bba_priv *dev)
{
	if(!dev) return -1;

	__bba_reset();

	bba_ins(BBA_NAFR_PAR0,dev->ethaddr.addr, 6);

	dev->revid = bba_cmd_in8(0x01);
	
	bba_cmd_outs(0x04,&dev->devid,2);
	bba_cmd_out8(0x05,dev->acstart);

	bba_out8(0x5b, (bba_in8(0x5b)&~0x80));
	bba_out8(0x5e, 0x01);
	bba_out8(0x5c, (bba_in8(0x5c)|0x04));

	bba_out8(BBA_NCRB, 0x00);

	__bba_recv_init();
	
	bba_out8(BBA_IR,0xFF);
	bba_out8(BBA_IMR,0xFF&~BBA_IMR_FIFOEIM);

	bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);

	return 1;
}

static s32 bba_init_one(struct bba_priv *dev)
{
	s32 ret;

	if(!dev) return -1;

	dev->revid = 0x00;
	dev->devid = 0xD107;
	dev->acstart = 0x4E;
	
	ret = __bba_init(dev);
	
	return ret;
}

static s32 bba_probe(struct bba_priv *dev)
{
	s32 ret;
	u32 cid;

	cid = __bba_read_cid();
	if(cid!=BBA_CID) return -1;
	
	ret = bba_init_one(dev);
	return ret;
}

void bba_init()
{
	s32 ret;

	_siReg[15] = (_siReg[15]&~0x0001);

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL)!=1) return;
	
	ret = bba_probe(&bba_device);
	if(ret<0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return;
	}
	EXI_Unlock(EXI_CHANNEL_0);

}

void bba_appcall()
{

}
