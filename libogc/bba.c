// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <tuxedo/tick.h>
#include <tuxedo/sync.h>
#include <gcutil.h>
#include <ogc/bba.h>

//#define BBA_DEBUG

#ifdef BBA_DEBUG
#include <ogc/system.h>
#else
#define kprintf(...) ((void)0)
#endif

#define BBA_EXI_CH      EXI_CHANNEL_0
#define BBA_EXI_IRQ_CH  EXI_CHANNEL_2
#define BBA_EXI_DEVICE  EXI_DEVICE_2
#define BBA_EXI_SPEED   EXI_SPEED32MHZ
#define BBA_EXI_CID     0x04020200

#define BBA_DEVID       0xd107

#define BBA_TXBUF_START 0x00
#define BBA_RXBUF_START 0x01
#define BBA_RXBUF_END   0x0f

enum {
	BBA_CMD_ID        = 0x00,
	BBA_CMD_REVID     = 0x01,
	BBA_CMD_IRQ_MASK  = 0x02,
	BBA_CMD_IRQ_STAT  = 0x03,
	BBA_CMD_DEVID     = 0x04,
	BBA_CMD_START     = 0x05,
	BBA_CMD_CHALLENGE = 0x08,
	BBA_CMD_RESPONSE  = 0x09,
	BBA_CMD_CHRESULT  = 0x0b,
	BBA_CMD_INIT      = 0x0f,
};

enum {
	BBA_IM_GMAC      = 0x80,
	BBA_IM_UNK6      = 0x40,
	BBA_IM_UNK5      = 0x20,
	BBA_IM_CHALLENGE = 0x10,
	BBA_IM_CHRESULT  = 0x08,

	BBA_IM_ALL       = 0xf8,
};

enum {
	BBA_GMAC_NCRA      = 0x00,
	BBA_GMAC_NCRB      = 0x01,
	BBA_GMAC_TRA       = 0x02,
	BBA_GMAC_TRB       = 0x03,
	BBA_GMAC_LTPS      = 0x04,
	BBA_GMAC_LRPS      = 0x05,
	BBA_GMAC_MPC       = 0x06,
	BBA_GMAC_IMR       = 0x08,
	BBA_GMAC_IR        = 0x09,
	BBA_GMAC_BP        = 0x0a,
	BBA_GMAC_TLBP      = 0x0c,
	BBA_GMAC_TWP       = 0x0e,
	BBA_GMAC_TRP       = 0x12,
	BBA_GMAC_RXINTT    = 0x14,
	BBA_GMAC_RWP       = 0x16,
	BBA_GMAC_RRP       = 0x18,
	BBA_GMAC_RHBP      = 0x1a,
	BBA_GMAC_EEPROM    = 0x1c,
	BBA_GMAC_BICT      = 0x1d,
	BBA_GMAC_IORDP     = 0x1e,
	BBA_GMAC_PAR       = 0x20,
	BBA_GMAC_MAR       = 0x26,
	BBA_GMAC_ANALOG    = 0x2e,
	BBA_GMAC_DINTVAL   = 0x2f,
	BBA_GMAC_NWAYC     = 0x30,
	BBA_GMAC_NWAYS     = 0x31,
	BBA_GMAC_GCA       = 0x32,
	BBA_GMAC_GCB       = 0x33,
	BBA_GMAC_TWD       = 0x34,
	BBA_GMAC_HIPR      = 0x3a,
	BBA_GMAC_LPC       = 0x3b,
	BBA_GMAC_DMASR     = 0x3c,
	BBA_GMAC_MISC1     = 0x3d,
	BBA_GMAC_TXFIFOCNT = 0x3e,
	BBA_GMAC_RRD       = 0x40,
	BBA_GMAC_ID1       = 0x44,
	BBA_GMAC_ID2       = 0x46,
	BBA_GMAC_WRTXFIFOD = 0x48,
	BBA_GMAC_IORD      = 0x4c,
	BBA_GMAC_MISC2     = 0x50,
	BBA_GMAC_HRPKTCNT  = 0x52,
	BBA_GMAC_FRAGCNT   = 0x54,

