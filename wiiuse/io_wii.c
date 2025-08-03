#ifdef GEKKO

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <lwp_wkspace.inl>

#include "definitions.h"
#include "wiiuse_internal.h"
#include "events.h"
#include "io.h"

#define MAX_COMMANDS					0x100
#define MAX_WIIMOTES					5

static vu32* const _ipcReg = (u32*)0xCD000000;
static u8 *__queue_buffer[MAX_WIIMOTES] = { 0, 0, 0, 0, 0 };

extern void parse_event(struct wiimote_t *wm);
extern void idle_cycle(struct wiimote_t* wm);
extern void hexdump(void *d, int len);

extern void __SYS_DisablePowerButton(u32 duration);

static __inline__ u32 ACR_ReadReg(u32 reg)
{
	return _ipcReg[reg>>2];
}

static __inline__ void ACR_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg>>2] = val;
}

static s32 __wiiuse_disconnected(void *arg,struct bte_pcb *pcb,u8 err)
{
	struct wiimote_listen_t *wml = (struct wiimote_listen_t*)arg;
	struct wiimote_t *wm = wml->wm;

	if(!wm) return ERR_OK;

	// Disable system power button events for a bit after Wiimote disconnection
	// BT module hardware will trigger power button press in GPIO just like front panel
	// if auth'd Wiimote power button pressed, so this is the only way to disambiguate
	// It sends power button events for ~1s, but if we disable for 200ms, we can get
	// power button event throttling to take care of the rest
	__SYS_DisablePowerButton(200);

	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_BATTERY_CRITICAL));
	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_IR|WIIMOTE_STATE_IR_INIT));
	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_SPEAKER|WIIMOTE_STATE_SPEAKER_INIT));
	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_EXP|WIIMOTE_STATE_EXP_HANDSHAKE|WIIMOTE_STATE_EXP_FAILED));
	WIIMOTE_DISABLE_STATE(wm,(WIIMOTE_STATE_CONNECTED|WIIMOTE_STATE_HANDSHAKE|WIIMOTE_STATE_HANDSHAKE_COMPLETE));

	while(wm->cmd_head) {
		__lwp_queue_append(&wm->cmdq,&wm->cmd_head->node);
		wm->cmd_head = wm->cmd_head->next;
	}
	wm->cmd_tail = NULL;
	
	if(wm->event_cb) wm->event_cb(wm,WIIUSE_DISCONNECT);

	wml->wm = NULL;

	bte_free(wml->sock);
	wml->sock = NULL;
	return ERR_OK;
}

static s32 __wiiuse_receive(void *arg,void *buffer,u16 len)
{
	struct wiimote_listen_t *wml = (struct wiimote_listen_t*)arg;
	struct wiimote_t *wm = wml->wm;

	if(!wm || !buffer || len==0) return ERR_OK;

	WIIUSE_DEBUG("__wiiuse_receive[%02x]",*(char*)buffer);
	wm->event = WIIUSE_NONE;

	memcpy(wm->event_buf,buffer,len);
	memset(&(wm->event_buf[len]),0,(MAX_PAYLOAD - len));
	parse_event(wm);

	if(wm->event!=WIIUSE_NONE) {
		if(wm->event_cb) wm->event_cb(wm,wm->event);
	}

	return ERR_OK;
}

static s32 __wiiuse_connected(void *arg,struct bte_pcb *pcb,u8 err)
{
	struct wiimote_listen_t *wml = (struct wiimote_listen_t*)arg;
	struct wiimote_t *wm;

	WIIUSE_DEBUG("__wiiuse_connected(%d)", err);
	wm = wml->assign_cb(wml, err);
	
	if(!wm) {
		bte_disconnect(wml->sock);
		wml->sock = NULL;
		return ERR_OK;
	}

	wml->wm = wm;

	wm->sock = wml->sock;
	wm->bdaddr = wml->bdaddr;

	WIIMOTE_ENABLE_STATE(wm,(WIIMOTE_STATE_CONNECTED|WIIMOTE_STATE_HANDSHAKE));

	wm->handshake_state = 0;
	wiiuse_handshake(wm,NULL,0);

	return ERR_OK;
}

static s32 __wiiuse_connect_step2(void *arg,struct bte_pcb *pcb,u8 err)
{
	WIIUSE_DEBUG("__wiiuse_connect_step2(%d)", err);
	if (err!=ERR_OK) {
		bte_disconnect(pcb);
		return ERR_OK;
	}

	err = bte_connectasync_step2(pcb,__wiiuse_connected);

	if(err==ERR_OK) return ERR_OK;
	
	WIIUSE_ERROR("__wiiuse_connect_step2: bte_connectasync_step2 failed(%d)", err);
	return err;
}

static s32 __wiiuse_accept_step2(void *arg,struct bte_pcb *pcb,u8 err)
{
	WIIUSE_DEBUG("__wiiuse_accept_step2(%d)", err);
	if (err!=ERR_OK) {
		bte_disconnect(pcb);
		return ERR_OK;
	}

	err = bte_listenasync_step2(pcb,__wiiuse_connected);

	if(err==ERR_OK) return ERR_OK;
	
	WIIUSE_ERROR("__wiiuse_accept_step2: bte_listenasync_step2 failed(%d)", err);
	return err;
}

