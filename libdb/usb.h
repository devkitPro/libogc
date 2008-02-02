#ifndef __USB_H___
#define __USB_H___

struct dbginterface* usb_init(s32 channel);

int usb_recvbuffer(s32 chn,void *buffer,int size);
int usb_sendbuffer(s32 chn,const void *buffer,int size);
int usb_recvbuffer_safe(s32 chn,void *buffer,int size);
int usb_sendbuffer_safe(s32 chn,const void *buffer,int size);
void usb_flush();

#endif