	BBA_GMAC_UNK58         = 0x58,
	BBA_GMAC_UNK59         = 0x59,
	BBA_GMAC_UNK5A         = 0x5a,
	BBA_GMAC_UNK5B         = 0x5b,
	BBA_GMAC_UNK_NWAY_CFG  = 0x5c,
	BBA_GMAC_UNK5D         = 0x5d,
	BBA_GMAC_UNK_NWAY_MODE = 0x5e,
	BBA_GMAC_UNK_NWAY_PEER = 0x5f, // seems to be peer link mode support
	BBA_GMAC_UNK60         = 0x60,
};

enum {
	BBA_NCRA_RESET         = 0x01,
	BBA_NCRA_START_TX_DMA  = 0x02,
	BBA_NCRA_START_TX_FIFO = 0x04,
	BBA_NCRA_START_RX      = 0x08,
};

enum {
	BBA_NCRB_PR       = 0x01,
	BBA_NCRB_CA       = 0x02,
	BBA_NCRB_PM       = 0x04,
	BBA_NCRB_PB       = 0x08,
	BBA_NCRB_AB       = 0x10,
	BBA_NCRB_RXINTC   = 0xc0,
	BBA_NCRB_RXINTC_1 = 0x00,
	BBA_NCRB_RXINTC_2 = 0x40,
	BBA_NCRB_RXINTC_4 = 0x80,
	BBA_NCRB_RXINTC_8 = 0xc0,
};

enum {
	BBA_GMAC_IM_FRAG    = 0x01,
	BBA_GMAC_IM_RX      = 0x02,
	BBA_GMAC_IM_TX      = 0x04,
	BBA_GMAC_IM_RXERR   = 0x08,
	BBA_GMAC_IM_TXERR   = 0x10,
	BBA_GMAC_IM_FIFOERR = 0x20,
	BBA_GMAC_IM_BUSERR  = 0x40,
	BBA_GMAC_IM_RXFULL  = 0x80,
	BBA_GMAC_IM_ALL     = 0xff,
};

enum {
	BBA_GCA_BPSCRM      = 0x01,
	BBA_GCA_PBW         = 0x02,
	BBA_GCA_SLOWSRAM    = 0x04,
	BBA_GCA_ARXERRB     = 0x08,
	BBA_GCA_MIISEL      = 0x10,
	BBA_GCA_AUTOPUB     = 0x20,
	BBA_GCA_TXFIFOCNTEN = 0x40,
};

enum {
	BBA_MISC2_HBRLEN    = 0x03,
	BBA_MISC2_HBRLEN_x1 = 0x01,
	BBA_MISC2_HBRLEN_x2 = 0x02,
	BBA_MISC2_HBRLEN_x4 = 0x03,
	BBA_MISC2_RUNTSIZE  = 0x04,
	BBA_MISC2_DREQBCTRL = 0x08,
	BBA_MISC2_RINTSEL   = 0x10,
	BBA_MISC2_ITPSEL    = 0x20,
	BBA_MISC2_A11A8EN   = 0x40,
	BBA_MISC2_AUTORCVR  = 0x80,
};

typedef union BbaRxDescriptor {
	u32 raw;
	struct {
		u32 status   : 8;
		u32 pkt_len  : 12;
		u32 next_ptr : 12;
	};
} BbaRxDescriptor;

MK_INLINE bool BBA_Lock(void)
{
	return EXI_LockSync(BBA_EXI_CH, BBA_EXI_DEVICE);
}

MK_INLINE void BBA_Unlock(void)
{
	EXI_Unlock(BBA_EXI_CH);
}

MK_INLINE s32 BBA_ExiSelect(void)
{
	return EXI_Select(BBA_EXI_CH, BBA_EXI_DEVICE, BBA_EXI_SPEED);
}

