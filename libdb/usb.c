#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <gccore.h>

#include "debug_if.h"
#include "usb.h"

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static struct dbginterface usb_device;

static __inline__ int __send_command(s32 chn,u32 cmd)
{
	s32 ret;

	ret = 0;
	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)) {
		if(!EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED32MHZ)) ret |= 0x01;
		if(!EXI_Imm(chn,&cmd,sizeof(cmd),EXI_READWRITE,NULL)) ret |= 0x02;
		if(!EXI_Sync(chn)) ret |= 0x04;
		if(!EXI_Deselect(chn)) ret |= 0x08;
		if(!EXI_Unlock(chn)) ret |= 0x10;
		if(!ret) ret = 1;
	}
	return ret;
}

static int __usb_sendbyte(s32 chn,char ch)
{
	s32 ret;
	u32 val;

	val = (0xB0000000|_SHIFTL(ch,20,8));
	ret = __send_command(chn,val);
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

static int __usb_recvbyte(s32 chn,char *ch)
{
	s32 ret;
	u32 val;

	*ch = 0;
	val = 0xA0000000;
	ret = __send_command(chn,val);
	if(ret==1 && !(val&0x08000000)) ret = 0;
	else if(ret==1) *ch = _SHIFTR(val,16,8);

	return ret;
}

static int __usb_checksend(s32 chn)
{
	s32 ret;
	u32 val;

	val = 0xC0000000;
	ret = __send_command(chn,val);
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

static int __usb_checkrecv(s32 chn)
{
	s32 ret;
	u32 val;

	val = 0xD0000000;
	ret = __send_command(chn,val);
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

static int __usb_isgeckoalive(s32 chn)
{
	s32 ret;
	u32 val;

	val = 0x90000000;
	ret = __send_command(chn,val);;
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

static int usbopen(struct dbginterface *device)
{
	if(!__usb_isgeckoalive(device->fhndl)) return -1;

	return 0;
}

static int usbclose(struct dbginterface *device)
{
	return 0;
}

static int usbwait(struct dbginterface *device)
{
	return 0;
}

static int usbread(struct dbginterface *device,void *buffer,int size)
{
	return usb_recvbuffer(device->fhndl,buffer,size);
}

static int usbwrite(struct dbginterface *device,const void *buffer,int size)
{
	return usb_sendbuffer(device->fhndl,buffer,size);
}

int usb_recvbuffer(s32 chn,void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		ret = __usb_recvbyte(chn,ptr);
		if(ret==0) break;

		ptr++;
		left--;
	}
	return (size - left);
}

int usb_sendbuffer(s32 chn,const void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		ret = __usb_sendbyte(chn,*ptr);
		if(ret==0) break;

		ptr++;
		left--;
	}
	return (size - left);
}

int usb_recvbuffer_safe(s32 chn,void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		if(__usb_checkrecv(chn)) {
			ret = __usb_recvbyte(chn,ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}
	}
	return (size - left);
}

int usb_sendbuffer_safe(s32 chn,const void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		if(__usb_checksend(chn)) {
			ret = __usb_sendbyte(chn,*ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}
	}
	return (size - left);
}

void usb_flush(s32 chn)
{
	char tmp;
	while(__usb_recvbyte(chn,&tmp));
}

struct dbginterface* usb_init(s32 channel)
{
	usb_device.fhndl = channel;
	if(__usb_isgeckoalive(channel)>=0)
		usb_flush(channel);

	usb_device.open = usbopen;
	usb_device.close = usbclose;
	usb_device.wait = usbwait;
	usb_device.read = usbread;
	usb_device.write = usbwrite;

	return &usb_device;
}
