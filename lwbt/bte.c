#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>

#include "bt.h"
#include "bte.h"
#include "hci.h"
#include "l2cap.h"
#include "btmemb.h"
#include "physbusif.h"


#define STACKSIZE					32768

struct bt_state
{
	err_t last_err;
	struct bd_addr bdaddr;

	syswd_t timer_svc;
	lwpq_t hci_cmdq;
	u8_t hci_opdone;
	u8_t hci_inited;

	u8_t num_founddevs;
	struct inquiry_info *info;
};

static struct bt_state btstate;
static u8 ppc_stack[STACKSIZE] ATTRIBUTE_ALIGN(8);

err_t link_key_not(void *arg,struct bd_addr *bdaddr,u8_t *key);
err_t pin_req(void *arg,struct bd_addr *bdaddr);
err_t l2cap_connected(void *arg,struct l2cap_pcb *l2cappcb,u16_t result,u16_t status);
err_t acl_conn_complete(void *arg,struct bd_addr *bdaddr);
err_t l2cap_disconnected_ind(void *arg, struct l2cap_pcb *pcb, err_t err);
err_t read_bdaddr_complete(void *arg,struct bd_addr *bdaddr);
err_t bte_input(void *arg,struct l2cap_pcb *pcb,struct pbuf *p,err_t err);
err_t bte_start_cmd_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
err_t bte_inquiry_complete(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result);

MEMB(bte_pcbs,sizeof(struct bte_pcb),MEMP_NUM_BTE_PCB);

static void bt_alarmhandler(syswd_t alarm)
{
	SYS_SwitchFiber(0,0,0,0,(u32)l2cap_tmr,(u32)(&ppc_stack[STACKSIZE]));
}

static void bte_waitcmdfinish()
{
	u32 level;

	_CPU_ISR_Disable(level);
	while(!btstate.hci_opdone)
		LWP_ThreadSleep(btstate.hci_cmdq);
	btstate.hci_opdone = 0;
	_CPU_ISR_Restore(level);
}

static void bte_cmdfinish(err_t err)
{
	u32 level;

	_CPU_ISR_Disable(level);
	btstate.last_err = err;
	btstate.hci_opdone = 1;
	LWP_ThreadSignal(btstate.hci_cmdq);
	_CPU_ISR_Restore(level);
}

static void bte_reset_all()
{
	if(btstate.info!=NULL) free(btstate.info);

	memset(&(btstate.bdaddr),0,sizeof(struct bd_addr));
	
	btstate.info = NULL;
	btstate.hci_inited = 0;
	btstate.hci_opdone = 0;
	btstate.num_founddevs = 0;
	btstate.last_err = ERR_OK;
}

static void bte_restart()
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	bte_reset_all();
	hci_reset_all();
	l2cap_reset_all();
	physbusif_reset_all();
	
	hci_cmd_complete(bte_start_cmd_complete);
	hci_reset();
	_CPU_ISR_Restore(level);
}

void bte_init()
{
	struct timespec tb;

	memset(&btstate,0,sizeof(struct bt_state));
	
	btmemb_init(&bte_pcbs);

	hci_init();
	l2cap_init();
	physbusif_init();

	LWP_InitQueue(&btstate.hci_cmdq);
	SYS_CreateAlarm(&btstate.timer_svc);

	tb.tv_sec = 1;
	tb.tv_nsec = 0;
	SYS_SetPeriodicAlarm(btstate.timer_svc,&tb,&tb,bt_alarmhandler);
}

s32 bte_start()
{
	if(btstate.hci_inited==1) return ERR_OK;

	bte_restart();
	bte_waitcmdfinish();

	return (s32)btstate.last_err;
}

struct bte_pcb* bte_new()
{
	struct bte_pcb *pcb;

	if((pcb=btmemb_alloc(&bte_pcbs))==NULL) return NULL;

	memset(pcb,0,sizeof(struct bte_pcb));

	return pcb;
}

s32 bte_inquiry(struct inquiry_info *info,u16 max_cnt,u16 timeout,u8 flush)
{
	s32_t i;
	err_t last_err;
	u8_t timeout_inq;

	last_err = ERR_OK;
	if(btstate.num_founddevs==0 || flush==1) {
		timeout_inq = (u8_t)((f32)timeout/1.28F)+1;
		hci_inquiry(0x009E8B33,timeout_inq,0x00,bte_inquiry_complete);
		bte_waitcmdfinish();

		last_err = btstate.last_err;
	}

	if(last_err==ERR_OK && btstate.num_founddevs>0) {
		for(i=0;i<btstate.num_founddevs && i<max_cnt;i++) {
			bd_addr_set(&(info[i].bdaddr),&(btstate.info[i].bdaddr));
			memcpy(info[i].cod,btstate.info[i].cod,3);
		}
	}
	return (s32)((last_err==ERR_OK) ? btstate.num_founddevs : last_err);
}

s32 bte_connect(struct bte_pcb *pcb,struct bd_addr *bdaddr,u8 psm,s32 (*recv)(void *arg,void *buffer,u16 len))
{
	err_t err = ERR_OK;

	if(pcb==NULL) return ERR_VAL;

	if((pcb->l2capcb=l2cap_new())==NULL) return ERR_MEM;

	pcb->psm = psm;
	pcb->recv = recv;
	bd_addr_set(&(pcb->bdaddr),bdaddr);

	l2cap_arg(pcb->l2capcb,pcb);
	hci_connection_complete(acl_conn_complete);
	err = l2ca_connect_req(pcb->l2capcb,bdaddr,psm,0,l2cap_connected);
	if(err!=ERR_OK) return err;

	bte_waitcmdfinish();
	return (s32)btstate.last_err;
}

