#ifndef __IPC_H__
#define __IPC_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef s32 (*ipccallback)(s32 result,void *usrdata);

s32 iosCreateHeap(void *membase,s32 size);
s32 iosDestroyHeap(s32 hid);
void* iosAlloc(s32 hid,s32 size);
void iosFree(s32 hid,void *ptr);

void* IPC_GetBufferLo();
void* IPC_GetBufferHi();
void IPC_SetBufferLo(void *bufferlo);
void IPC_SetBufferHi(void *bufferhi);

s32 IOS_Open(const char *dev,u32 access_type);
s32 IOS_OpenAsync(const char *devicename,u32 access_type,ipccallback ipc_cb,void *usrdata);

s32 IOS_Close(s32 fid);
s32 IOS_CloseAsync(s32 fid,ipccallback ipc_cb,void *usrdata);

s32 IOS_Read(s32 fid,void *buf,s32 len);
s32 IOS_ReadAsync(s32 devId,void *buf,s32 len,ipccallback ipc_cb,void *usrdata);
s32 IOS_Write(s32 fid,const void *buf,s32 len);
s32 IOS_WriteAsync(s32 devId,const void *buf,s32 len,ipccallback ipc_cb,void *usrdata);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
