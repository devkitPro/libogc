#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <gccore.h>

#include "geckousb.h"
#include "ogc/usbgecko.h"

static struct dbginterface usb_device;

extern void udelay(int us);

static int usbopen(struct dbginterface *device)
{
	if(!usb_isgeckoalive(device->fhndl)) {
		return -1;
	}

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
	int ret;
	ret = usb_recvbuffer_safe(device->fhndl,buffer,size);
	return ret;
}

static int usbwrite(struct dbginterface *device,const void *buffer,int size)
{
	int ret;
	ret = usb_sendbuffer_safe(device->fhndl,buffer,size);
	return ret;
}

struct dbginterface* usb_init(s32 channel)
{
	usb_device.fhndl = channel;
	if(usb_isgeckoalive(channel))
		usb_flush(channel);

	usb_device.open = usbopen;
	usb_device.close = usbclose;
	usb_device.wait = usbwait;
	usb_device.read = usbread;
	usb_device.write = usbwrite;

	return &usb_device;
}
