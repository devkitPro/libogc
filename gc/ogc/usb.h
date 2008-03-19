#ifndef __USB_H__
#define __USB_H__

#if defined(HW_RVL)

#include <gctypes.h>

#define USB_MAXPATH				IPC_MAXPATH_LEN

#define USB_OK					0
#define USB_FAILED				1


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef s32 (*usbcallback)(s32 result,void *usrdata);

s32 USB_Initialize();
s32 USB_Deinitialize();

s32 USB_OpenDevice(const char *device,u16 vid,u16 pid,s32 *fd);
s32 USB_CloseDevice(s32 *fd);

s32 USB_GetDeviceDescription(s32 fd,void *descr_buffer);
s32 USB_DeviceRemovalNotifyAsync(s32 fd,usbcallback cb,void *userdata);

void USB_SuspendDevice(s32 fd);
void USB_ResumeDevice(s32 fd);

s32 USB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
s32 USB_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* defined(HW_RVL) */

#endif
