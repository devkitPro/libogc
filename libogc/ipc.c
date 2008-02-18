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

//#define DEBUG_IPC

#define IPC_HEAP_SIZE			2048
#define IPC_REQUESTSIZE			64

#define IOS_OPEN				0x01
#define IOS_CLOSE				0x02
#define IOS_READ				0x03
#define IOS_WRITE				0x04
#define IOS_SEEK				0x05
#define IOS_IOCTL				0x06
#define IOS_IOCTLV				0x07

#if defined(HW_RVL)

struct _ipcreq
{						//ipc struct size: 32
	u32 cmd;			//0
	s32 result;			//4
	s32 fd;				//8
	union {
		struct {
			char *filepath;
			u32 mode;
		} open;
		struct {
			void *data;
			u32 len;
		} read, write;
		struct {
			u32 where;
			u32 whence;
		} seek;
		struct {
			u32 ioctl;
			void *buffer_in;
			u32 len_in;
			void *buffer_io;
			u32 len_io;
		} ioctl;
		struct {
			u32 ioctl;
			u32 argcin;
			u32 argcio;
			struct _ioctlv *argv;
		} ioctlv;
	};

	ipccallback cb;		//32
	void *usrdata;		//36
	u32 relnch;			//40
	lwpq_t syncqueue;	//44
	u8 pad1[16];		//48 - 60
} ATTRIBUTE_PACKED;

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

static __inline__ u32 IPC_ReadReg(u32 reg)
{
	return _ipcReg[reg];
}

static __inline__ void IPC_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg] = val;
}

static __inline__ void ACR_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg>>2] = val;
}

static __inline__ void* __ipc_allocreq()
{
	return iosAlloc(_ipc_hid,IPC_REQUESTSIZE);
}

static __inline__ void __ipc_freereq(void *ptr)
{
	iosFree(_ipc_hid,ptr);
}

static s32 __ipc_queuerequest(struct _ipcreq *req)
{
	u32 cnt;
	u32 level;
#ifdef DEBUG_IPC
	printf("__ipc_queuerequest(0x%p)\n",req);
#endif
	DCFlushRange(req,32);
	_CPU_ISR_Disable(level);

	cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	if(cnt>=16) {
		_CPU_ISR_Restore(level);
		return IPC_EQUEUEFULL;
	}

	_ipc_responses.reqs[_ipc_responses.req_queue_no] = req;
	_ipc_responses.req_queue_no = ((_ipc_responses.req_queue_no+1)&0x0f);
	_ipc_responses.cnt_queue++;

	_CPU_ISR_Restore(level);
	return IPC_OK;
}

static s32 __ipc_syncqueuerequest(struct _ipcreq *req)
{
	u32 cnt;
#ifdef DEBUG_IPC
	printf("__ipc_syncqueuerequest(0x%p)\n",req);
#endif
	cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	if(cnt>=16) {
		return IPC_EQUEUEFULL;
	}

	_ipc_responses.reqs[_ipc_responses.req_queue_no] = req;
	_ipc_responses.req_queue_no = ((_ipc_responses.req_queue_no+1)&0x0f);
	_ipc_responses.cnt_queue++;

	return IPC_OK;
}

static void __ipc_sendrequest()
{
	u32 cnt;
	u32 ipc_send;
	struct _ipcreq *req;
#ifdef DEBUG_IPC
	printf("__ipc_sendrequest()\n");
#endif
	cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	if(cnt>0) {
		req = _ipc_responses.reqs[_ipc_responses.req_send_no];
		if(req!=NULL) {
			if(req->relnch) {
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
	struct _ipcreq *req = NULL;
	ioctlv *v = NULL;
#ifdef DEBUG_IPC
	printf("__ipc_replyhandler()\n");
#endif
	req = (struct _ipcreq*)IPC_ReadReg(2);
	if(req==NULL) return;

	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x04);
	IPC_WriteReg(1,ipc_ack);
	ACR_WriteReg(48,0x40000000);

	req = MEM_PHYSICAL_TO_K0(req);
	DCInvalidateRange(req,32);
	
	if(req->cmd==IOS_READ) {
		if(req->read.data!=NULL) {
			req->read.data = MEM_PHYSICAL_TO_K0(req->read.data);
			if(req->result>0) DCInvalidateRange(req->read.data,req->result);
		}
	} else if(req->cmd==IOS_IOCTL) {
		if(req->ioctl.buffer_io!=NULL) {
			req->ioctl.buffer_io = MEM_PHYSICAL_TO_K0(req->ioctl.buffer_io);
			DCInvalidateRange(req->ioctl.buffer_io,req->ioctl.len_io);
		}
		DCInvalidateRange(req->ioctl.buffer_in,req->ioctl.len_in);
	} else if(req->cmd==IOS_IOCTLV) {
		if(req->ioctlv.argv!=NULL) {
			req->ioctlv.argv = MEM_PHYSICAL_TO_K0(req->ioctlv.argv);
			DCInvalidateRange(req->ioctlv.argv,((req->ioctlv.argcin+req->ioctlv.argcio)*sizeof(struct _ioctlv)));
		}

		cnt = 0;
		v = (ioctlv*)req->ioctlv.argv;
		while(cnt<(req->ioctlv.argcin+req->ioctlv.argcio)) {
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
#ifdef DEBUG_IPC
	printf("__ipc_ackhandler()\n");
#endif
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
#ifdef DEBUG_IPC
	printf("__ipc_interrupthandler(%d)\n",irq);
#endif
	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0014)==0x0014) __ipc_replyhandler();

	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0022)==0x0022) __ipc_ackhandler();
}

