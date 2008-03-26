#ifndef __BTE_H__
#define __BTE_H__

#include "bd_addr.h"

#define ERR_OK						0
#define ERR_MEM						-1
#define ERR_BUF						-2
#define ERR_ABRT					-3
#define ERR_RST						-4
#define ERR_CLSD					-5
#define ERR_CONN					-6
#define ERR_VAL						-7
#define ERR_ARG						-8
#define ERR_RTE						-9
#define ERR_USE						-10
#define ERR_IF						-11
#define ERR_PKTSIZE					-17

#define HIDP_HDR_TRANS_MASK			0xf0
#define HIDP_HDR_PARAM_MASK			0x0f

#define HIDP_TRANS_HANDSHAKE		0x00
#define HIDP_TRANS_HIDCONTROL		0x10
#define HIDP_TRANS_GETREPORT		0x40
#define HIDP_TRANS_SETREPORT		0x50
#define HIDP_TRANS_GETPROTOCOL		0x60
#define HIDP_TRANS_SETPROTOCOL		0x70
#define HIDP_TRANS_GETIDLE			0x80
#define HIDP_TRANS_SETIDLE			0x90
#define HIDP_TRANS_DATA				0xa0
#define HIDP_TRANS_DATAC			0xb0

#define HIDP_HSHK_SUCCESSFULL		0x00
#define HIDP_HSHK_NOTREADY			0x01
#define HIDP_HSHK_INV_REPORTID		0x02
#define HIDP_HSHK_NOTSUPPORTED		0x03
#define HIDP_HSHK_IVALIDPARAM		0x04
#define HIDP_HSHK_UNKNOWNERROR		0x0e
#define HIDP_HSHK_FATALERROR		0x0f

#define HIDP_CTRL_NOP				0x00
#define HIDP_CTRL_HARDRESET			0x01
#define HIDP_CTRL_SOFTRESET			0x02
#define HIDP_CTRL_SUSPEND			0x03
#define HIDP_CTRL_RESUME			0x04
#define HIDP_CTRL_VC_UNPLUG			0x05

/* HIDP data transaction headers */
#define HIDP_DATA_RTYPE_MASK        0x03
#define HIDP_DATA_RSRVD_MASK        0x0c
#define HIDP_DATA_RTYPE_OTHER       0x00
#define HIDP_DATA_RTYPE_INPUT       0x01
#define HIDP_DATA_RTYPE_OUPUT       0x02
#define HIDP_DATA_RTYPE_FEATURE     0x03

#define HIDP_PROTO_BOOT				0x00
#define HIDP_PROTO_REPORT			0x01

struct l2cap_pcb;

struct inquiry_info
{
	struct bd_addr bdaddr;
	u8 cod[3];
};

struct bte_pcb
{
	u8 psm;
	void *cbarg;

	struct bd_addr bdaddr;
	struct l2cap_pcb *l2capcb;

	s32 (*recv)(void *arg,void *buffer,u16 len);
};

void bte_init();
s32 bte_start();
struct bte_pcb* bte_new();
void bte_arg(struct bte_pcb *pcb,void *arg);
s32 bte_inquiry(struct inquiry_info *info,u16 max_cnt,u16 timeout,u8 flush);
s32 bte_connect(struct bte_pcb *pcb,struct bd_addr *bdaddr,u8 psm,s32 (*recv)(void *arg,void *buffer,u16 len));
s32 bte_sendmessage(struct bte_pcb *pcb,void *message,u16 len);

#endif