MK_INLINE s32 BBA_ExiDeselect(void)
{
	return EXI_Deselect(BBA_EXI_CH);
}

MK_NOINLINE static void BBA_CmdInit(void)
{
	u8 ret;
	u16 hdr = BBA_CMD_INIT << 8;

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 2, EXI_WRITE);
	EXI_ImmEx(BBA_EXI_CH, &ret, 1, EXI_READ);
	PPCBusyWaitUs(200);
	BBA_ExiDeselect();
}

MK_NOINLINE static void BBA_CmdRead(unsigned cmd, void* buf, u32 len)
{
	u16 hdr = cmd << 8;

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 2, EXI_WRITE);
	EXI_ImmEx(BBA_EXI_CH, buf, len, EXI_READ);
	BBA_ExiDeselect();
}

MK_NOINLINE static void BBA_CmdWrite(unsigned cmd, const void* buf, u32 len)
{
	u16 hdr = 0x4000 | (cmd << 8);

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 2, EXI_WRITE);
	EXI_ImmEx(BBA_EXI_CH, (void*)buf, len, EXI_WRITE);
	BBA_ExiDeselect();
}

MK_INLINE u8 BBA_CmdRead8(unsigned reg)
{
	u8 val;
	BBA_CmdRead(reg, &val, 1);
	return val;
}

MK_INLINE void BBA_CmdWrite8(unsigned reg, u8 val)
{
	BBA_CmdWrite(reg, &val, 1);
}

MK_INLINE void BBA_CmdWrite16(unsigned reg, u16 val)
{
	BBA_CmdWrite(reg, &val, 2);
}

MK_NOINLINE static void BBA_GmacRead(unsigned reg, void* buf, u32 len)
{
	u32 hdr = 0x80000000 | (reg << 8);

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 4, EXI_WRITE);
	EXI_ImmEx(BBA_EXI_CH, buf, len, EXI_READ);
	BBA_ExiDeselect();
}

MK_NOINLINE static void BBA_GmacReadDma(unsigned reg, void* buf, u32 len)
{
	u32 hdr = 0x80000000 | (reg << 8);

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 4, EXI_WRITE);
	EXI_Transfer(BBA_EXI_CH, buf, len, EXI_READ);
	BBA_ExiDeselect();
}

MK_NOINLINE static void BBA_GmacWrite(unsigned reg, const void* buf, u32 len)
{
	u32 hdr = 0xC0000000 | (reg << 8);

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 4, EXI_WRITE);
	EXI_ImmEx(BBA_EXI_CH, (void*)buf, len, EXI_WRITE);
	BBA_ExiDeselect();
}

MK_INLINE u8 BBA_GmacRead8(unsigned reg)
{
	u8 val;
	BBA_GmacRead(reg, &val, 1);
	return val;
}

MK_INLINE u16 BBA_GmacRead16(unsigned reg)
{
	u16 val;
	BBA_GmacRead(reg, &val, 2);
	return __builtin_bswap16(val);
}

MK_INLINE u32 BBA_GmacRead32(unsigned reg)
{
	u32 val;
	BBA_GmacRead(reg, &val, 4);
	return __builtin_bswap32(val);
}

MK_INLINE u16 BBA_GmacRead12(unsigned reg)
{
	return BBA_GmacRead16(reg) & 0xfff;
}

MK_INLINE void BBA_GmacWrite8(unsigned reg, u8 val)
{
	BBA_GmacWrite(reg, &val, 1);
}

MK_INLINE void BBA_GmacWrite16(unsigned reg, u16 val)
{
	val = __builtin_bswap16(val);
	BBA_GmacWrite(reg, &val, 2);
}

MK_INLINE void BBA_GmacWrite12(unsigned reg, u16 val)
{
	BBA_GmacWrite16(reg, val & 0xfff);
}

//-----------------------------------------------------------------------------

