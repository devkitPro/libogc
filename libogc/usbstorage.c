/*-------------------------------------------------------------

usbstorage.c -- Bulk-only USB mass storage support

Copyright (C) 2008
Sven Peter (svpe) <svpe@gmx.net>

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
#ifdef HW_RVL

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <ogc/cond.h>
#include <errno.h>

#include "asm.h"
#include "processor.h"

#define	HEAP_SIZE			4096
#define	TAG_START			0x0BADC0DE

#define	CBW_SIZE			31
#define	CBW_SIGNATURE			0x43425355
#define	CBW_IN				(1 << 7)
#define	CBW_OUT				0

#define	CSW_SIZE			13
#define	CSW_SIGNATURE			0x53425355

#define	SCSI_TEST_UNIT_READY		0x00
#define	SCSI_REQUEST_SENSE		0x03
#define	SCSI_READ_CAPACITY		0x25
#define	SCSI_READ_10			0x28
#define	SCSI_WRITE_10			0x2A

#define	SCSI_SENSE_REPLY_SIZE		18
#define	SCSI_SENSE_NOT_READY		0x02
#define	SCSI_SENSE_MEDIUM_ERROR		0x03
#define	SCSI_SENSE_HARDWARE_ERROR	0x04

#define	USB_CLASS_MASS_STORAGE		0x08
#define	MASS_STORAGE_SCSI_COMMANDS	0x06
#define	MASS_STORAGE_BULK_ONLY		0x50

#define USBSTORAGE_GET_MAX_LUN		0xFE
#define USBSTORAGE_RESET		0xFF

#define	USB_ENDPOINT_BULK		0x02

#define USBSTORAGE_CYCLE_RETRIES	3

static s32 hId = -1;

static s32 __usbstorage_reset(usbstorage_handle *dev);
static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun);

/* XXX: this is a *really* dirty and ugly way to send a bulkmessage with a timeout
 *      but there's currently no other known way of doing this and it's in my humble
 *      opinion still better than having a function blocking forever while waiting
 *      for the USB data/IOS reply..
 */

static s32 __usb_blkmsgtimeout_cb(s32 retval, void *dummy)
{
	usbstorage_handle *dev = (usbstorage_handle *)dummy;
	dev->retval = retval;
	LWP_CondSignal(dev->cond);
	return 0;
}

static s32 __USB_BlkMsgTimeout(usbstorage_handle *dev, u8 bEndpoint, u16 wLength, void *rpData)
{
	s32 retval;
	struct timespec ts;

	ts.tv_sec = 2;
	ts.tv_nsec = 0;


	dev->retval = USBSTORAGE_ETIMEDOUT;
	retval = USB_WriteBlkMsgAsync(dev->usb_fd, bEndpoint, wLength, rpData, __usb_blkmsgtimeout_cb, (void *)dev);
	if(retval < 0)
		return retval;

	retval = LWP_CondTimedWait(dev->cond, dev->lock, &ts);

	if(retval == ETIMEDOUT)
	{
		USB_CloseDevice(&dev->usb_fd);
		return USBSTORAGE_ETIMEDOUT;
	}

	retval = dev->retval;
	return retval;
}

static s32 __USB_CtrlMsgTimeout(usbstorage_handle *dev, u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, u16 wLength, void *rpData)
{
	s32 retval;
	struct timespec ts;

	ts.tv_sec = 2;
	ts.tv_nsec = 0;


	dev->retval = USBSTORAGE_ETIMEDOUT;
	retval = USB_WriteCtrlMsgAsync(dev->usb_fd, bmRequestType, bmRequest, wValue, wIndex, wLength, rpData, __usb_blkmsgtimeout_cb, (void *)dev);
	if(retval < 0)
		return retval;

	retval = LWP_CondTimedWait(dev->cond, dev->lock, &ts);

	if(retval == ETIMEDOUT)
	{
		USB_CloseDevice(&dev->usb_fd);
		return USBSTORAGE_ETIMEDOUT;
	}

	retval = dev->retval;
	return retval;
}

