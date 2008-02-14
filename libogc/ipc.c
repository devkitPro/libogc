#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include "asm.h"
#include "processor.h"
#include "lwp.h"
#include "irq.h"
#include "ipc.h"
#include "cache.h"
#include "system.h"
#include "lwp_heap.h"

#define IPC_BUFFER_SIZE			2048

#define IOS_OPEN				0x01
#define IOS_CLOSE				0x02
#define IOS_READ				0x03
#define IOS_WRITE				0x04
#define IOS_SEEK				0x05
#define IOS_IOCTL				0x06
#define IOS_IOCTLV				0x07

#if defined(HW_RVL)

struct _ipcioctlv
{
	void *data;
	u32 len;
};

struct _ipcreq
{
	u32 ios_cmd;		//0
	s32 result;			//4
	s32 devId;			//8
	void *ios_data;		//12
	u32 *ios_param;		//16
	u32 len1;			//20
	void *pad6;			//24
	u32 len2;			//28

	ipccallback cb;		//32
	void *usrdata;		//36
	void *pad10;		//40
	lwpq_t syncqueue;	//44
	u32 pad12;			//48
	u32 pad13;			//52
	u32 pad14;			//56
	u32 pad15;			//60
};

struct _ipcreqres
{
	u32 cnt_sent;
	u32 cnt_queue;
	u32 req_send_no;
	u32 req_queue_no;
	struct _ipcreq *reqs[16];
};

struct _ipcheap
{
	void *membase;
	u32 size;
	heap_cntrl heap;
};

static s32 _ipc_hid = -1;
static s32 _ipc_mailboxack = 1;
static u32 _ipc_relnchFl = 0;
static u32 _ipc_initialized = 0;
static u32 _ipc_clntinitialized = 0;
static struct _ipcreq *_ipc_relnchRpc = NULL;

static void *_ipc_bufferlo = NULL;
static void *_ipc_bufferhi = NULL;
static void *_ipc_currbufferlo = NULL;
static void *_ipc_currbufferhi = NULL;

static struct _ipcreqres _ipc_responses;

static struct _ipcheap _ipc_heaps[8] = 
{
	{NULL, 0, {}},
	{NULL, 0, {}},
	{NULL, 0, {}},
	{NULL, 0, {}},
	{NULL, 0, {}},
	{NULL, 0, {}},
	{NULL, 0, {}},
	{NULL, 0, {}}
};

static vu32 *_ipcReg = (u32*)0xCD000000;

extern void __MaskIrq(u32 nMask);
extern void __UnmaskIrq(u32 nMask);
extern void* __SYS_GetIPCBufferLo();
extern void* __SYS_GetIPCBufferHi();

u32 IPC_ReadReg(u32 reg);
void IPC_WriteReg(u32 reg,u32 val);
void ACR_WriteReg(u32 reg,u32 val);

static struct _ipcreq* __ipc_allocreq()
{
	return (struct _ipcreq*)iosAlloc(_ipc_hid,64);
}

static void __ipc_freereq(struct _ipcreq *req)
{
	iosFree(_ipc_hid,req);
}

static s32 __ipc_queuerequest(struct _ipcreq *req)
{
	u32 level;
	s32 cnt;

	DCFlushRange(req,32);
	_CPU_ISR_Disable(level);
	
	if(_ipc_responses.cnt_queue>=_ipc_responses.cnt_sent) {
		cnt = ((16 - (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent))%16);
	} else
		cnt = (_ipc_responses.cnt_queue + (0 - _ipc_responses.cnt_sent));

	if(cnt<=0) {
		_CPU_ISR_Restore(level);
		return -8;
	}

	_ipc_responses.reqs[_ipc_responses.req_queue_no] = req;
	_ipc_responses.req_queue_no = ((_ipc_responses.req_queue_no+1)&0x0f);
	_ipc_responses.cnt_queue++;

	_CPU_ISR_Restore(level);
	return 0;
}

static s32 __ipc_syncqueuerequest(struct _ipcreq *req)
{
	s32 cnt;

	if(_ipc_responses.cnt_queue>=_ipc_responses.cnt_sent) {
		cnt = ((16 - (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent))%16);
	} else
		cnt = (_ipc_responses.cnt_queue + (0 - _ipc_responses.cnt_sent));

	if(cnt<=0) return -8;

	_ipc_responses.reqs[_ipc_responses.req_queue_no] = req;
	_ipc_responses.req_queue_no = ((_ipc_responses.req_queue_no+1)&0x0f);
	_ipc_responses.cnt_queue++;

	return 0;
}

static void __ipc_sendrequest()
{
	s32 cnt;
	u32 ipc_send;
	struct _ipcreq *req;

	if(_ipc_responses.cnt_queue>=_ipc_responses.cnt_sent)
		cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	else
		cnt = (_ipc_responses.cnt_queue + (0 - _ipc_responses.cnt_sent));

	if(cnt>0) {
		req = _ipc_responses.reqs[_ipc_responses.req_send_no];
		if(req!=NULL) {
			if(req->pad10!=NULL) {
				_ipc_relnchFl = 1;
				_ipc_relnchRpc = req;
				_ipc_mailboxack--;
			}

			IPC_WriteReg(0,MEM_VIRTUAL_TO_PHYSICAL(req));
			_ipc_responses.req_send_no = ((_ipc_responses.req_send_no+1)&0x0f);
			_ipc_responses.cnt_sent++;
			_ipc_mailboxack--;

			ipc_send = ((IPC_ReadReg(1)&0x30)|0x01);
			IPC_WriteReg(1,ipc_send);
		}
	}
}

