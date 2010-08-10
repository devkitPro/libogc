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
#include <unistd.h>

#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/lwp_queue.h>
#include <ogc/usbmouse.h>

#define MOUSE_THREAD_STACKSIZE		(1024 * 4)
#define MOUSE_THREAD_PRIO			65
#define MOUSE_THREAD_UDELAY			(1000 * 1000 * 3)

#define MOUSE_MAX_DATA				20

#define	HEAP_SIZE					4096
#define DEVLIST_MAXSIZE				8

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

static lwp_queue _queue;
static lwpq_t _mouse_queue;

static s32 hId = -1;
static bool _mouse_is_inited = false;
static lwp_t _mouse_thread = LWP_THREAD_NULL;
static bool _mouse_thread_running = false;
static bool _mouse_thread_quit = false;
static struct umouse *_mouse = NULL;
static s8 *_mousedata = NULL;

static u8 _mouse_stack[MOUSE_THREAD_STACKSIZE] ATTRIBUTE_ALIGN(8);

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
		event.button = _mousedata[0];
		event.rx = _mousedata[1];
		event.ry = _mousedata[2];
		_mouse_addEvent(&event);
		USB_ReadIntrMsgAsync(_mouse->fd, _mouse->ep, _mouse->ep_size, _mousedata, _mouse_event_cb, 0);
	}
	else
	{
		_mouse->connected = false;
	}
	return 0;
}

//Callback when the mouse is disconnected
static s32 _disconnect(s32 retval, void *data)
{
	_mouse->connected = false;
	return 1;
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

//Close the device
static void USBMouse_Close(void)
{
	if (_mouse && _mouse->fd != -1) {
		USB_CloseDevice(&_mouse->fd);
		_mouse->fd = -1;
	}
}

//Search for a mouse connected to the wii usb port
//Thanks to Sven Peter usbstorage support
static s32 USBMouse_Open()
{
	usb_device_entry *buffer;
	u8 device_count, i;
	u16 vid, pid;
	bool found = false;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	buffer = iosAlloc(hId, DEVLIST_MAXSIZE * sizeof(usb_device_entry));
	if(buffer == NULL)
		return -1;

	memset(buffer, 0, DEVLIST_MAXSIZE * sizeof(usb_device_entry));

	if (USB_GetDeviceList(buffer, DEVLIST_MAXSIZE, USB_CLASS_HID, &device_count) < 0)
	{
		iosFree(hId,buffer);
		return -2;
	}

	for (i = 0; i < device_count; i++)
	{
		vid = buffer[i].vid;
		pid = buffer[i].pid;

		if ((vid == 0) || (pid == 0))
			continue;

		s32 fd = 0;
		if (USB_OpenDevice(buffer[i].device_id, vid, pid, &fd) < 0)
			continue;

		if (USB_GetDescriptors(fd, &udd) < 0) {
			USB_CloseDevice(&fd);
			continue;
		}

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

						if (ued->wMaxPacketSize > MOUSE_MAX_DATA)
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

	iosFree(hId,buffer);

	if (!found)
		return -3;

	if (USB_DeviceRemovalNotifyAsync(_mouse->fd, &_disconnect, NULL) < 0)
	{
		USBMouse_Close();
		return -8;
	}

	//set boot protocol
	USB_WriteCtrlMsg(_mouse->fd, USB_REQTYPE_SET, USB_REQ_SETPROTOCOL, 0, _mouse->interface, 0, NULL);
	USB_ReadIntrMsgAsync(_mouse->fd, _mouse->ep, _mouse->ep_size, _mousedata, _mouse_event_cb, NULL);
	_mouse->connected = true;
	return 1;
}

bool MOUSE_IsConnected(void)
{
	if (!_mouse) return false;
	return _mouse->connected;
}

static void * _mouse_thread_func(void *arg)
{
	while (!_mouse_thread_quit)
	{
		// scan for new attached mice
		if (!MOUSE_IsConnected())
		{
			USBMouse_Close();
			USBMouse_Open();
		}
		usleep(MOUSE_THREAD_UDELAY);
	}
	return NULL;
}

//Initialize USB and USB_MOUSE and the event queue
s32 MOUSE_Init(void)
{
	if(_mouse_is_inited) return 0;

	if (USB_Initialize() != IPC_OK)
		return -1;

	if (USBMouse_Initialize() != IPC_OK) {
		return -2;
	}

	_mousedata = (s8*)iosAlloc(hId,MOUSE_MAX_DATA);
	_mouse = (struct umouse *) malloc(sizeof(struct umouse));
	memset(_mouse, 0, sizeof(struct umouse));
	_mouse->fd = -1;

	if (!_mouse_thread_running)
	{
		// start the mouse thread
		_mouse_thread_quit = false;
		memset(_mouse_stack, 0, MOUSE_THREAD_STACKSIZE);

		s32 res = LWP_CreateThread(&_mouse_thread, _mouse_thread_func, NULL,
									_mouse_stack, MOUSE_THREAD_STACKSIZE,
									MOUSE_THREAD_PRIO);

		if (res)
		{
			USBMouse_Close();
			MOUSE_FlushEvents();
			USBMouse_Deinitialize();
			_mouse_thread_running = false;
			return -6;
		}
		_mouse_thread_running = true;
	}

	__lwp_queue_init_empty(&_queue);
	LWP_InitQueue(&_mouse_queue);
	_mouse_is_inited = true;
	return 0;
}

// Deinitialize USB_MOUSE and the event queue
s32 MOUSE_Deinit(void)
{
	if(!_mouse_is_inited) return 1;

	if (_mouse_thread_running) {
		_mouse_thread_quit = true;
		LWP_JoinThread(_mouse_thread, NULL);
		_mouse_thread_running = false;
	}

	LWP_ThreadBroadcast(_mouse_queue);
	LWP_CloseQueue(_mouse_queue);

	USBMouse_Close();
	MOUSE_FlushEvents();
	USBMouse_Deinitialize();
	if(_mousedata!=NULL) iosFree(hId,_mousedata);
	if(_mouse!=NULL) free(_mouse);
	_mouse_is_inited = false;
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