s32 USBStorage_Initialize()
{
	if(hId > 0)
		return 0;

	hId = iosCreateHeap(HEAP_SIZE);

	if(hId < 0)
		return IPC_ENOHEAP;

	return IPC_OK;

}

s32 USBStorage_Deinitialize()
{
	s32 retval;

	retval = iosDestroyHeap(hId);
	hId = -1;
	return retval;
}

static inline void __write32(u8 *p, u32 v)
{
	p[0] = v;
	p[1] = v >> 8;
	p[2] = v >> 16;
	p[3] = v >> 24;
}

static inline u32 __read32(u8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static s32 __send_cbw(usbstorage_handle *dev, u8 lun, u32 len, u8 flags, const u8 *cb, u8 cbLen)
{
	s32 retval = USBSTORAGE_OK;
	STACK_ALIGN(u8,cbw,CBW_SIZE,32);

	if(cbLen == 0 || cbLen > 16)
		return IPC_EINVAL;

	memset(cbw, 0, CBW_SIZE);

	__write32(cbw, CBW_SIGNATURE);
	__write32(cbw + 4, dev->tag);
	__write32(cbw + 8, len);
	cbw[12] = flags;
	cbw[13] = lun;
	cbw[14] = (cbLen > 6 ? 0x10 : 6);

	memcpy(cbw + 15, cb, cbLen);

	if(dev->suspended == 1)
	{
		USB_ResumeDevice(dev->usb_fd);
		dev->suspended = 0;
	}

	retval = __USB_BlkMsgTimeout(dev, dev->ep_out, CBW_SIZE, (void *)cbw);

	if(retval == CBW_SIZE)
		retval = USBSTORAGE_OK;
	else if(retval > 0)
		retval = USBSTORAGE_ESHORTWRITE;

	return retval;
}

static s32 __read_csw(usbstorage_handle *dev, u8 *status, u32 *dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u32 signature, tag, _dataResidue, _status;
	STACK_ALIGN(u8,csw,CSW_SIZE,32);

	memset(csw, 0, CSW_SIZE);

	retval = __USB_BlkMsgTimeout(dev, dev->ep_in, CSW_SIZE, csw);
	if(retval == CSW_SIZE)
		retval = USBSTORAGE_OK;
	else if(retval > 0)
		return USBSTORAGE_ESHORTREAD;
	else
		return retval;

	signature = __read32(csw);
	tag = __read32(csw + 4);
	_dataResidue = __read32(csw + 8);
	_status = csw[12];

	if(signature != CSW_SIGNATURE) return USBSTORAGE_ESIGNATURE;

	if(dataResidue != NULL)
		*dataResidue = _dataResidue;
	if(status != NULL)
		*status = _status;

	if(tag != dev->tag) return USBSTORAGE_ETAG;
	dev->tag++;

	return retval;
}

static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u8 *bfr = NULL;

	u8 status = 0;
	u32 dataResidue = 0;
	u32 thisLen;

	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;

	bfr = iosAlloc(hId, write ? dev->ep_out_size : dev->ep_in_size);
	if(bfr == NULL) return IPC_ENOMEM;

	LWP_MutexLock(dev->lock);
	do
	{
		retries--;

		if(retval == USBSTORAGE_ETIMEDOUT)
			break;

		if(write)
		{
			retval = __send_cbw(dev, lun, len, CBW_OUT, cb, cbLen);
			if(retval == USBSTORAGE_ETIMEDOUT)
				break;
			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
			while(len > 0)
			{
				thisLen = len > dev->ep_out_size ? dev->ep_out_size : len;
				memset(bfr, 0, dev->ep_out_size);
				memcpy(bfr, buffer, thisLen);
				retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, bfr);

				if(retval == USBSTORAGE_ETIMEDOUT)
					break;

				if(retval < 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break;
				}


				if(retval != thisLen && len > 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break;
				}
				len -= retval;
				buffer += retval;
			}

			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
		}
		else
		{
			retval = __send_cbw(dev, lun, len, CBW_IN, cb, cbLen);

			if(retval == USBSTORAGE_ETIMEDOUT)
				break;

			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
			while(len > 0)
			{
				thisLen = len > dev->ep_in_size ? dev->ep_in_size : len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_in, thisLen, bfr);
				if(retval < 0)
					break;

				memcpy(buffer, bfr, retval);
				len -= retval;
				buffer += retval;

				if(retval != thisLen)
					break;
			}

			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
		}

		retval = __read_csw(dev, &status, &dataResidue);

		if(retval == USBSTORAGE_ETIMEDOUT)
			break;

		if(retval < 0)
		{
			if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
				retval = USBSTORAGE_ETIMEDOUT;
			continue;
		}

		retval = USBSTORAGE_OK;
	} while(retval < 0 && retries > 0);

	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}
	LWP_MutexUnlock(dev->lock);


	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;

	if(bfr != NULL) iosFree(hId, bfr);
	return retval;
}

