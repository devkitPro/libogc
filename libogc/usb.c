/*-------------------------------------------------------------

usb.c -- USB lowlevel

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#if defined(HW_RVL)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include <ogcsys.h>
#include <ipc.h>
#include <asm.h>
#include <processor.h>

#include "usb.h"

#define USB_HEAPSIZE					2048
#define USB_STRUCTSIZE					96

#define USB_IOCTL_CTRLMSG				0
#define USB_IOCTL_BLKMSG				1
#define USB_IOCTL_INTRMSG				2
#define USB_IOCTL_SUSPENDDEV			5
#define USB_IOCTL_RESUMEDEV				6
#define USB_IOCTL_GETDEVLIST			12
#define USB_IOCTL_DEVREMOVALHOOK		26
#define USB_IOCTL_DEVINSERTHOOK			27

struct _usb_cb {
	usbcallback cb;
	void *usrdata;
	ioctlv *vec;
};

static s32 hId = -1;

static s32 __usb_control_messageCB(s32 result,void *usrdata)
{
	ioctlv *vec = NULL;
	struct _usb_cb *msgcb = (struct _usb_cb*)usrdata;

	if(msgcb==NULL) return IPC_EINVAL;

	if(msgcb->cb!=NULL) msgcb->cb(result,msgcb->usrdata);
    
	vec = msgcb->vec;
	if(vec==NULL) return IPC_EINVAL;

	if(vec[0].data!=NULL) iosFree(hId,vec[0].data);
	if(vec[1].data!=NULL) iosFree(hId,vec[1].data);
	if(vec[2].data!=NULL) iosFree(hId,vec[2].data);
	if(vec[3].data!=NULL) iosFree(hId,vec[3].data);
	if(vec[4].data!=NULL) iosFree(hId,vec[4].data);
	if(vec[5].data!=NULL) iosFree(hId,vec[5].data);

	iosFree(hId,vec);
	iosFree(hId,msgcb);

	return IPC_OK;
}

static s32 __usb_bulk_messageCB(s32 result,void *usrdata)
{
	ioctlv *vec = NULL;
	struct _usb_cb *msgcb = (struct _usb_cb*)usrdata;

	if(msgcb==NULL) return IPC_EINVAL;
	
	if(msgcb->cb!=NULL) msgcb->cb(result,msgcb->usrdata);

	vec = msgcb->vec;
	if(vec==NULL) return IPC_EINVAL;

	if(vec[0].data!=NULL) iosFree(hId,vec[0].data);
	if(vec[1].data!=NULL) iosFree(hId,vec[1].data);

	iosFree(hId,vec);
	iosFree(hId,msgcb);

	return IPC_OK;
}

static s32 __usb_control_message(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	s32 ret = IPC_ENOMEM;
	u8 *pRqType = NULL;
	u8 *pRq = NULL;
	u8 *pNull = NULL;
	u16 *pValue = NULL;
	u16 *pIndex = NULL;
	u16 *pLength = NULL;
	ioctlv *vec = NULL;
	struct _usb_cb *msgcb = NULL;

	if(((s32)rpData%32)!=0) return IPC_EINVAL;
	if(wLength && !rpData) return IPC_EINVAL;
	if(!wLength && rpData) return IPC_EINVAL;

	vec = iosAlloc(hId,sizeof(ioctlv)*7);
	if(vec==NULL) return IPC_ENOMEM;

	pRqType = iosAlloc(hId,32);
	if(pRqType==NULL) goto done;
	*pRqType = bmRequestType;

	pRq = iosAlloc(hId,32);
	if(pRq==NULL) goto done;
	*pRq = bmRequest;

	pValue = iosAlloc(hId,32);
	if(pValue==NULL) goto done;
	*pValue = bswap16(wValue);

	pIndex = iosAlloc(hId,32);
	if(pIndex==NULL) goto done;
	*pIndex = bswap16(wIndex);

	pLength = iosAlloc(hId,32);
	if(pLength==NULL) goto done;
	*pLength = bswap16(wLength);

	pNull = iosAlloc(hId,32);
	if(pNull==NULL) goto done;
	*pNull = 0;

	vec[0].data = pRqType;
	vec[0].len = sizeof(u8);
	vec[1].data = pRq;
	vec[1].len = sizeof(u8);
	vec[2].data = pValue;
	vec[2].len = sizeof(u16);
	vec[3].data = pIndex;
	vec[3].len = sizeof(u16);
	vec[4].data = pLength;
	vec[4].len = sizeof(u16);
	vec[5].data = pNull;
	vec[5].len = sizeof(u8);
	vec[6].data = rpData;
	vec[6].len = wLength;

	if(cb==NULL)
		ret = IOS_Ioctlv(fd,USB_IOCTL_CTRLMSG,6,1,vec);
	else {
		msgcb = iosAlloc(hId,sizeof(struct _usb_cb));
		if(msgcb==NULL) goto done;

		msgcb->cb = cb;
		msgcb->usrdata = usrdata;
		msgcb->vec = vec;
		return IOS_IoctlvAsync(fd,USB_IOCTL_CTRLMSG,6,1,vec,__usb_control_messageCB,msgcb);
	}

done:
	if(pNull!=NULL) iosFree(hId,pNull);
	if(pLength!=NULL) iosFree(hId,pLength);
	if(pIndex!=NULL) iosFree(hId,pIndex);
	if(pValue!=NULL) iosFree(hId,pValue);
	if(pRq!=NULL) iosFree(hId,pRq);
	if(pRqType!=NULL) iosFree(hId,pRqType);
	if(vec!=NULL) iosFree(hId,vec);

	return ret;
}

static s32 __usb_interrupt_bulk_message(s32 fd,u8 ioctl,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	s32 ret = IPC_ENOMEM;
	u8 *pEndP = NULL;
	u16 *pLength = NULL;
	ioctlv *vec = NULL;
	struct _usb_cb *msgcb = NULL;

	if(rpData==NULL || ((s32)rpData%32)!=0) return IPC_EINVAL;

	vec = iosAlloc(hId,sizeof(ioctlv)*3);
	if(vec==NULL) return IPC_ENOMEM;

	pEndP = iosAlloc(hId,32);
	if(pEndP==NULL) goto done;
	*pEndP = bEndpoint;

	pLength = iosAlloc(hId,32);
	if(pLength==NULL) goto done;
	*pLength = wLength;

	vec[0].data = pEndP;
	vec[0].len = sizeof(u8);
	vec[1].data = pLength;
	vec[1].len = sizeof(u16);
	vec[2].data = rpData;
	vec[2].len = wLength;

	if(cb==NULL)
		ret = IOS_Ioctlv(fd,ioctl,2,1,vec);
	else {
		msgcb = iosAlloc(hId,sizeof(struct _usb_cb));
		if(msgcb==NULL) goto done;

		msgcb->cb = cb;
		msgcb->usrdata = usrdata;
		msgcb->vec = vec;
		return IOS_IoctlvAsync(fd,ioctl,2,1,vec,__usb_bulk_messageCB,msgcb);
	}

done:
	if(pLength!=NULL) iosFree(hId,pLength);
	if(pEndP!=NULL) iosFree(hId,pEndP);
	if(vec!=NULL) iosFree(hId,vec);

	return ret;
}

s32 USB_Initialize()
{
	hId = iosCreateHeap(USB_HEAPSIZE);
	if(hId<0) return IPC_ENOMEM;

	return IPC_OK;
}

s32 USB_Deinitialize()
{
	return iosDestroyHeap(hId);
}

s32 USB_OpenDevice(const char *device,u16 vid,u16 pid,s32 *fd)
{
	s32 _fd = -1;
	s32 ret = USB_OK;
	char *devicepath = NULL;

	devicepath = iosAlloc(hId,USB_MAXPATH);
	if(devicepath==NULL) return IPC_ENOMEM;

	snprintf(devicepath,USB_MAXPATH,"/dev/usb/%s/%x/%x",device,vid,pid);

	_fd = IOS_Open(devicepath,0);
	if(_fd<0) ret = _fd;

	*fd = _fd;

	if(devicepath!=NULL) iosFree(hId,devicepath);
	return ret;
}

s32 USB_CloseDevice(s32 *fd)
{
	s32 ret;

	ret = IOS_Close(*fd);
	if(ret>=0) *fd = -1;

	return ret;
}

s32 USB_GetDeviceDescription(s32 fd,usb_devdesc *devdesc)
{
	s32 ret;
	usb_devdesc *p;
	
	p = iosAlloc(hId,USB_DT_DEVICE_SIZE);
	if(p==NULL) return IPC_ENOMEM;

	ret = __usb_control_message(fd,USB_ENDPOINT_IN,USB_REQ_GETDESCRIPTOR,(USB_DT_DEVICE<<8),0,USB_DT_DEVICE_SIZE,p,NULL,NULL);
	if(ret>=0) memcpy(devdesc,p,USB_DT_DEVICE_SIZE);

	if(p!=NULL) iosFree(hId,p);
	return ret;
}

s32 USB_GetAsciiString(s32 fd,u16 wIndex,u16 wLangID,u16 wLength,void *rpData)
{
	s32 ret;

	ret = __usb_control_message(fd,USB_ENDPOINT_IN,USB_REQ_GETDESCRIPTOR,((USB_DT_STRING<<8)+wIndex),wLangID,wLength,rpData,NULL,NULL);
	if(ret>0) {
		// todo: implement unicode to char translation
	}
	return ret;
}

s32 USB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 USB_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_WriteIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 USB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 USB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	return __usb_interrupt_bulk_message(fd,USB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 USB_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,NULL,NULL);
}

s32 USB_ReadCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,cb,usrdata);
}

s32 USB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,NULL,NULL);
}

s32 USB_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,cb,usrdata);
}

s32 USB_DeviceRemovalNotifyAsync(s32 fd,usbcallback cb,void *userdata)
{
	return IOS_IoctlAsync(fd,USB_IOCTL_DEVREMOVALHOOK,NULL,0,NULL,0,cb,userdata);
}

s32 USB_DeviceInsertNotifyAsync(const char *devpath,u16 vid,u16 pid,usbcallback cb,void *usrdata)
{
	char *path;
	s32 fd,ret = 0;

	if(devpath==NULL || *devpath=='\0') return IPC_EINVAL;

	path = (char*)iosAlloc(hId,IPC_MAXPATH_LEN);
	if(path==NULL) return IPC_ENOMEM;
	
	strncpy(path,devpath,IPC_MAXPATH_LEN);
	fd = IOS_Open(devpath,IPC_OPEN_NONE);
	if(fd<0) return fd;
	
	ret = IOS_IoctlvFormatAsync(hId,fd,USB_IOCTL_DEVINSERTHOOK,cb,usrdata,"hh:",vid,pid);
	IOS_Close(fd);

	iosFree(hId,path);
	return ret;
}

void USB_SuspendDevice(s32 fd)
{
	IOS_Ioctl(fd,USB_IOCTL_SUSPENDDEV,NULL,0,NULL,0);
}

void USB_ResumeDevice(s32 fd)
{
	IOS_Ioctl(fd,USB_IOCTL_RESUMEDEV,NULL,0,NULL,0);
}

s32 USB_GetDeviceList(const char *devpath,void *descr_buffer,u8 num_descr,u8 b0,u8 *cnt_descr)
{
	s32 fd,ret;
	char *path;
	u32 cntdevs;

	if(devpath==NULL || *devpath=='\0') return IPC_EINVAL;

	path = (char*)iosAlloc(hId,IPC_MAXPATH_LEN);
	if(path==NULL) return IPC_ENOMEM;
	
	strncpy(path,devpath,IPC_MAXPATH_LEN);
	fd = IOS_Open(path,IPC_OPEN_NONE);
	if(fd<0) {
		iosFree(hId,path);
		return fd;
	}

	cntdevs = 0;
	ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_GETDEVLIST,"bb:bd",num_descr,b0,&cntdevs,descr_buffer,(num_descr<<3));
	if(ret>=0) *cnt_descr = cntdevs;
	
	iosFree(hId,path);
	IOS_Close(fd);
	return ret;
}
#endif /* defined(HW_RVL) */

