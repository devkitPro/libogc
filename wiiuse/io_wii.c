#ifdef GEKKO

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "definitions.h"
#include "wiiuse_internal.h"
#include "events.h"
#include "io.h"

#define MAX_COMMANDS					0x20

static vu32 *_ipcReg = (u32*)0xCD000000;

extern void parse_event(struct wiimote_t *wm);
extern void idle_cycle(struct wiimote_t* wm);
extern void hexdump(void *d, int len);

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
	struct wiimote_t *wm = (struct wiimote_t*)arg;

	//printf("wiimote disconnected\n");
	if(wm->event_cb) wm->event_cb(wm,WIIUSE_DISCONNECT);
	return ERR_OK;
}

static s32 __wiiuse_receive(void *arg,void *buffer,u16 len)
{
	struct wiimote_t *wm = (struct wiimote_t*)arg;

	if(!buffer || len==0) return ERR_OK;

	//printf("__wiiuse_receive[%02x]\n",*(char*)buffer);
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
	struct wiimote_t *wm = (struct wiimote_t*)arg;

	//printf("__wiiuse_connected()\n");
	WIIMOTE_ENABLE_STATE(wm,(WIIMOTE_STATE_CONNECTED|WIIMOTE_STATE_HANDSHAKE));

	wm->handshake_state = 0;
	wiiuse_handshake(wm,NULL,0);

	return ERR_OK;
}

static s32 __wiiuse_sent(void *arg,struct bte_pcb *pcb,u8 err)
{
	struct wiimote_t *wm = (struct wiimote_t*)arg;
	struct cmd_blk_t *cmd = wm->cmd_head;

	if(!cmd) return ERR_OK;
	if(cmd->state!=CMD_SENT) return ERR_OK;

	//printf("__wiiuse_sent(%p,%02x)\n",cmd,cmd->data[0]);

	switch(cmd->data[0]) {
		case WM_CMD_CTRL_STATUS:
		case WM_CMD_WRITE_DATA:
		case WM_CMD_READ_DATA:
		case WM_CMD_STREAM_DATA:
			return ERR_OK;
		default:
			wm->cmd_head = cmd->next;

			cmd->state = CMD_DONE;
			if(cmd->cb) cmd->cb(wm,NULL,0);

			__lwp_queue_append(&wm->cmdq,&cmd->node);
			wiiuse_send_next_command(wm);
			break;
	}
	return ERR_OK;
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

int wiiuse_register(struct wiimote_t *wm,struct bd_addr *bdaddr)
{
	s32 err;

	if(!wm || !bdaddr) return 0;

	wm->bdaddr = *bdaddr;

	wm->sock = bte_new();
	if(wm->sock==NULL) return 0;

	bte_arg(wm->sock,wm);
	bte_received(wm->sock,__wiiuse_receive);
	bte_disconnected(wm->sock,__wiiuse_disconnected);
	
	err = bte_registerdeviceasync(wm->sock,bdaddr,__wiiuse_connected);
	if(err==ERR_OK) {
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_DEV_REGISTER);
		return 1;
	}

	return 0;
}	

void wiiuse_disconnect(struct wiimote_t *wm)
{
	if(wm==NULL || wm->sock==NULL) return;

	bte_disconnect(wm->sock);
}

void wiiuse_sensorbar_enable(int enable)
{
	__wiiuse_sensorbar_enable(enable);
}

void wiiuse_init_cmd_queue(struct wiimote_t *wm)
{
	u8 *buffer;
	u32 size;

	size = (MAX_COMMANDS*sizeof(struct cmd_blk_t));
	buffer = malloc(size);
	__lwp_queue_initialize(&wm->cmdq,buffer,MAX_COMMANDS,sizeof(struct cmd_blk_t));
}

int wiiuse_io_write(struct wiimote_t *wm,ubyte *buf,int len)
{
	if(wm->sock)
		return bte_sendmessageasync(wm->sock,buf,len,__wiiuse_sent);

	return ERR_CONN;
}

#endif