static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[16];
	u8 sense[SCSI_SENSE_REPLY_SIZE];
	u8 status = 0;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;

	retval = __cycle(dev, lun, NULL, 0, cmd, 1, 0, &status, NULL);
	if(retval < 0) return retval;

	if(status != 0)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		cmd[5] = 0;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL);
		if(retval < 0) return retval;

		status = sense[2] & 0x0F;
		if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR) return USBSTORAGE_ESENSE;
	}

	return retval;
}

static s32 __usbstorage_reset(usbstorage_handle *dev)
{
	s32 retval;

	if(dev->suspended == 1)
	{
		USB_ResumeDevice(dev->usb_fd);
		dev->suspended = 0;
	}

	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);

	/* FIXME?: some devices return -7004 here which definitely violates the usb ms protocol but they still seem to be working... */
	if(retval < 0 && retval != -7004)
		goto end;

	/* gives device enough time to process the reset */
	usleep(60);

	retval = USB_ClearHalt(dev->usb_fd, dev->ep_in);
	if(retval < 0)
		goto end;
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_out);

end:
	return retval;
}

s32 USBStorage_Open(usbstorage_handle *dev, const char *bus, u16 vid, u16 pid)
{
	u8 *response_buffer = NULL;
	s32 retval = -1;
	u8 conf;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	memset(dev, 0, sizeof(*dev));

	dev->tag = TAG_START;
	if(LWP_MutexInit(&dev->lock, false) < 0)
		goto free_and_return;
	if(LWP_CondInit(&dev->cond) < 0)
		goto free_and_return;

	retval = USB_OpenDevice(bus, vid, pid, &dev->usb_fd);
	if(retval < 0)
		goto free_and_return;
	
	response_buffer = iosAlloc(hId, 1);
	if(response_buffer == NULL)
		goto free_and_return;
	
	retval = USB_GetDescriptors(dev->usb_fd, &udd);
	if(retval < 0)
		goto free_and_return;

	for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
	{
		ucd = &udd.configurations[iConf];		
		for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
		{
			uid = &ucd->interfaces[iInterface];
			if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
			   uid->bInterfaceSubClass == MASS_STORAGE_SCSI_COMMANDS &&
			   uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY)
			{
				if(uid->bNumEndpoints < 2)
					continue;

				dev->ep_in = dev->ep_in_size = dev->ep_out = dev->ep_out_size = 0;
				for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
				{
					ued = &uid->endpoints[iEp];
					if(ued->bmAttributes != USB_ENDPOINT_BULK)
						continue;

					if(ued->bEndpointAddress & USB_ENDPOINT_IN)
					{
						dev->ep_in = ued->bEndpointAddress;
						dev->ep_in_size = ued->wMaxPacketSize;
					}
					else
					{
						dev->ep_out = ued->bEndpointAddress;
						dev->ep_out_size = ued->wMaxPacketSize;
					}
				}
				if(dev->ep_in != 0 && dev->ep_out != 0)
				{
					dev->configuration = ucd->bConfigurationValue;
					dev->interface = uid->bInterfaceNumber;
					dev->altInterface = uid->bAlternateSetting;
					goto found;
				}
			}
		}
	}

	USB_FreeDescriptors(&udd);
	retval = USBSTORAGE_ENOINTERFACE;
	goto free_and_return;

