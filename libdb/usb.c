#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <gccore.h>

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static __inline__ int __send_command(s32 chn,u32 cmd)
{
	s32 ret;

	ret = 0;
	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)) {
		if(!EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED32MHZ)) ret = 0;
		else if(!EXI_Imm(chn,&cmd,sizeof(cmd),EXI_READWRITE,NULL)) ret = 0;
		else if(!EXI_Sync(chn)) ret = 0;
		else if(!EXI_Deselect(chn)) ret = 0;
		else if(!EXI_Unlock(chn)) ret = 0;
		else ret = 1;
	}
	return ret;
}

static int __usb_sendbyte(char ch)
{
	s32 ret;
	u32 val;

	val = (0xB0000000|_SHIFTL(ch,20,8));
	ret = __send_command(EXI_CHANNEL_0,val);
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

static int __usb_recvbyte(char *ch)
{
	s32 ret;
	u32 val;

	*ch = 0;
	val = 0xA0000000;
	ret = __send_command(EXI_CHANNEL_0,val);
	if(ret==1 && !(val&0x08000000)) ret = 0;
	else if(ret==1) *ch = _SHIFTR(val,16,8);

	return ret;
}

static int __usb_checksend()
{
	s32 ret;
	u32 val;

	val = 0xC0000000;
	ret = __send_command(EXI_CHANNEL_0,val);
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

static int __usb_checkrecv()
{
	s32 ret;
	u32 val;

	val = 0xD0000000;
	ret = __send_command(EXI_CHANNEL_0,val);
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

int usb_checkgecko()
{
	s32 ret;
	u32 val;

	val = 0x90000000;
	ret = __send_command(EXI_CHANNEL_0,val);;
	if(ret==1 && !(val&0x04000000)) ret = 0;

	return ret;
}

int usb_recvbuffer(void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		ret = __usb_recvbyte(ptr);
		if(ret==0) break;

		ptr++;
		left--;
	}
	return (size - left);
}

int usb_sendbuffer(const void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		ret = __usb_sendbyte(*ptr);
		if(ret==0) break;

		ptr++;
		left--;
	}
	return (size - left);
}

int usb_recvbuffer_safe(void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		if(__usb_checkrecv()) {
			ret = __usb_recvbyte(ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}
	}
	return (size - left);
}

int usb_sendbuffer_safe(const void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		if(__usb_checksend()) {
			ret = __usb_sendbyte(*ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}
	}
	return (size - left);
}

void usb_flush()
{
	char tmp;
	while(__usb_recvbyte(&tmp));
}