s32 bte_sendmessage(struct bte_pcb *pcb,void *message,u16 len)
{
	err_t err;
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,len,PBUF_RAM))==NULL) {
		return ERR_MEM;
	}
	
	memcpy(p->payload,message,len);

	//hexdump(p->payload,p->tot_len);	
	err = l2ca_datawrite(pcb->l2capcb,p);
	btpbuf_free(p);

	return (s32)err;
}

void bte_arg(struct bte_pcb *pcb,void *arg)
{
	pcb->cbarg = arg;
}

err_t read_bdaddr_complete(void *arg,struct bd_addr *bdaddr)
{
	memcpy(&(btstate.bdaddr),bdaddr,6);
	return ERR_OK;
}

err_t acl_wlp_completed(void *arg,struct bd_addr *bdaddr)
{
	hci_wlp_complete(NULL);
	//hci_sniff_mode(bdaddr,200,100,10,10);
	return ERR_OK;
}

err_t acl_conn_complete(void *arg,struct bd_addr *bdaddr)
{
	printf("acl_conn_complete\n");
	//memcpy(&(btstate.acl_bdaddr),bdaddr,6);

	hci_connection_complete(NULL);
	hci_wlp_complete(acl_wlp_completed);
	hci_write_link_policy_settings(bdaddr,0x000f);
	return ERR_OK;
}

err_t pin_req(void *arg,struct bd_addr *bdaddr)
{
	//printf("pin_req\n");
	return ERR_OK;
}

err_t l2cap_disconnected_ind(void *arg, struct l2cap_pcb *pcb, err_t err)
{
	//printf("l2cap_disconnected_ind\n");
	return ERR_OK;
}

err_t link_key_not(void *arg,struct bd_addr *bdaddr,u8_t *key)
{
	//printf("link_key_not\n");
	return hci_write_stored_link_key(bdaddr,key);
}

err_t bte_input(void *arg,struct l2cap_pcb *pcb,struct pbuf *p,err_t err)
{
	struct bte_pcb *btepcb = (struct bte_pcb*)arg;

	if(btepcb->recv!=NULL) btepcb->recv(btepcb->cbarg,p->payload,p->tot_len);

	return ERR_OK;
}

err_t l2cap_connected(void *arg,struct l2cap_pcb *l2cappcb,u16_t result,u16_t status)
{
	if(result==L2CAP_CONN_SUCCESS) {
		//printf("l2cap_connected\n");
		l2cap_disconnect_ind(l2cappcb,l2cap_disconnected_ind);
		l2cap_recv(l2cappcb,bte_input);
		bte_cmdfinish(ERR_OK);
	} else {
		l2cap_close(l2cappcb);
		bte_restart();
		bte_cmdfinish(ERR_CONN);
	}
	return ERR_OK;
}

err_t bte_inquiry_complete(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result)
{
	u8_t i;
	struct hci_inq_res *p;

	if(result==HCI_SUCCESS) {
		//printf("inquiry_complete\n");
		if(ires!=NULL) {
			//printf("inquiry_complete(cod = 0x%02x%02x%02x)\n",ires->cod[0],ires->cod[1],ires->cod[2]);
			if(btstate.info!=NULL) free(btstate.info);
			btstate.info = NULL;
			btstate.num_founddevs = 0;

			p = ires;
			while(p!=NULL) {
				btstate.num_founddevs++;
				p = p->next;
			}

			p = ires;
			btstate.info = (struct inquiry_info*)malloc(sizeof(struct inquiry_info)*btstate.num_founddevs);
			for(i=0;i<btstate.num_founddevs && p!=NULL;i++) {
				bd_addr_set(&(btstate.info[i].bdaddr),&(p->bdaddr));
				memcpy(btstate.info[i].cod,p->cod,3);
				p = p->next;
			}
			bte_cmdfinish(ERR_OK);
		}
	}
	return ERR_OK;
}

err_t bte_start_cmd_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err = ERR_OK;
	u8_t cod_rend_periph[] = {0x00,0x05,0x00,0x00,0x05,0x00};

	switch(ogf) {
		case HCI_INFO_PARAM:
			if(ocf==HCI_READ_BUFFER_SIZE) {
				if(result==HCI_SUCCESS) {
					hci_read_bd_addr(read_bdaddr_complete);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_BD_ADDR) {
				if(result==HCI_SUCCESS) {
					hci_set_event_filter(0x01,0x01,cod_rend_periph);
				} else
					err = ERR_CONN;
			}			
			break;
		case HCI_HC_BB_OGF:
			if(ocf==HCI_RESET) {
				if(result==HCI_SUCCESS) {
					hci_read_buffer_size();
				} else 
					err = ERR_CONN;
			} else if(ocf==HCI_SET_EVENT_FILTER) {
				if(result==HCI_SUCCESS) {
					hci_write_page_timeout(0x8000);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PAGE_TIMEOUT) {
				if(result==HCI_SUCCESS) {
					btstate.hci_inited = 1;
					hci_cmd_complete(NULL);
					bte_cmdfinish(ERR_OK);
				} else
					err = ERR_CONN;
			}
			break;
		default:
			//LOG("Unknown command complete event. OGF = 0x%x OCF = 0x%x\n", ogf, ocf);
			break;
	}

	if(err!=ERR_OK) bte_cmdfinish(err);
	return err;
}
