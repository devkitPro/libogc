#ifndef __USB_H__
#define __USB_H__

#if defined(HW_RVL)

#include <gctypes.h>

#define USB_MAXPATH					IPC_MAXPATH_LEN

#define USB_OK						0
#define USB_FAILED					1

/* Descriptor types */
#define USB_DT_DEVICE				0x01
#define USB_DT_CONFIG				0x02
#define USB_DT_STRING				0x03
#define USB_DT_INTERFACE			0x04
#define USB_DT_ENDPOINT				0x05

/* Standard requests */
#define USB_REQ_GETSTATUS			0x00
#define USB_REQ_CLEARFEATURE		0x01
#define USB_REQ_SETFEATURE			0x03
#define USB_REQ_SETADDRESS			0x05
#define USB_REQ_GETDESCRIPTOR		0x06
#define USB_REQ_SETDESCRIPTOR		0x07
#define USB_REQ_GETCONFIG			0x08
#define USB_REQ_SETCONFIG			0x09
#define USB_REQ_GETINTERFACE		0x0a
#define USB_REQ_SETINTERFACE		0x0b
#define USB_REQ_SYNCFRAME			0x0c

/* Descriptor sizes per descriptor type */
#define USB_DT_DEVICE_SIZE			18
#define USB_DT_CONFIG_SIZE			9
#define USB_DT_INTERFACE_SIZE		9
#define USB_DT_ENDPOINT_SIZE		7
#define USB_DT_ENDPOINT_AUDIO_SIZE	9	/* Audio extension */
#define USB_DT_HUB_NONVAR_SIZE		7
   
#define USB_ENDPOINT_IN				0x80
#define USB_ENDPOINT_OUT			0x00


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _usbdevdesc 
{
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdUSB;
	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;
	u8  iSerialNumber;
	u8  bNumConfigurations;
} usb_devdesc;

typedef s32 (*usbcallback)(s32 result,void *usrdata);

s32 USB_Initialize();
s32 USB_Deinitialize();

s32 USB_OpenDevice(const char *device,u16 vid,u16 pid,s32 *fd);
s32 USB_CloseDevice(s32 *fd);

s32 USB_GetDeviceDescription(s32 fd,usb_devdesc *devdesc);
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