u8 g_bbaMac[6];

static bool s_bbaInit;
static bool s_bbaRxFull;
static u8 s_bbaRevid;
static u8 s_bbaCmdIrqMask;
static bool s_bbaPendingIrq;
static BbaBeginRxCb s_bbaRxBegin;

static bool s_bbaTxActive;
static BbaBuffer *s_bbaTxHead, *s_bbaTxTail;

static KThread s_bbaThread;

static void BBA_CmdSetIrqMask(unsigned mask)
{
	BBA_CmdWrite8(BBA_CMD_IRQ_MASK, s_bbaCmdIrqMask = mask);
}

bool BBA_Init(void)
{
	if (s_bbaInit) {
		return true;
	}

	u32 id;
	if (!EXI_GetID(BBA_EXI_CH, BBA_EXI_DEVICE, &id) || id != BBA_EXI_CID) {
		return false;
	}

	if (!BBA_Lock()) {
		return false;
	}

	BBA_GmacWrite8(BBA_GMAC_UNK60, 0x00);
	PPCBusyWaitMs(10);
	BBA_CmdInit();
	PPCBusyWaitMs(10);
	BBA_GmacWrite8(BBA_GMAC_NCRA, BBA_NCRA_RESET);
	BBA_GmacWrite8(BBA_GMAC_NCRA, 0);
	// XX: do PHY shit here?
	BBA_GmacWrite8(BBA_GMAC_UNK60, 0x10);

	BBA_GmacRead(BBA_GMAC_PAR, g_bbaMac, 6);

	s_bbaRevid = BBA_CmdRead8(BBA_CMD_REVID);
	kprintf("[BBA] RevID: %02X\n", s_bbaRevid);
	BBA_CmdWrite16(BBA_CMD_DEVID, BBA_DEVID);

	BBA_CmdWrite8(BBA_CMD_START, 0x4e);

	BBA_GmacWrite8(BBA_GMAC_UNK5B, BBA_GmacRead8(BBA_GMAC_UNK5B) &~ 0x80);

	u8 curmode = BBA_GmacRead8(BBA_GMAC_UNK_NWAY_MODE);
	u8 curpeer = BBA_GmacRead8(BBA_GMAC_UNK_NWAY_PEER);
	kprintf("[BBA] Mode: %02X; Peer: %02X\n", curmode, curpeer);

	if (!((curmode|curpeer) & 0x10) || ((curpeer &~ curmode) & 0xf)) {
		kprintf("[BBA] Inefficient mode detected, renegotiating\n");
		BBA_GmacWrite8(BBA_GMAC_UNK_NWAY_MODE, 0x1f);
		BBA_GmacWrite8(BBA_GMAC_UNK_NWAY_CFG, BBA_GmacRead8(BBA_GMAC_UNK_NWAY_CFG) | 4);
	}

	BBA_GmacWrite8(BBA_GMAC_NCRB, BBA_NCRB_CA|BBA_NCRB_PM|BBA_NCRB_AB|BBA_NCRB_RXINTC_2);
	BBA_GmacWrite8(BBA_GMAC_TRB, 0x2e);

	BBA_GmacWrite8(BBA_GMAC_MISC2, BBA_MISC2_AUTORCVR);
	BBA_GmacWrite8(BBA_GMAC_IMR, 0xff);
	BBA_CmdSetIrqMask(BBA_IM_ALL);

	BBA_GmacWrite12(BBA_GMAC_TLBP, BBA_TXBUF_START);
	BBA_GmacWrite12(BBA_GMAC_BP,   BBA_RXBUF_START);
	BBA_GmacWrite12(BBA_GMAC_RWP,  BBA_RXBUF_START);
	BBA_GmacWrite12(BBA_GMAC_RRP,  BBA_RXBUF_START);
	BBA_GmacWrite12(BBA_GMAC_RHBP, BBA_RXBUF_END);

	BBA_GmacWrite16(BBA_GMAC_RXINTT, BBA_GCA_ARXERRB);

	BBA_GmacWrite8(BBA_GMAC_NCRA, BBA_NCRA_START_RX);

	BBA_Unlock();

	s_bbaInit = true;
	return true;
}