static void __ipc_replyhandler()
{
	u32 ipc_ack,cnt;
	struct _ipcreq *ptr = NULL;
	struct _ipcreq *req = NULL;
	struct _ipcioctlv *v = NULL;
	req = (struct _ipcreq*)IPC_ReadReg(2);
	if(req==NULL) return;

	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x04);
	IPC_WriteReg(1,ipc_ack);
	ACR_WriteReg(48,0x40000000);

	req = MEM_PHYSICAL_TO_K0(req);
	DCInvalidateRange(req,32);
	
	if(req->devId==0x0003) {
		if(req->ios_data!=NULL) {
			req->ios_data = MEM_PHYSICAL_TO_K0(req->ios_data);
			if(req->result>0) DCInvalidateRange(req->ios_data,req->result);
		}
	} else if(req->devId==0x0006) {
		if(req->pad6!=NULL) {
			req->pad6 = MEM_PHYSICAL_TO_K0(req->pad6);
			DCInvalidateRange(req->pad6,req->len2);
		}
		DCInvalidateRange(req->ios_param,req->len1);
	} else if(req->devId==0x0007) {
		if(req->pad6!=NULL) req->pad6 = MEM_PHYSICAL_TO_K0(req->pad6);

		ptr = (struct _ipcreq*)req->ios_data;
		DCInvalidateRange(ptr->ios_data,(((u32)req->ios_param+req->len1)<<3));

		cnt=0;
		v = (struct _ipcioctlv*)ptr->ios_data;
		while(cnt<((u32)req->ios_param+req->len1)) {
			if(v[cnt].data!=NULL) {
				v[cnt].data = MEM_PHYSICAL_TO_K0(v[cnt].data);
				DCInvalidateRange(v[cnt].data,v[cnt].len);
			}
			cnt++;
		}
		if(_ipc_relnchFl && _ipc_relnchRpc==req) {
			_ipc_relnchFl = 0;
			if(_ipc_mailboxack<1) _ipc_mailboxack++;
		}

	}

	if(req->cb!=NULL) {
		req->cb(req->result,req->usrdata);
		__ipc_freereq(req);
	} else
		LWP_ThreadSignal(req->syncqueue);

	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x08);
	IPC_WriteReg(1,ipc_ack);
}

static void __ipc_ackhandler()
{
	u32 ipc_ack;

	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x02);
	IPC_WriteReg(1,ipc_ack);
	ACR_WriteReg(48,0x40000000);

	if(_ipc_mailboxack<1) _ipc_mailboxack++;
	if(_ipc_mailboxack>0) {
		if(_ipc_relnchFl){
			_ipc_relnchRpc->result = 0;
			_ipc_relnchFl = 0;

			LWP_ThreadSignal(_ipc_relnchRpc->syncqueue);

			ipc_ack = ((IPC_ReadReg(1)&0x30)|0x08);
			IPC_WriteReg(1,ipc_ack);
		}
		__ipc_sendrequest();
	}

}

static void __ipc_interrupthandler(u32 irq,void *ctx)
{
	u32 ipc_int;

	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0014)==0x0014) __ipc_replyhandler();

	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0022)==0x0022) __ipc_ackhandler();
}

s32 iosCreateHeap(void *membase,s32 size)
{
	s32 i;
	u32 level;

	_CPU_ISR_Disable(level);

	i=0;
	while(i<8) {
		if(_ipc_heaps[i].membase==NULL) break;
		i++;
	}
	if(i>=8) return -5;

	_ipc_heaps[i].membase = membase;
	_ipc_heaps[i].size = size;
	__lwp_heap_init(&_ipc_heaps[i].heap,membase,size,32);

	_CPU_ISR_Restore(level);
	return i;
}

s32 iosDestroyHeap(s32 hid)
{
	u32 level;
	s32 ret = 0;

	_CPU_ISR_Disable(level);
	
	if(hid>=0 && hid<8) {
		if(_ipc_heaps[hid].membase!=NULL) {
			_ipc_heaps[hid].membase = NULL;
			_ipc_heaps[hid].size = 0;
		}
	} else
		ret = -4;

	_CPU_ISR_Restore(level);
	return ret;
}

void* iosAlloc(s32 hid,s32 size)
{
	if(hid<0 || hid>7 || size<=0) return NULL;
	return __lwp_heap_allocate(&_ipc_heaps[hid].heap,size);
}

void iosFree(s32 hid,void *ptr)
{
	if(hid<0 || hid>7 || ptr==NULL) return;
	__lwp_heap_free(&_ipc_heaps[hid].heap,ptr);
}

