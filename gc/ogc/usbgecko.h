#ifndef __USBGECKO_H___
#define __USBGECKO_H___

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void usb_flush(s32 chn);
int usb_isgeckoalive(s32 chn);
int usb_recvbuffer(s32 chn,void *buffer,int size);
int usb_sendbuffer(s32 chn,const void *buffer,int size);
int usb_recvbuffer_safe(s32 chn,void *buffer,int size);
int usb_sendbuffer_safe(s32 chn,const void *buffer,int size);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
