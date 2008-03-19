#if defined(HW_RVL)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include <ogcsys.h>
#include <ipc.h>

#include "iusb.h"

#define SWAP16(x)					((((x)&0xff)<<8)|(((x)>>8)&0xff))

#define IUSB_HEAPSIZE					2048
#define IUSB_STRUCTSIZE					96

#define IUSB_IOCTL_CTRLMSG				0
#define IUSB_IOCTL_BLKMSG				1
#define IUSB_IOCTL_INTRMSG				2
#define IUSB_IOCTL_SUSPENDDEV			5
#define IUSB_IOCTL_RESUMEDEV			6
#define IUSB_IOCTL_GETDEVLIST			12
#define IUSB_IOCTL_RHDESCA				15
#define IUSB_IOCTL_DEVREMOVALHOOK		26
#define IUSB_IOCTL_DEVINSERTHOOK		27

static s32 hId = -1;

struct iusb_p
{
	iusbcallback cb;
	void *usrdata;
	void *cpbuffer;
	union {
		struct {
			u8 bmRequestType;
			u8 bmRequest;
			u16 wValue;
			u16 wIndex;
			u16 wLength;
			void *rpData;
		} ctrlmsg;
		struct {
			u8 bEndpoint;
			u16 wLength;
			void *rpData;
		} intrblkmsg;
	};
};

static s32 __intrBlkCtrlMsgCB(s32 result,void *usrdata)
{
	struct iusb_p *iusbdata = (struct iusb_p*)usrdata;

	//printf("__intrBlkCtrlMsgCB(%d,%p,%p)\n",result,iusbdata,iusbdata->cb);
	if(iusbdata->cb!=NULL) iusbdata->cb(result,iusbdata->usrdata);

	if(usrdata!=NULL) iosFree(hId,usrdata);
	return result;
}

static s32 __ctrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,iusbcallback cb,void *userdata)
{
	s32 ret = IUSB_FAILED;
	struct iusb_p *iusbdata = NULL;

	if(rpData==NULL || ((s32)rpData%32)!=0) return IPC_EINVAL;

	if(cb==NULL)
		ret = IOS_IoctlvFormat(hId,fd,IUSB_IOCTL_CTRLMSG,"bbhhhb:d",bmRequestType,bmRequest,SWAP16(wValue),SWAP16(wIndex),SWAP16(wLength),0,rpData,wLength);
	else {
		iusbdata = (struct iusb_p*)iosAlloc(hId,sizeof(struct iusb_p));
		if(iusbdata!=NULL) {
			iusbdata->cb = cb;
			iusbdata->usrdata = userdata;

			iusbdata->ctrlmsg.bmRequestType = bmRequestType;
			iusbdata->ctrlmsg.bmRequest = bmRequest;
			iusbdata->ctrlmsg.wValue = wValue;
			iusbdata->ctrlmsg.wIndex = wIndex;
			iusbdata->ctrlmsg.wLength = wLength;
			iusbdata->ctrlmsg.rpData = rpData;
			ret = IOS_IoctlvFormatAsync(hId,fd,IUSB_IOCTL_CTRLMSG,__intrBlkCtrlMsgCB,iusbdata,"bbhhhb:d",bmRequestType,bmRequest,SWAP16(wValue),SWAP16(wIndex),SWAP16(wLength),0,rpData,wLength);
		}
	}

	return ret;
}

static s32 __intrBlkMsg(s32 fd,u8 ioctl,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	s32 ret = IUSB_FAILED;
	struct iusb_p *iusbdata = NULL;

	if(rpData==NULL || ((s32)rpData%32)!=0) return IPC_EINVAL;

	if(cb==NULL)
		ret = IOS_IoctlvFormat(hId,fd,ioctl,"bh:d",bEndpoint,wLength,rpData,wLength);
	else {
		iusbdata = (struct iusb_p*)iosAlloc(hId,sizeof(struct iusb_p));
		if(iusbdata!=NULL) {
			iusbdata->cb = cb;
			iusbdata->usrdata = usrdata;

			iusbdata->intrblkmsg.bEndpoint = bEndpoint;
			iusbdata->intrblkmsg.wLength = wLength;
			iusbdata->intrblkmsg.rpData = rpData;
			ret = IOS_IoctlvFormatAsync(hId,fd,ioctl,__intrBlkCtrlMsgCB,iusbdata,"bh:d",bEndpoint,wLength,rpData,wLength);
		}
	}

	return ret;
}

static s32 __unicode2ascii(void *rpData,u16 wLength)
{
	s32 clen,wlen;
	char dest[128];
	char *src = rpData;

	if(src[1]!=0x0003) return -1;

	clen=0; wlen=2;
	while(wlen<wLength && clen<127) {
		if(wlen>=src[0]) break;
		
		if(src[wlen+1]!=0) dest[clen] = 0x3f;
		else dest[clen] = src[wlen];

		wlen += 2;
		clen++;
	}
	dest[clen] = 0;

	memcpy(rpData,dest,clen);
	return clen;
}

static s32 __getsetRhPortStatus(s32 fd,s32 ioctl,void *arginp,s32 argins,void *argiop,s32 argios)
{
	return IOS_IoctlvFormat(hId,fd,ioctl,"d:d",arginp,argins,argiop,argios);
}

s32 IUSB_Initialize()
{
	hId = iosCreateHeap(IUSB_HEAPSIZE);
	if(hId<0) return IPC_ENOMEM;

	return IPC_OK;
}