s32 iosCreateHeap(void *membase,s32 size)
{
	s32 i;
	u32 level;
#ifdef DEBUG_IPC
	printf("iosCreateHeap(0x%p,%d)\n",membase,size);
#endif
	_CPU_ISR_Disable(level);

	i=0;
	while(i<8) {
		if(_ipc_heaps[i].membase==NULL) break;
		i++;
	}
	if(i>=8) return -5;

	_ipc_heaps[i].membase = membase;
	_ipc_heaps[i].size = size;
	_CPU_ISR_Restore(level);

	__lwp_heap_init(&_ipc_heaps[i].heap,membase,size,PPC_CACHE_ALIGNMENT);
	return i;
}

s32 iosDestroyHeap(s32 hid)
{
	u32 level;
	s32 ret = 0;
#ifdef DEBUG_IPC
	printf("iosDestroyHeap(%d)\n",hid);
#endif
	_CPU_ISR_Disable(level);
	
	if(hid>=0 && hid<8) {
		if(_ipc_heaps[hid].membase!=NULL) {
			_ipc_heaps[hid].membase = NULL;
			_ipc_heaps[hid].size = 0;
		}
	} else
		ret = IPC_EINVAL;

	_CPU_ISR_Restore(level);
	return ret;
}

void* iosAlloc(s32 hid,s32 size)
{
#ifdef DEBUG_IPC
	printf("iosAlloc(%d,%d)\n",hid,size);
#endif
	if(hid<0 || hid>7 || size<=0) return NULL;
	return __lwp_heap_allocate(&_ipc_heaps[hid].heap,size);
}

void iosFree(s32 hid,void *ptr)
{
#ifdef DEBUG_IPC
	printf("iosFree(%d,0x%p)\n",hid,ptr);
#endif
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
	s32 free;
	u32 ipclo,ipchi;

	if(!_ipc_clntinitialized) {
		_ipc_clntinitialized = 1;

		__IPC_Init();

		ipclo = (u32)IPC_GetBufferLo();
		ipchi = (u32)IPC_GetBufferHi();
		free = (ipchi - (ipclo+IPC_HEAP_SIZE));
		if(free<0) return IPC_ENOMEM;

		_ipc_hid = iosCreateHeap((void*)ipclo,IPC_HEAP_SIZE);
		IPC_SetBufferLo((void*)(ipclo+IPC_HEAP_SIZE));
		IRQ_Request(IRQ_PI_ACR,__ipc_interrupthandler,NULL);
		__UnmaskIrq(IM_PI_ACR);
		IPC_WriteReg(1,56);
	}
	return IPC_OK;
}

s32 IOS_Open(const char *filepath,u32 mode)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;
#ifdef DEBUG_IPC
	printf("IOS_Open(%s,%d)\n",filepath,mode);
#endif
	if(filepath==NULL) return IPC_EINVAL;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_OPEN;
	req->cb = NULL;
	req->relnch = 0;

	DCFlushRange((void*)filepath,strnlen(filepath,IPC_MAXPATH_LEN));

	req->open.filepath	= (char*)MEM_VIRTUAL_TO_PHYSICAL(filepath);
	req->open.mode		= mode;

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