void __wiiuse_sensorbar_enable(int enable)
{
	u32 val;
	u32 level;

	level = IRQ_Disable();
	val = (ACR_ReadReg(0xc0)&~0x100);
	if(enable) val |= 0x100;
	ACR_WriteReg(0xc0,val);
	IRQ_Restore(level);
}

int wiiuse_accept(struct wiimote_listen_t *wml, struct bd_addr *bdaddr, u8 *name, struct wiimote_t *(*assign_cb)(wiimote_listen *wml, u8 err))
{
	s32 err;

	if(!wml || !bdaddr || !assign_cb) return 0;

	WIIUSE_DEBUG("wiiuse_accept %p, bdaddr: %02x:%02x:%02x:%02x:%02x:%02x",wml,bdaddr->addr[5],bdaddr->addr[4],bdaddr->addr[3],bdaddr->addr[2],bdaddr->addr[1],bdaddr->addr[0]);

	if(wml->sock!=NULL) {
		WIIUSE_ERROR("wiiuse_accept: wml->sock was not NULL!");
		return 0;
	}

	wml->wm = NULL;
	bd_addr_set(&(wml->bdaddr),bdaddr);
	if(name) {
		strncpy((char *)wml->name, (char *)name, sizeof(wml->name));
		wml->name[sizeof(wml->name) - 1] = 0x00;
	} else {
		memset(wml->name, 0, sizeof(wml->name));
	}
	wml->assign_cb = assign_cb;

	wml->sock = bte_new();
	if (wml->sock==NULL)
	{
		WIIUSE_ERROR("wiiuse_accept: bte_new failed to alloc new sock");
		return 0;
	}
	
	bte_arg(wml->sock,wml);
	bte_received(wml->sock,__wiiuse_receive);
	bte_disconnected(wml->sock,__wiiuse_disconnected);
	
	err = bte_listenasync(wml->sock,bdaddr,__wiiuse_accept_step2);
	if(err==ERR_OK) return 1;
	
	WIIUSE_ERROR("wiiuse_accept: bte_listenasync failed(%d)", err);

	bte_free(wml->sock);
	wml->sock = NULL;
	return 0;
}	

int wiiuse_connect(struct wiimote_listen_t *wml, struct bd_addr *bdaddr, u8 *name, struct wiimote_t *(*assign_cb)(wiimote_listen *wml, u8 err))
{
	s32 err;

	if(!wml || !bdaddr|| !assign_cb) return 0;
	
	WIIUSE_DEBUG("wiiuse_connect %p, bdaddr: %02x:%02x:%02x:%02x:%02x:%02x",wml,bdaddr->addr[5],bdaddr->addr[4],bdaddr->addr[3],bdaddr->addr[2],bdaddr->addr[1],bdaddr->addr[0]);

	if(wml->sock!=NULL) {
		WIIUSE_ERROR("wiiuse_connect: wml->sock was not NULL!");
		return 0;
	}

	wml->wm = NULL;
	bd_addr_set(&(wml->bdaddr),bdaddr);
	if(name) {
		strncpy((char *)wml->name, (char *)name, sizeof(wml->name));
		wml->name[sizeof(wml->name) - 1] = 0x00;
	} else {
		memset(wml->name, 0, sizeof(wml->name));
	}
	wml->assign_cb = assign_cb;

	wml->sock = bte_new();
	if (wml->sock==NULL)
	{
		WIIUSE_ERROR("wiiuse_connect: bte_new failed to alloc new sock");
		return 0;
	}
	
	bte_arg(wml->sock,wml);
	bte_received(wml->sock,__wiiuse_receive);
	bte_disconnected(wml->sock,__wiiuse_disconnected);

	err = bte_connectasync(wml->sock,bdaddr,__wiiuse_connect_step2);
	if(err==ERR_OK) return 1;
	
	WIIUSE_ERROR("wiiuse_connect: bte_connectasync failed(%d)", err);

	bte_free(wml->sock);
	wml->sock = NULL;
	return 0;
}	

void wiiuse_disconnect(struct wiimote_t *wm)
{
	WIIUSE_DEBUG("wiiuse_disconnect");

	if(wm==NULL || wm->sock==NULL) return;

	WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_CONNECTED);
	bte_disconnect(wm->sock);
	wm->sock = NULL;
}

void wiiuse_sensorbar_enable(int enable)
{
	__wiiuse_sensorbar_enable(enable);
}

void wiiuse_init_cmd_queue(struct wiimote_t *wm)
{
	u32 size;

	if (!__queue_buffer[wm->unid]) {
		size = (MAX_COMMANDS*sizeof(struct cmd_blk_t));
		__queue_buffer[wm->unid] = __lwp_wkspace_allocate(size);
		if(!__queue_buffer[wm->unid]) return;
	}

	__lwp_queue_initialize(&wm->cmdq,__queue_buffer[wm->unid],MAX_COMMANDS,sizeof(struct cmd_blk_t));
}

int wiiuse_io_write(struct wiimote_t *wm,ubyte *buf,int len)
{
	if(wm->sock) {
		return bte_senddata(wm->sock,buf,len);
	}

	return ERR_CONN;
}

#endif
