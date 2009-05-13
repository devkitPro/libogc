/*-------------------------------------------------------------

usbmouse.c -- USB mouse support

Copyright (C) 2009
Daryl Borth (Tantric)

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

#include <string.h>
#include <malloc.h>

#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/usbmouse.h>

static lwp_queue _queue;

typedef struct {
	lwp_node node;
	mouse_event event;
} _node;

struct umouse {
	bool connected;

	s32 fd;

	u8 configuration;
	u32 interface;
	u32 altInterface;

	u8 ep;
	u32 ep_size;
};

static s32 hId = -1;
static struct umouse *_mouse;
static signed char *mousedata = 0;

#define	HEAP_SIZE					4096
#define DEVLIST_MAXSIZE				8

static lwpq_t _mouse_queue;

//Add an event to the event queue
static s32 _mouse_addEvent(const mouse_event *event) {
	_node *n = malloc(sizeof(_node));
	n->event = *event;

	__lwp_queue_append(&_queue, (lwp_node*) n);

	return 1;
}

// Event callback
static s32 _mouse_event_cb(s32 result,void *usrdata)
{
	mouse_event event;

	if (result>0)
	{
		event.button = mousedata[0];
		event.rx = mousedata[1];
		event.ry = mousedata[2];
		_mouse_addEvent(&event);
		USB_ReadIntrMsgAsync(_mouse->fd, 0x81, 4, mousedata, _mouse_event_cb, 0);
	}
	else
	{
		_mouse->connected = false;
		_mouse->fd = 0;
	}
	return 0;
}

//init the ioheap
static s32 USBMouse_Initialize(void)
{
	if (hId > 0)
		return 0;

	hId = iosCreateHeap(HEAP_SIZE);

	if (hId < 0)
		return IPC_ENOHEAP;

	return IPC_OK;
}

//Destroy the io heap
static s32 USBMouse_Deinitialize(void)
{
	if (hId < 0)
		return -1;

	s32 retval;
	retval = iosDestroyHeap(hId);
	hId = -1;

	return retval;
}

//Search for a mouse connected to the wii usb port
//Thanks to Sven Peter usbstorage support
static s32 USBMouse_Open()
{
	u8 *buffer;
	u8 dummy, i;
	u16 vid, pid;
	bool found = false;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	buffer = memalign(32, DEVLIST_MAXSIZE << 3);
	if(buffer == NULL)
		return -1;

	memset(buffer, 0, DEVLIST_MAXSIZE << 3);

	if (USB_GetDeviceList("/dev/usb/oh0", buffer, DEVLIST_MAXSIZE, 0, &dummy) < 0)
	{
		free(buffer);
		return -2;
	}

	if (_mouse) {
		USB_CloseDevice(&_mouse->fd);
	} else {
		_mouse = (struct umouse *) malloc(sizeof(struct umouse));

		if (!_mouse)
			return -1;
	}

	memset(_mouse, 0, sizeof(struct umouse));

	for (i = 0; i < DEVLIST_MAXSIZE; i++)
	{
		vid = *((u16 *) (buffer + (i << 3) + 4));
		pid = *((u16 *) (buffer + (i << 3) + 6));

		if ((vid == 0) || (pid == 0))
			continue;

		s32 fd = 0;
		if (USB_OpenDevice("oh0", vid, pid, &fd) < 0)
			continue;

		USB_GetDescriptors(fd, &udd);
		for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
		{
			ucd = &udd.configurations[iConf];

			for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
			{
				uid = &ucd->interfaces[iInterface];

				if ((uid->bInterfaceClass == USB_CLASS_HID) &&
					(uid->bInterfaceSubClass == USB_SUBCLASS_BOOT) &&
					(uid->bInterfaceProtocol== USB_PROTOCOL_MOUSE))
				{
					for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
					{
						ued = &uid->endpoints[iEp];

						if (ued->bmAttributes != USB_ENDPOINT_INTERRUPT)
							continue;

						if (!(ued->bEndpointAddress & USB_ENDPOINT_IN))
							continue;

						_mouse->fd = fd;

						_mouse->configuration = ucd->bConfigurationValue;
						_mouse->interface = uid->bInterfaceNumber;
						_mouse->altInterface = uid->bAlternateSetting;

						_mouse->ep = ued->bEndpointAddress;
						_mouse->ep_size = ued->wMaxPacketSize;

						found = true;

						break;
					}
				}

				if (found)
					break;
			}

			if (found)
				break;
		}

		USB_FreeDescriptors(&udd);

		if (found)
			break;
		else
			USB_CloseDevice(&fd);
	}

	free(buffer);

	if (!found)
		return -3;

	//set boot protocol
	USB_WriteCtrlMsg(_mouse->fd, USB_REQTYPE_SET, USB_REQ_SETPROTOCOL, 0, 0, 0, 0);
	USB_ReadIntrMsgAsync(_mouse->fd, 0x81, 4, mousedata, _mouse_event_cb, 0);
	_mouse->connected = true;
	return 1;
}

//Close the device
static void USBMouse_Close(void)
{
	if (!_mouse)
		return;

	USB_CloseDevice(&_mouse->fd);

	free(_mouse);
	_mouse = NULL;

	return;
}

//Initialize USB and USB_MOUSE and the event queue
s32 MOUSE_Init(void)
{
	if(mousedata) free(mousedata);
	mousedata = (signed char*) iosAlloc(32, 20);

	if (USB_Initialize() != IPC_OK)
		return -1;

	if (USBMouse_Initialize() != IPC_OK) {
		return -2;
	}

	if(!USBMouse_Open())
		return -3;

	__lwp_queue_initialize(&_queue, 0, 0, 0);
	LWP_InitQueue(&_mouse_queue);

	return 0;
}

// Deinitialize USB_MOUSE and the event queue
s32 MOUSE_Deinit(void)
{
	LWP_ThreadBroadcast(_mouse_queue);
	LWP_CloseQueue(_mouse_queue);

	USBMouse_Close();
	MOUSE_FlushEvents();
	USBMouse_Deinitialize();
	if(mousedata) free(mousedata);
	return 1;
}

//Get the first event of the event queue
s32 MOUSE_GetEvent(mouse_event *event)
{
	_node *n = (_node *) __lwp_queue_get(&_queue);

	if (!n)
		return 0;

	*event = n->event;

	free(n);

	return 1;
}

//Flush all pending events
s32 MOUSE_FlushEvents(void)
{
	s32 res;
	_node *n;

	res = 0;
	while (true) {
		n = (_node *) __lwp_queue_get(&_queue);

		if (!n)
			break;

		free(n);
		res++;
	}

	return res;
}

#endif