static BbaLinkState BBA_GetLinkStateImpl(void)
{
	u8 nways = BBA_GmacRead8(BBA_GMAC_NWAYS);
	if (nways & 3) {
		if (nways & 4) {
			return nways & 0xf0;
		} else if (nways & 2) {
			return BbaLinkState_Up_100HD;
		} else if (nways & 1) {
			return BbaLinkState_Up_10HD;
		}
	}
	return BbaLinkState_Down;
}

BbaLinkState BBA_GetLinkState(void)
{
	if (!s_bbaInit || !BBA_Lock()) {
		return BbaLinkState_Down;
	}

	BbaLinkState ret = BBA_GetLinkStateImpl();

	BBA_Unlock();
	return ret;
}

static u32 BBA_Challenge(u32 chal)
{
	unsigned revid_0     = s_bbaRevid;
	unsigned revid_eth_0 = _SHIFTR(BBA_DEVID, 8, 8);
	unsigned revid_eth_1 = _SHIFTR(BBA_DEVID, 0, 8);

	unsigned i0 = _SHIFTR(chal, 24, 8);
	unsigned i1 = _SHIFTR(chal, 16, 8);
	unsigned i2 = _SHIFTR(chal,  8, 8);
	unsigned i3 = _SHIFTR(chal,  0, 8);

	u8 c0 = (i0 + i1*0xc1 + 0x18 + revid_0) ^ (i3*i2 + 0x90);
	u8 c1 = (i1 + i2+0x90)                  ^ (c0 + i0 - 0xc1);
	u8 c2 = (i2 + 0xc8)                     ^ (c0 + ((revid_eth_0 + revid_0*0x23) ^ 0x19));
	u8 c3 = (i0 + 0xc1)                     ^ (i3 + ((revid_eth_1 + 0xc8) ^ 0x90));

	return (c0 << 24) | (c1 << 16) | (c2 << 8) | c3;
}

MK_NOINLINE static void BBA_CheckIrqs(void)
{
	unsigned mask = s_bbaCmdIrqMask;
	unsigned stat = BBA_CmdRead8(BBA_CMD_IRQ_STAT) & mask;
	if (!stat) {
		return;
	}

	BBA_CmdWrite8(BBA_CMD_IRQ_MASK, 0);

	unsigned irqid = __builtin_clz(stat);
	unsigned irqbit = PPCBIT(irqid);

	switch (irqbit) {
		default: {
			kprintf("[BBA] IRQ %02X\n", irqbit);
			break;
		}

		case BBA_IM_GMAC: {
			s_bbaPendingIrq = true;
			mask &= ~BBA_IM_GMAC;
			break;
		}

		case BBA_IM_CHALLENGE: {
			u32 chal, chres;

			BBA_CmdWrite8(BBA_CMD_START, 0x4e);
			BBA_CmdRead(BBA_CMD_CHALLENGE, &chal, 4);
			chres = BBA_Challenge(chal);
			kprintf("[BBA] Challenge %08X -> %08X\n", chal, chres);
			BBA_CmdWrite(BBA_CMD_RESPONSE, &chres, 4);
			break;
		}

		case BBA_IM_CHRESULT: {
			kprintf("[BBA] Challenge result %02X\n", BBA_CmdRead8(BBA_CMD_CHRESULT));
			break;
		}
	}

	BBA_CmdWrite8(BBA_CMD_IRQ_STAT, irqbit);
	BBA_CmdSetIrqMask(mask);
}