void* IPC_GetBufferLo()
{
	return _ipc_currbufferlo;
}

void* IPC_GetBufferHi()
{
	return _ipc_currbufferhi;
}

void IPC_SetBufferLo(void *bufferlo)
{
	if(_ipc_bufferlo<=bufferlo) _ipc_currbufferlo = bufferlo;
}

void IPC_SetBufferHi(void *bufferhi)
{
	if(bufferhi<=_ipc_bufferhi) _ipc_currbufferhi = bufferhi;
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

void __IPC_Init()
{
	if(!_ipc_initialized) {
		_ipc_bufferlo = _ipc_currbufferlo = __SYS_GetIPCBufferLo();
		_ipc_bufferhi = _ipc_currbufferhi = __SYS_GetIPCBufferHi();
		_ipc_initialized = 1;
	}
}

u32 __IPC_ClntInit()
{
	u32 ipclo,ipchi;

	if(!_ipc_clntinitialized) {
		_ipc_clntinitialized = 1;
		ipclo = (u32)IPC_GetBufferLo();
		ipchi = (u32)IPC_GetBufferHi();
		if(ipchi>=(ipclo+IPC_BUFFER_SIZE)) {
			__IPC_Init();
			_ipc_hid = iosCreateHeap((void*)ipclo,IPC_BUFFER_SIZE);
			IPC_SetBufferLo((void*)(ipclo+IPC_BUFFER_SIZE));
			IRQ_Request(IRQ_PI_ACR,__ipc_interrupthandler,NULL);
			__UnmaskIrq(IM_PI_ACR);
			IPC_WriteReg(1,56);
		} else 
			return -22;
	}
	return 0;
}

s32 IOS_Open(const char *dev,u32 access_type)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	if(dev==NULL) return -4;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_OPEN;
	req->cb = NULL;
	req->pad10 = NULL;

	DCFlushRange((void*)dev,strnlen(dev,64));

	req->ios_data = (void*)MEM_VIRTUAL_TO_PHYSICAL(dev);
	req->ios_param = (void*)access_type;

	LWP_InitQueue(&req->syncqueue);
	DCFlushRange(req,32);

	_CPU_ISR_Disable(level);
	ret = __ipc_syncqueuerequest(req);
	if(ret==0) {
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		LWP_ThreadSleep(req->syncqueue);
		ret = req->result;
	}
	_CPU_ISR_Restore(level);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_OpenAsync(const char *dev,u32 access_type,ipccallback ipc_cb,void *usrdata)
{
	s32 ret = 0;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;

	req->ios_cmd = IOS_OPEN;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->pad10 = NULL;

	DCFlushRange((void*)dev,strnlen(dev,64));
	
	req->ios_data = (void*)MEM_VIRTUAL_TO_PHYSICAL(dev);
	req->ios_param = (void*)access_type;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Close(s32 devId)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_CLOSE;
	req->cb = NULL;
	req->pad10 = NULL;
	req->devId = devId;
	
	LWP_InitQueue(&req->syncqueue);
	DCFlushRange(req,32);

	_CPU_ISR_Disable(level);
	ret = __ipc_syncqueuerequest(req);
	if(ret==0) {
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		LWP_ThreadSleep(req->syncqueue);
		ret = req->result;
	}
	_CPU_ISR_Restore(level);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_CloseAsync(s32 devId,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_CLOSE;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->pad10 = NULL;
	req->devId = devId;
	
	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Read(s32 devId,void *buf,s32 len)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_READ;
	req->cb = NULL;
	req->pad10 = NULL;
	req->devId = devId;
	
	DCInvalidateRange(buf,len);
	req->ios_data = (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->ios_param = (void*)len;

	LWP_InitQueue(&req->syncqueue);
	DCFlushRange(req,32);

	_CPU_ISR_Disable(level);
	ret = __ipc_syncqueuerequest(req);
	if(ret==0) {
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		LWP_ThreadSleep(req->syncqueue);
		ret = req->result;
	}
	_CPU_ISR_Restore(level);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_ReadAsync(s32 devId,void *buf,s32 len,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_READ;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->pad10 = NULL;
	req->devId = devId;
	
	DCInvalidateRange(buf,len);
	req->ios_data = (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->ios_param = (void*)len;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Write(s32 devId,const void *buf,s32 len)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_WRITE;
	req->cb = NULL;
	req->pad10 = NULL;
	req->devId = devId;
	
	DCFlushRange((void*)buf,len);
	req->ios_data = (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->ios_param = (void*)len;

	LWP_InitQueue(&req->syncqueue);
	DCFlushRange(req,32);

	_CPU_ISR_Disable(level);
	ret = __ipc_syncqueuerequest(req);
	if(ret==0) {
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		LWP_ThreadSleep(req->syncqueue);
		ret = req->result;
	}
	_CPU_ISR_Restore(level);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_WriteAsync(s32 devId,const void *buf,s32 len,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return -22;
	
	req->ios_cmd = IOS_WRITE;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->pad10 = NULL;
	req->devId = devId;
	
	DCFlushRange((void*)buf,len);
	req->ios_data = (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->ios_param = (void*)len;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

#endif
