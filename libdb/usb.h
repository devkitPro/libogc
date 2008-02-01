#ifndef __USB_H___
#define __USB_H___

int usb_checkgecko();
int usb_recvbuffer(void *buffer,int size);
int usb_sendbuffer(const void *buffer,int size);
int usb_recvbuffer_safe(void *buffer,int size);
int usb_sendbuffer_safe(const void *buffer,int size);
void usb_flush();

#endif