static void BBA_ProcessRx(void)
{
	for (;;) {
		unsigned rwp = BBA_GmacRead12(BBA_GMAC_RWP);
		unsigned rrp = BBA_GmacRead12(BBA_GMAC_RRP);
		if (rwp == rrp) {
			break;
		}

		do {
			BbaRxDescriptor descr = { BBA_GmacRead32(rrp<<8) };
			unsigned len = descr.pkt_len - 4; // Not counting trailing CRC32

			if (s_bbaRxBegin) do {
				BbaBuffer buf = {0};
				if (!s_bbaRxBegin(&buf, len)) {
					break;
				}

				unsigned readpos = (rrp<<8) + 4;
				unsigned rxbufend = (BBA_RXBUF_END+1)<<8;

				if (readpos + len <= rxbufend) {
					BBA_GmacReadDma(readpos, buf.mem, len);
				} else {
					unsigned temp = rxbufend - readpos;
					BBA_GmacReadDma(readpos, buf.mem, temp);
					BBA_GmacReadDma(BBA_RXBUF_START<<8, (u8*)buf.mem+temp, len-temp);
				}

				buf.cb(&buf, true);
			} while (0);

			rrp = descr.next_ptr;
			rwp = BBA_GmacRead12(BBA_GMAC_RWP);
		} while (rwp != rrp);

		BBA_GmacWrite12(BBA_GMAC_RRP, rrp);
		BBA_GmacWrite8(BBA_GMAC_IR, BBA_GMAC_IM_RX);
	}

	if (s_bbaRxFull) {
		s_bbaRxFull = false;
		BBA_GmacWrite8(BBA_GMAC_IR, BBA_GMAC_IM_RXFULL);
		BBA_GmacWrite8(BBA_GMAC_IMR, BBA_GMAC_IM_ALL);
	}
}

static void BBA_TxBegin(void)
{
	BbaBuffer* buf = s_bbaTxHead;

	// Cancel all pending TXs if link is down
	if (BBA_GetLinkStateImpl() == BbaLinkState_Down) {
		s_bbaTxHead = NULL;
		s_bbaTxTail = NULL;

		while (buf) {
			BbaBuffer* next = buf->next;
			buf->cb(buf, false);
			buf = next;
		}

		return;
	}

	u32 hdr = 0xC0000000 | (BBA_GMAC_WRTXFIFOD << 8);

	BBA_ExiSelect();
	EXI_ImmEx(BBA_EXI_CH, &hdr, 4, EXI_WRITE);
	EXI_Transfer(BBA_EXI_CH, buf->mem, buf->size, EXI_WRITE);
	if (buf->size < 60) {
		EXI_Transfer(BBA_EXI_CH, (void*)0x80000100, 60 - buf->size, EXI_WRITE);
	}
	BBA_ExiDeselect();

	BBA_GmacWrite8(BBA_GMAC_NCRA, BBA_GmacRead8(BBA_GMAC_NCRA) | BBA_NCRA_START_TX_FIFO);
	s_bbaTxActive = true;
}

static void BBA_TxEnd(bool ok)
{
	if (!s_bbaTxActive) {
		return;
	}

	BbaBuffer* buf = s_bbaTxHead;
	s_bbaTxHead = buf->next;
	if (!s_bbaTxHead) {
		s_bbaTxTail = NULL;
	}
	s_bbaTxActive = false;

	buf->cb(buf, ok);
}

