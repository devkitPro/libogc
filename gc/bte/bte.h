#ifndef __BTE_H__
#define __BTE_H__

#include <gccore.h>
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

#define HIDP_STATE_READY			0x00
#define HIDP_STATE_LISTEN			0x01
#define HIDP_STATE_CONNECTING		0x02
#define HIDP_STATE_CONNECTED		0x04

#define HIDP_CONTROL_CHANNEL		0x11
#define HIDP_DATA_CHANNEL			0x13

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
#define HIDP_DATA_RTYPE_MASK		0x03
#define HIDP_DATA_RSRVD_MASK		0x0c
#define HIDP_DATA_RTYPE_OTHER		0x00
#define HIDP_DATA_RTYPE_INPUT		0x01
#define HIDP_DATA_RTYPE_OUPUT		0x02
#define HIDP_DATA_RTYPE_FEATURE		0x03

#define HIDP_PROTO_BOOT				0x00
#define HIDP_PROTO_REPORT			0x01

#define BD_LINK_KEY_LEN				16
#define BD_NAME_LEN					248
#define BD_MAX_INQUIRY_DEVS			255

enum pair_mode {
	PAIR_MODE_NORMAL,
	PAIR_MODE_TEMPORARY
};

#ifdef __cplusplus
	extern "C" {
#endif /* __cplusplus */

struct l2cap_pcb;
struct ctrl_req_t;

struct inquiry_info
{
	struct bd_addr bdaddr;
	u8 cod[3];
};

struct inquiry_info_ex
{
	struct bd_addr bdaddr;
	u8 cod[3];
	u8 psrm;
	u8 psm;
	u16 co;
};

struct bte_inquiry_res
{
	u8 count;
	struct inquiry_info_ex *info;
};

struct linkkey_info
{
	struct bd_addr bdaddr;
	u8 key[BD_LINK_KEY_LEN];
};

struct pad_name_info
{
	struct bd_addr bdaddr;
	u8 name[BD_NAME_LEN];
};

struct bte_pcb
{
	u8 err;
	u32 state;
	void *cbarg;

	struct ctrl_req_t *ctrl_req_head;
	struct ctrl_req_t *ctrl_req_tail;

	lwpq_t cmdq;

	struct bd_addr bdaddr;

	struct l2cap_pcb *ctl_pcb;
	struct l2cap_pcb *data_pcb;


	s32 (*recv)(void *arg,void *buffer,u16 len);
	s32 (*conn_req)(void *arg,struct bte_pcb *pcb,struct bd_addr *bdaddr,u8 *cod,u8 link_type,u8 err);
	s32 (*conn_cfm)(void *arg,struct bte_pcb *pcb,u8 err);
	s32 (*disconn_cfm)(void *arg,struct bte_pcb *pcb,u8 err);
};

typedef s32 (*btecallback)(s32 result,void *userdata);

void BTE_Init(void);
void BTE_Shutdown(void);
void BTE_Close(void);
s32 BTE_InitCore(btecallback cb);
s32 BTE_ApplyPatch(btecallback cb);
s32 BTE_InitSub(btecallback cb);
s32 BTE_ReadStoredLinkKey(struct linkkey_info *keys,u8 max_cnt,btecallback cb);
s32 BTE_ReadBdAddr(struct bd_addr *bdaddr, btecallback cb);
s32 BTE_SetEvtFilter(u8 filter_type,u8 filter_cond_type,u8 *cond, btecallback cb);
s32 BTE_ReadRemoteName(struct pad_name_info *padinfo, btecallback cb);
s32 BTE_Inquiry(u8 max_cnt,u8 flush, btecallback cb);
s32 BTE_PeriodicInquiry(u8 max_cnt,u8 flush,btecallback cb);
s32 BTE_ExitPeriodicInquiry(void);
s32 BTE_LinkKeyRequestReply(struct bd_addr *bdaddr,u8 *key);
s32 BTE_LinkKeyRequestNegativeReply(struct bd_addr *bdaddr);
void (*BTE_SetDisconnectCallback(void (*callback)(struct bd_addr *bdaddr,u8 reason)))(struct bd_addr *bdaddr,u8 reason);
void BTE_SetHostSyncButtonCallback(void (*callback)(u32 held));
void BTE_SetConnectionRequestCallback(s8 (*callback)(void *arg,struct bd_addr *bdaddr,u8 *cod,u8 link_type));
void BTE_SetLinkKeyRequestCallback(s8 (*callback)(void *arg,struct bd_addr *bdaddr));
void BTE_SetLinkKeyNotificationCallback(s8 (*callback)(void *arg,struct bd_addr *bdaddr,u8 *key));
u8 BTE_GetPairMode(void);
s32 BTE_WriteStoredLinkKey(struct bd_addr *bdaddr,u8 *key);
s32 BTE_ClearStoredLinkKeys(void);
s32 BTE_DeleteStoredLinkKey(struct bd_addr *bdaddr);

struct bte_pcb* bte_new(void);
void bte_free(struct bte_pcb *pcb);
void bte_arg(struct bte_pcb *pcb,void *arg);
void bte_received(struct bte_pcb *pcb, s32 (*recv)(void *arg,void *buffer,u16 len));
void bte_disconnected(struct bte_pcb *pcb,s32 (disconn_cfm)(void *arg,struct bte_pcb *pcb,u8 err));

s32 bte_disconnect(struct bte_pcb *pcb);

s32 bte_listenasync(struct bte_pcb *pcb,struct bd_addr *bdaddr,s32 (*conn_cfm)(void *arg,struct bte_pcb *pcb,u8 err));
s32 bte_listenasync_step2(struct bte_pcb *pcb,s32 (*conn_cfm)(void *arg,struct bte_pcb *pcb,u8 err));
s32 bte_connectasync(struct bte_pcb *pcb,struct bd_addr *bdaddr,s32 (*conn_cfm)(void *arg,struct bte_pcb *pcb,u8 err));
s32 bte_connectasync_step2(struct bte_pcb *pcb,s32 (*conn_cfm)(void *arg,struct bte_pcb *pcb,u8 err));
s32 bte_senddata(struct bte_pcb *pcb,void *message,u16 len);
s32 bte_sendmessage(struct bte_pcb *pcb,void *message,u16 len);
s32 bte_sendmessageasync(struct bte_pcb *pcb,void *message,u16 len,s32 (*sent)(void *arg,struct bte_pcb *pcb,u8 err));

#ifdef __cplusplus
	}
#endif /* __cplusplus */

#endif

