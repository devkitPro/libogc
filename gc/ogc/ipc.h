#ifndef __IPC_H__
#define __IPC_H__

#include <gctypes.h>

#define IPC_HEAP			 -1

#define IPC_MAXPATH_LEN		 64

#define IPC_OK				  0
#define IPC_EINVAL			 -4
#define IPC_ENOHEAP			 -5
#define IPC_EQUEUEFULL       -8
#define IPC_ENOMEM			-22

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _ioctlv
{
	void *data;
	u32 len;
} ioctlv;


typedef s32 (*ipccallback)(s32 result,void *usrdata);

s32 iosCreateHeap(s32 size);
s32 iosDestroyHeap(s32 hid);
void* iosAlloc(s32 hid,s32 size);
void iosFree(s32 hid,void *ptr);

void* IPC_GetBufferLo();
void* IPC_GetBufferHi();
void IPC_SetBufferLo(void *bufferlo);
void IPC_SetBufferHi(void *bufferhi);

s32 IOS_Open(const char *filepath,u32 mode);
s32 IOS_OpenAsync(const char *filepath,u32 mode,ipccallback ipc_cb,void *usrdata);

s32 IOS_Close(s32 fd);
s32 IOS_CloseAsync(s32 fd,ipccallback ipc_cb,void *usrdata);

s32 IOS_Seek(s32 fd,s32 where,s32 whence);
s32 IOS_SeekAsync(s32 fd,s32 where,s32 whence,ipccallback ipc_cb,void *usrdata);
s32 IOS_Read(s32 fd,void *buf,s32 len);
s32 IOS_ReadAsync(s32 fd,void *buf,s32 len,ipccallback ipc_cb,void *usrdata);
s32 IOS_Write(s32 fd,const void *buf,s32 len);
s32 IOS_WriteAsync(s32 fd,const void *buf,s32 len,ipccallback ipc_cb,void *usrdata);

s32 IOS_Ioctl(s32 fd,s32 ioctl,void *buffer_in,s32 len_in,void *buffer_io,s32 len_io);
s32 IOS_IoctlAsync(s32 fd,s32 ioctl,void *buffer_in,s32 len_in,void *buffer_io,s32 len_io,ipccallback ipc_cb,void *usrdata);
s32 IOS_Ioctlv(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv);
s32 IOS_IoctlvAsync(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv,ipccallback ipc_cb,void *usrdata);

s32 IOS_IoctlvFormat(s32 hId,s32 fd,s32 ioctl,const char *format,...);
s32 IOS_IoctlvFormatAsync(s32 hId,s32 fd,s32 ioctl,ipccallback usr_cb,void *usr_data,const char *format,...);

s32 IOS_IoctlvReboot(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