static void BBA_ProcessIrqs(void)
{
	unsigned mask = BBA_GmacRead8(BBA_GMAC_IMR);
	unsigned stat = BBA_GmacRead8(BBA_GMAC_IR) & mask;

	if (stat & BBA_GMAC_IM_RXFULL) {
		kprintf("[BBA] RX full irq\n");
		BBA_GmacWrite8(BBA_GMAC_IMR, BBA_GMAC_IM_ALL &~ BBA_GMAC_IM_RXFULL);
		s_bbaRxFull = true;
		stat &= ~BBA_GMAC_IM_RXFULL; // Handled by BBA_ProcessRx
	}

	if (stat & BBA_GMAC_IM_RX) {
		BBA_ProcessRx();
		stat &= ~BBA_GMAC_IM_RX; // Handled by BBA_ProcessRx
	}

	if (stat & BBA_GMAC_IM_RXERR) {
		kprintf("[BBA] RX error irq\n");
	}

	if (stat & (BBA_GMAC_IM_TX|BBA_GMAC_IM_TXERR)) {
		BBA_TxEnd(!(stat & BBA_GMAC_IM_TXERR));
	}

	if (stat & BBA_GMAC_IM_FRAG) {
		kprintf("[BBA] Fragment counter irq\n");
	}

	if (stat & BBA_GMAC_IM_FIFOERR) {
		kprintf("[BBA] FIFO error irq\n");
	}

	if (stat & BBA_GMAC_IM_BUSERR) {
		kprintf("[BBA] Bus error irq\n");
	}

	if (stat) {
		BBA_GmacWrite8(BBA_GMAC_IR, stat);
	}

	BBA_CmdWrite8(BBA_CMD_IRQ_STAT, BBA_IM_GMAC);
	BBA_CmdSetIrqMask(BBA_IM_ALL);
}

MK_NOINLINE static void BBA_ProcessEvents(bool do_irqs)
{
	if (do_irqs) {
		BBA_ProcessIrqs();
	}

	if (!s_bbaTxActive && s_bbaTxHead) {
		BBA_TxBegin();
	}
}

MK_INLINE void BBA_FetchProcessEvents(void)
{
	bool do_irqs = s_bbaPendingIrq;
	s_bbaPendingIrq = false;
	BBA_ProcessEvents(do_irqs);
}

static s32 BBA_ExiCallback(s32 nChn, s32 nDev)
{
	if (!EXI_Lock(BBA_EXI_CH, BBA_EXI_DEVICE, BBA_ExiCallback)) {
		return 0;
	}

	BBA_CheckIrqs();

	if (!s_bbaPendingIrq) {
		// Unlock if there are no IRQs
		BBA_Unlock();
	} else if (s_bbaThread.suspend) {
		// Resume the worker thread if it is suspended
		KThreadResume(&s_bbaThread);
	}

	return 1;
}

static sptr BBA_ThreadMain(void* unused)
{
	EXI_RegisterEXICallback(BBA_EXI_IRQ_CH, BBA_ExiCallback);

	for (;;) {
		{
			PPCIrqState st = PPCIrqLockByMsr();
			while (!s_bbaPendingIrq) {
				KThreadSuspend(KThreadGetSelf());
			}

			s_bbaPendingIrq = false;
			PPCIrqUnlockByMsr(st);
		}

		BBA_ProcessEvents(true);
		BBA_Unlock();
	}

	EXI_RegisterEXICallback(BBA_EXI_IRQ_CH, NULL);
	return 0;
}

void BBA_StartThread(void* stack_top, u16 prio)
{
	if (!s_bbaInit || s_bbaThread.state >= KTHR_STATE_RUNNING) {
		return;
	}

	KThreadPrepare(&s_bbaThread, BBA_ThreadMain, NULL, stack_top, prio);
	KThreadResume(&s_bbaThread);
}

void BBA_Poll(void)
{
	if (!s_bbaInit || !BBA_Lock()) {
		return;
	}

	BBA_CheckIrqs();
	BBA_FetchProcessEvents();
	BBA_Unlock();
}

void BBA_OnRxBegin(BbaBeginRxCb cb)
{
	s_bbaRxBegin = cb;
}

bool BBA_TxEnqueue(BbaBuffer* buf)
{
	if (!s_bbaInit || !BBA_Lock()) {
		return false;
	}

	buf->next = NULL;
	if (s_bbaTxTail) {
		s_bbaTxTail->next = buf;
	} else {
		s_bbaTxHead = buf;
	}
	s_bbaTxTail = buf;

	BBA_FetchProcessEvents();
	BBA_Unlock();
	return true;
}