s32 IOS_OpenAsync(const char *filepath,u32 mode,ipccallback ipc_cb,void *usrdata)
{
	s32 ret = 0;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_OPEN;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	DCFlushRange((void*)filepath,strnlen(filepath,IPC_MAXPATH_LEN));
	
	req->open.filepath	= (char*)MEM_VIRTUAL_TO_PHYSICAL(filepath);
	req->open.mode		= mode;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Close(s32 fd)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_CLOSE;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;
	
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

s32 IOS_CloseAsync(s32 fd,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_CLOSE;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;
	
	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Read(s32 fd,void *buf,s32 len)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_READ;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;
	
	DCInvalidateRange(buf,len);
	req->read.data	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->read.len	= len;

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

s32 IOS_ReadAsync(s32 fd,void *buf,s32 len,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_READ;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;
	
	DCInvalidateRange(buf,len);
	req->read.data	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->read.len	= len;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Write(s32 fd,const void *buf,s32 len)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_WRITE;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;
	
	DCFlushRange((void*)buf,len);
	req->write.data	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->write.len	= len;

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

s32 IOS_WriteAsync(s32 fd,const void *buf,s32 len,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_WRITE;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;
	
	DCFlushRange((void*)buf,len);
	req->write.data		= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->write.len		= len;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Seek(s32 fd,s32 where,s32 whence)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_SEEK;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	req->seek.where		= where;
	req->seek.whence	= whence;

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

s32 IOS_SeekAsync(s32 fd,s32 where,s32 whence,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_SEEK;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	req->seek.where		= where;
	req->seek.whence	= whence;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Ioctl(s32 fd,s32 ioctl,void *buffer_in,s32 len_in,void *buffer_io,s32 len_io)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_IOCTL;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	req->ioctl.ioctl		= ioctl;
	req->ioctl.buffer_in	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_in);
	req->ioctl.len_in		= len_in;
	req->ioctl.buffer_io	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_io);
	req->ioctl.len_io		= len_io;

	DCFlushRange(buffer_in,len_in);
	DCFlushRange(buffer_io,len_io);

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

s32 IOS_IoctlAsync(s32 fd,s32 ioctl,void *buffer_in,s32 len_in,void *buffer_io,s32 len_io,ipccallback ipc_cb,void *usrdata)
{
	s32 ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_IOCTL;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	req->ioctl.ioctl		= ioctl;
	req->ioctl.buffer_in	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_in);
	req->ioctl.len_in		= len_in;
	req->ioctl.buffer_io	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_io);
	req->ioctl.len_io		= len_io;

	DCFlushRange(buffer_in,len_in);
	DCFlushRange(buffer_io,len_io);

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 IOS_Ioctlv(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv)
{
	s32 i,ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_IOCTLV;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	req->ioctlv.ioctl	= ioctl;
	req->ioctlv.argcin	= cnt_in;
	req->ioctlv.argcio	= cnt_io;
	req->ioctlv.argv	= (struct _ioctlv*)MEM_VIRTUAL_TO_PHYSICAL(argv);

	i = 0;
	while(i<cnt_in) {
		if(argv[i].data!=NULL && argv[i].len>0) {
			DCFlushRange(argv[i].data,argv[i].len);
			argv[i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[i].data);
		}
		i++;
	}

	i = 0;
	while(i<cnt_io) {
		if(argv[cnt_in+i].data!=NULL && argv[cnt_in+i].len>0) {
			DCFlushRange(argv[cnt_in+i].data,argv[cnt_in+i].len);
			argv[cnt_in+i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[cnt_in+i].data);
		}
		i++;
	}
	DCFlushRange(argv,((cnt_in+cnt_io)<<3));

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


s32 IOS_IoctlvAsync(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv,ipccallback ipc_cb,void *usrdata)
{
	s32 i,ret;
	u32 level;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;
	
	req->cmd = IOS_IOCTLV;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	req->ioctlv.ioctl	= ioctl;
	req->ioctlv.argcin	= cnt_in;
	req->ioctlv.argcio	= cnt_io;
	req->ioctlv.argv	= (struct _ioctlv*)MEM_VIRTUAL_TO_PHYSICAL(argv);

	i = 0;
	while(i<cnt_in) {
		if(argv[i].data!=NULL && argv[i].len>0) {
			DCFlushRange(argv[i].data,argv[i].len);
			argv[i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[i].data);
		}
		i++;
	}

	i = 0;
	while(i<cnt_io) {
		if(argv[cnt_in+i].data!=NULL && argv[cnt_in+i].len>0) {
			DCFlushRange(argv[cnt_in+i].data,argv[cnt_in+i].len);
			argv[cnt_in+i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[cnt_in+i].data);
		}
		i++;
	}
	DCFlushRange(argv,((cnt_in+cnt_io)<<3));

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