found:
	USB_FreeDescriptors(&udd);

	retval = USBSTORAGE_EINIT;
	if(USB_GetConfiguration(dev->usb_fd, &conf) < 0)
		goto free_and_return;
	if(conf != dev->configuration && USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0)
		goto free_and_return;
	if(dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
		goto free_and_return;
	dev->suspended = 0;

	retval = USBStorage_Reset(dev);
	if(retval < 0)
		goto free_and_return;

	LWP_MutexLock(dev->lock);
	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_GET_MAX_LUN, 0, dev->interface, 1, response_buffer);
	LWP_MutexUnlock(dev->lock);
	if(retval < 0)
		dev->max_lun = 1;
	else
		dev->max_lun = *response_buffer;

	
	if(retval == USBSTORAGE_ETIMEDOUT)
		goto free_and_return;

	retval = USBSTORAGE_OK;
	dev->sector_size = (u32 *)calloc(dev->max_lun, sizeof(u32));
	if(dev->sector_size == NULL)
	{
		retval = IPC_ENOMEM;
		goto free_and_return;
	}

	if(dev->max_lun == 0)
		dev->max_lun++;

	/* taken from linux usbstorage module (drivers/usb/storage/transport.c) */
	/*
	 * Some devices (i.e. Iomega Zip100) need this -- apparently
	 * the bulk pipes get STALLed when the GetMaxLUN request is
	 * processed.   This is, in theory, harmless to all other devices
	 * (regardless of if they stall or not).
	 */
	USB_ClearHalt(dev->usb_fd, dev->ep_in);
	USB_ClearHalt(dev->usb_fd, dev->ep_out);

free_and_return:
	if(response_buffer != NULL)
		iosFree(hId, response_buffer);
	if(retval < 0)
	{
		LWP_MutexDestroy(dev->lock);
		LWP_CondDestroy(dev->cond);
		free(dev->sector_size);
		memset(dev, 0, sizeof(*dev));
		return retval;
	}
	return 0;
}

s32 USBStorage_Close(usbstorage_handle *dev)
{
	USB_CloseDevice(&dev->usb_fd);
	LWP_MutexDestroy(dev->lock);
	LWP_CondDestroy(dev->cond);
	free(dev->sector_size);
	memset(dev, 0, sizeof(*dev));
	return 0;
}

s32 USBStorage_Reset(usbstorage_handle *dev)
{
	s32 retval;

	LWP_MutexLock(dev->lock);
	retval = __usbstorage_reset(dev);
	LWP_MutexUnlock(dev->lock);

	return retval;
}

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev)
{
	return dev->max_lun;
}

s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun)
{
	s32 retval;

	if(lun >= dev->max_lun)
		return IPC_EINVAL;

	retval = __usbstorage_clearerrors(dev, lun);
	if(retval < 0)
		return retval;

	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], NULL);
	return retval;
}

s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors)
{
	s32 retval;
	u8 cmd[] = {SCSI_READ_CAPACITY, lun << 5};
	u8 response[8];

	retval = __cycle(dev, lun, response, 8, cmd, 1, 0, NULL, NULL);
	if(retval >= 0)
	{
		if(n_sectors != NULL)
			memcpy(n_sectors, response, 4);
		if(sector_size != NULL)
			memcpy(sector_size, response + 4, 4);
		retval = USBSTORAGE_OK;
	}

	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_READ_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >>  8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
		};
	if(lun >= dev->max_lun || dev->sector_size[lun] == 0 || (u32)buffer % 32)
		return IPC_EINVAL;
	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_WRITE_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >> 8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
		};
	if(lun >= dev->max_lun || dev->sector_size[lun] == 0 || (u32)buffer % 32)
		return IPC_EINVAL;
	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Suspend(usbstorage_handle *dev)
{
	if(dev->suspended == 1)
		return USBSTORAGE_OK;

	USB_SuspendDevice(dev->usb_fd);
	dev->suspended = 1;

	return USBSTORAGE_OK;

}

#endif /* HW_RVL */