s32 IUSB_Deinitialize()
{
	return iosDestroyHeap(hId);
}

s32 IUSB_OpenDevice(const char *device,u16 vid,u16 pid,s32 *fd)
{
	s32 _fd = -1;
	s32 ret = IUSB_OK;
	char *devicepath = NULL;

	devicepath = iosAlloc(hId,IUSB_MAXPATH);
	if(devicepath==NULL) return IPC_ENOMEM;

	snprintf(devicepath,IUSB_MAXPATH,"/dev/usb/%s/%x/%x",device,vid,pid);

	_fd = IOS_Open(devicepath,0);
	if(_fd<0) ret = _fd;

	*fd = _fd;

	if(devicepath!=NULL) iosFree(hId,devicepath);
	return ret;
}

s32 IUSB_CloseDevice(s32 *fd)
{
	s32 ret;

	ret = IOS_Close(*fd);
	if(ret>=0) *fd = -1;

	return ret;
}

s32 IUSB_GetDeviceDescription(s32 fd,void *descr_buffer)
{
	s32 ret;
	void *devdescr;
	
	devdescr = iosAlloc(hId,18);
	if(devdescr==NULL) return IPC_ENOMEM;

	DCInvalidateRange(devdescr,18);

	ret = __ctrlMsg(fd,0x80,6,0x100,0,18,devdescr,NULL,NULL);
	if(ret>=0) {
		//printf("device description:\n");
		//hexdump(devdescr,18);
	}

	if(devdescr!=NULL) iosFree(hId,devdescr);
	return ret;
}

s32 IUSB_GetAsciiString(s32 fd,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	s32 ret;

	DCInvalidateRange(rpData,wLength);

	ret = __ctrlMsg(fd,0x80,6,(0x300+wValue),wIndex,wLength,rpData,NULL,NULL);
	if(ret>0) {
		if((ret=__unicode2ascii(rpData,wLength))>=0) ((char*)rpData)[ret] = 0;
	}
	return ret;
}

s32 IUSB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	DCInvalidateRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 IUSB_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	DCInvalidateRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 IUSB_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	DCFlushRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 IUSB_WriteIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	DCFlushRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_INTRMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 IUSB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	DCInvalidateRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 IUSB_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	DCInvalidateRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 IUSB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	DCFlushRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 IUSB_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	DCFlushRange(rpData,wLength);
	return __intrBlkMsg(fd,IUSB_IOCTL_BLKMSG,bEndpoint,wLength,rpData,cb,usrdata);
}

s32 IUSB_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	DCInvalidateRange(rpData,wLength);
	return __ctrlMsg(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,NULL,NULL);
}

s32 IUSB_ReadCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	DCInvalidateRange(rpData,wLength);
	return __ctrlMsg(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,cb,usrdata);
}

s32 IUSB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	DCFlushRange(rpData,wLength);
	return __ctrlMsg(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,NULL,NULL);
}

s32 IUSB_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,iusbcallback cb,void *usrdata)
{
	DCFlushRange(rpData,wLength);
	return __ctrlMsg(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,cb,usrdata);
}

s32 IUSB_DeviceRemovalNotifyAsync(s32 fd,ipccallback cb,void *userdata)
{
	return IOS_IoctlAsync(fd,IUSB_IOCTL_DEVREMOVALHOOK,NULL,0,NULL,0,cb,userdata);
}

s32 IUSB_DeviceInsertNotifyAsync(const char *devpath,u16 vid,u16 pid,iusbcallback cb,void *usrdata)
{
	s32 fd,ret = 0;
	struct iusb_p *iusbdata = NULL;

	if(devpath==NULL || ((u32)devpath%32)!=0) return IPC_EINVAL;
	
	fd = IOS_Open(devpath,IPC_OPEN_NONE);
	if(fd<0) return fd;
	
	iusbdata = (struct iusb_p*)iosAlloc(hId,sizeof(struct iusb_p));
	if(iusbdata==NULL) return IPC_ENOMEM;

	iusbdata->cb = cb;
	iusbdata->usrdata = usrdata;
	ret = IOS_IoctlvFormatAsync(hId,fd,IUSB_IOCTL_DEVINSERTHOOK,cb,iusbdata,"hh:",vid,pid);
	IOS_Close(fd);

	return ret;
}

void IUSB_SuspendDevice(s32 fd)
{
	IOS_Ioctl(fd,IUSB_IOCTL_SUSPENDDEV,NULL,0,NULL,0);
}

void IUSB_ResumeDevice(s32 fd)
{
	IOS_Ioctl(fd,IUSB_IOCTL_RESUMEDEV,NULL,0,NULL,0);
}

s32 IUSB_GetRhDesca(s32 fd,u32 *desca)
{
	s32 ret;
	static u32 rhdesca ATTRIBUTE_ALIGN(32) = 0;

	ret = IOS_Ioctl(fd,IUSB_IOCTL_RHDESCA,NULL,0,&rhdesca,sizeof(u32));
	*desca = rhdesca;

	return ret;
}

s32 IUSB_GetDeviceList(const char *devpath,void *descr_buffer,u8 num_descr,u8 b0,u8 *cnt_descr)
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
	ret = IOS_IoctlvFormat(hId,fd,IUSB_IOCTL_GETDEVLIST,"bb:bd",num_descr,b0,&cntdevs,descr_buffer,(num_descr<<3));
	if(ret>=0) *cnt_descr = cntdevs;
	
	iosFree(hId,path);
	return ret;
}
#endif /* defined(HW_RVL) */

