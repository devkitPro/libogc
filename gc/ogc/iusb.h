#ifndef __IUSB_H__
#define __IUSB_H__

#if defined(HW_RVL)

#include <gctypes.h>

#define IUSB_MAXPATH			IPC_MAXPATH_LEN

#define IUSB_OK					0
#define IUSB_FAILED				1


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef s32 (*iusbcallback)(s32 result,void *usrdata);

s32 IUSB_Initialize();
s32 IUSB_Deinitialize();

s32 IUSB_OpenDevice(const char *device,u16 vid,u16 pid,s32 *fd);
s32 IUSB_CloseDevice(s32 *fd);

s32 IUSB_GetDeviceDescription(s32 fd,void *descr_buffer);
s32 IUSB_DeviceRemovalNotifyAsync(s32 fd,iusbcallback cb,void *userdata);

void IUSB_SuspendDevice(s32 fd);
void IUSB_ResumeDevice(s32 fd);

s32 IUSB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 IUSB_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata);

s32 IUSB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 IUSB_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata);

s32 IUSB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 IUSB_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata);

s32 IUSB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
s32 IUSB_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,iusbcallback cb,void *usrdata);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* defined(HW_RVL) */

#endif
