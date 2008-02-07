#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>

#include "irq.h"
#include "ipc.h"

#if defined(HW_RVL)

static s32 _ipc_hid = -1;
static s32 _ipc_mailboxack = 1;

static vu32 *_ipcReg = (u32*)0xCD000000;

static void __ipc_replyhandler()
{
	u32 ipc_val;

	ipc_val = IPC_ReadReg(2);

}

static void __ipc_ackhandler()
{
	u32 ipc_val;

	ipc_val = ((IPC_ReadReg(1)&0x30)|0x02);
	IPC_WriteReg(1,ipc_val);
	ACR_WriteReg(48,0x40000000);
}

static void __ipc_interrupthandler(u32 irq,void *ctx)
{
	u32 ipc_int;

	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0014)==0x0014) __ipc_replyhandler();

	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0022)==0x0022) __ipc_ackhandler();
}

u32 IPC_ReadReg(u32 reg)
{
	return _ipcReg[reg];
}

void IPC_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg] = val;
}

void ACR_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg>>2] = val;
}

u32 IPC_Init()
{
	IRQ_Request(IRQ_PI_ACR,__ipc_interrupthandler,NULL);
	IPC_WriteReg(1,56);
}

#endif
