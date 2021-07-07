/*-------------------------------------------------------------

usbcontroller.c -- Usb controller support

Copyright (C) 2020
Joris Vermeylen info@dacotaco.com
DacoTaco

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
#include <ogc/usbcontroller.h>

#define USB_REPORT_USAGE_CONSUMER_CONTROL	0x01
#define USB_REPORT_USAGE_PAGE_BUTTON    	0x09
#define USB_REPORT_USAGE_JOYSTICK       	0x04
#define USB_REPORT_USAGE_GAMEPAD        	0x05
#define USB_REPORT_USAGE_MULTIAXIS			0x08
#define USB_REPORT_USAGE_X					0x30
#define USB_REPORT_USAGE_Y					0x31
#define USB_REPORT_USAGE_Z					0x32
#define USB_REPORT_USAGE_Rx					0x33
#define USB_REPORT_USAGE_Ry					0x34
#define USB_REPORT_USAGE_Rz					0x35
#define USB_REPORT_USAGE_HAT				0x39

#define	HEAP_SIZE							0x1000
#define DEVLIST_MAXSIZE						0x08
#define MAX_INPUTSIZE						0x1E

typedef struct
{
    inputType type;
    u8 subType;
    u32 type_number;
    u32 logicalMinimum;
    u32 logicalMaximum;
    u8 bitLength;
    u16 prevReadData;
} input;

struct uctrlr {
	bool connected;
	
	s32 fd;

	eventcallback cb;
	
	u8 configuration;
	u32 interface;
	u32 altInterface;	
	s16 reportSize;

	u8 ep;
	u16 ep_size;
};

static u32 _input_cnt = 0;
static u32 button_cnt = 0;
static s32 hId = -1;
static struct uctrlr *_ctrlr = NULL;
static input* _inputs = NULL;

static void _submit(USBController_eventType eventType, u8 input_index, u16 value)
{
	if (!_ctrlr->cb)
		return;

	USBController_event ev;
	ev.eventType = eventType;
	if(eventType != USBCONTROLLER_DISCONNECTED)
	{
		ev.inputType = _inputs[input_index].type;
		ev.number = _inputs[input_index].type_number;
	}
	ev.value = value;

	_ctrlr->cb(ev);
}

//Callback when the keyboard is disconnected
static s32 _disconnect(s32 retval, void *data)
{
	(void) data;
	_ctrlr->connected = false;
	_submit(USBCONTROLLER_DISCONNECTED, 0, 0);

	return 1;
}

static s32 _getReportDescriptorSize(s32 fd, u8 interface)
{
	return USB_GetReportDescriptorSize(fd, interface);
}

static s32 _getReportDescriptor(s32 fd, u8 interface, void* buffer, u16 size)
{
	return USB_GetReportDescriptor(fd, interface, buffer, size);
}

static s8 _parseInputData(u32 usageMax, u32 reportCount, u32 reportSize, u32 logicalMinimum, u32 logicalMaximum, inputType type, u32 unitFlag, u32 inputFlags)
{
	if(_input_cnt >= MAX_INPUTSIZE)
		return -1;
	
    if (reportCount == 0)
        return -2;
	
    //we don't know how to interpet the data if the usagemax isn't the same as the report's data size
    if (usageMax > 0 && usageMax != reportCount)
        type = UNKNOWN;    

    for (u32 i = 0; i < reportCount; i++)
    {
        input _input;

        _input.bitLength = reportSize;
        _input.logicalMinimum = logicalMinimum;
        _input.logicalMaximum = logicalMaximum;
        _input.type = type;
        _input.type_number = (type == BUTTON?++button_cnt:0);

        _inputs[_input_cnt] = _input;
        _input_cnt++;
		
		if(_input_cnt >= MAX_INPUTSIZE)
			break;
    }

    return 0;
}

static u32 _getReportValue(u8* data, u8 size)
{
    if (data == NULL)
        return 0;

    u32 value = 0;
    for (u8 i = 0; i < size; i++)
        value |= data[i] << (i * 8);

    return value;
}

static s16 _processReport(u8* buf, u16 reportBuffSize)
{
    if (!buf)
        return -1;

    //Actual start
    if (buf[0] != 0x05 ||
        ( buf[1] != 0x01 && buf[1] != 0x05 && buf[1] != 0x0C ) ||
        (buf[2] & 0xFC) != USB_REPORT_USAGE ||
        (buf[3] != USB_REPORT_USAGE_JOYSTICK && buf[3] != USB_REPORT_USAGE_GAMEPAD && buf[3] != USB_REPORT_USAGE_MULTIAXIS && buf[3] != USB_REPORT_USAGE_CONSUMER_CONTROL))
    {
		//not a valid HID header
		//printf("header : 0x%02X%02X%02X%02X", buf[0], buf[1], buf[2], buf[3]);
        return -2;
    }

	u16 processed = 4;
    u8* report = buf + processed; 
    u32 logicalMax = 0, logicalMin = 0;
    u32 reportSize = 1, reportCount = 0;
    u32 usageMax = 0;
    inputType type = UNKNOWN;
    u32 unit = 0;
    u32 value = 0;
	s16 bitCount = 0;

    while (processed < reportBuffSize)
    {
		//a command is split into 2 parts. the command & the data size.
        u8 data_type = report[0] & 0xFC;
        u8 data_size = report[0] & 0x03;
        report++;
        processed++;
		
        switch (data_type)
        {
            //what input are we talking about here?
			case USB_REPORT_USAGE_PAGE:
				value = _getReportValue(report, data_size);
				if (value != USB_REPORT_USAGE_PAGE_BUTTON)
					type = UNKNOWN;
				else
					type = BUTTON;
				
				break;
			case USB_REPORT_USAGE:
				value = _getReportValue(report, data_size);
				switch (value)
				{
					case USB_REPORT_USAGE_HAT:
						type = HAT;
						break;
					case USB_REPORT_USAGE_Rx:
					case USB_REPORT_USAGE_Ry:
					case USB_REPORT_USAGE_Rz:
					case USB_REPORT_USAGE_X:
					case USB_REPORT_USAGE_Y:
					case USB_REPORT_USAGE_Z:
						type = AXIS;
						break;
					default:
						type = UNKNOWN;
						break;
				}

				break;
			case USB_REPORT_LOGICAL_MAXIMUM:
				logicalMax = _getReportValue(report, data_size);
				break;
			case USB_REPORT_LOGICAL_MINIMUM:
				logicalMin = _getReportValue(report, data_size);
				break;
			case USB_REPORT_REPORT_SIZE:
				reportSize = _getReportValue(report, data_size);
				break;
			case USB_REPORT_REPORT_COUNT:
				reportCount = _getReportValue(report, data_size);
				break;
			case USB_REPORT_USAGE_MAXIMUM:
				usageMax = _getReportValue(report, data_size);
				break;
			case USB_REPORT_UNIT:
				unit = _getReportValue(report, data_size);
				break;
			case USB_REPORT_INPUT:
				value = _getReportValue(report, data_size);
				bitCount += (reportCount*reportSize);
				_parseInputData(usageMax, reportCount, reportSize, logicalMin, logicalMax, type, unit, value);

				//reset everything, we have parsed this one
				logicalMax = logicalMin = reportCount = unit = 0;
				reportSize = 1;
				usageMax = 0;
				type = UNKNOWN;
				break;

				/*
					commands that we ignore. these have no use in how we process these commands but are important to the descriptor
				*/
			case USB_REPORT_USAGE_MINIMUM:
			case USB_REPORT_FEATURE:
			case USB_REPORT_OUTPUT:
			case USB_REPORT_PHYSICAL_MAXIMUM:
			case USB_REPORT_PHYSICAL_MINIMUM:
			case USB_REPORT_COLLECTION_START:
				break;
			case USB_REPORT_COLLECTION_END:
				//unsure what to do when data_size contains a number. the specs say its not done, but ...
				if (data_size != 0)
					goto end_loop;
				break;
			//if we come across unknown data we bail out.
			default:
				//printf("unknown data type 0x%02X",data_type);
				break;
        }

        //move to new data
        report += data_size;
        processed += data_size;
    }

end_loop:
    if (processed != reportBuffSize)
        return -3;

    return bitCount / 8 + (bitCount % 8 > 0);
}

s32 _processReportData(u8* buf, u16 data_size)
{
    if (!buf)
        return -1;

    u8* data = (u8*) iosAlloc(hId, data_size);
	if (data == NULL)
		return -2;
	
	//copy data over so we play around with it
	memcpy(data, buf, data_size);

    u32 data_index = 0;
    s8 shift = 0;
    for (u32 i = 0; i < _input_cnt; i++)
    {
        u16 readData = 0;
        s8 pressed = 0;

        //read data from stream
        for (s32 y = 0; y < _inputs[i].bitLength && data_index < data_size; y++)
        {
            readData |= ((data[data_index] & (0x01 << shift)) >> shift) << y;
            shift++;
			
            if (shift > 7)
            {
                shift = 0;
                data_index++;
            }
        }

        if (readData == _inputs[i].prevReadData)
			continue;
		
		if (_inputs[i].type == HAT)
		{
			if (_inputs[i].prevReadData <= _inputs[i].logicalMaximum && _inputs[i].prevReadData >= _inputs[i].logicalMinimum)
				pressed = (readData <= _inputs[i].logicalMaximum && readData >= _inputs[i].logicalMinimum);
			else
				pressed = 1;
		}
		else if (_inputs[i].type == AXIS)
		{
			// AXIS / throttles , how do they work? ¯\_(ツ)_/¯
			continue;
		}
		else
		{
			if (readData > _inputs[i].logicalMaximum || readData < _inputs[i].logicalMinimum)
				readData = 0;

			pressed = readData > _inputs[i].logicalMinimum;
		}
        
		_submit(pressed == 1?USBCONTROLLER_PRESSED:USBCONTROLLER_RELEASED, i, readData);
        _inputs[i].prevReadData = readData;
    }

    if (data != NULL)
    {
        iosFree(hId, data);
        data = NULL;
    }
	
	return 0;
}

//init the ioheap
s32 USBController_Initialize(void)
{
	if (hId > 0)
		return IPC_OK;

	hId = iosCreateHeap(HEAP_SIZE);

	if (hId <= 0)
		return IPC_ENOHEAP;

	return IPC_OK;
}

s32 USBController_Deinitialize(void)
{
	return IPC_OK;
}

//Search for a controller that is connected to the wii usb port
s32 USBController_Open(const eventcallback cb)
{
	usb_device_entry *buffer;
	u8 device_count, i, conf;
	u16 vid, pid;
	bool found = false;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	buffer = (usb_device_entry*)iosAlloc(hId, DEVLIST_MAXSIZE * sizeof(usb_device_entry));
	if(buffer == NULL)
		return -1;

	memset(buffer, 0, DEVLIST_MAXSIZE * sizeof(usb_device_entry));

	if (USB_GetDeviceList(buffer, DEVLIST_MAXSIZE, USB_CLASS_HID, &device_count) < 0)
	{
		iosFree(hId, buffer);
		return -2;
	}

	if (_ctrlr) {
		if (_ctrlr->fd != -1) USB_CloseDevice(&_ctrlr->fd);
	} else {
		_ctrlr = (struct uctrlr *) malloc(sizeof(struct uctrlr));

		if (!_ctrlr)
			return -3;
	}
	
	if(!_inputs)
	{
		_inputs = (input*) malloc(sizeof(input)*MAX_INPUTSIZE);
		if(!_inputs)
			return -4;
	}

	memset(_ctrlr, 0, sizeof(struct uctrlr));
	_input_cnt = 0;
	_ctrlr->fd = -1;
	memset(_inputs, 0, sizeof(input)*MAX_INPUTSIZE);

	//printf("device_count : %d",device_count);
	if(device_count <= 0)
		return -5;
	
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
				
				//printf("device(%d) : %d - %d - %d", i, uid->bInterfaceClass, uid->bInterfaceSubClass, uid->bInterfaceProtocol);				
				if ((uid->bInterfaceClass == USB_CLASS_HID) &&
					(uid->bInterfaceSubClass == USB_SUBCLASS_NONE) &&
					(uid->bInterfaceProtocol== USB_PROTOCOL_NONE))
				{
					for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
					{
						ued = &uid->endpoints[iEp];

						if (ued->bmAttributes != USB_ENDPOINT_INTERRUPT)
							continue;

						if (!(ued->bEndpointAddress & USB_ENDPOINT_IN))
							continue;
						
						//get the report descriptor and process it
						s32 descriptorSize = _getReportDescriptorSize(fd, uid->bInterfaceNumber);
						if(descriptorSize < 0)
							continue;								
						
						u8* descriptor = (u8*)iosAlloc(hId, descriptorSize);
						if(descriptor == NULL)
							return -6;
						memset(descriptor, 0, descriptorSize);
						
						if( _getReportDescriptor(fd, uid->bInterfaceNumber, descriptor, descriptorSize) < 0)
						{
							iosFree(hId, descriptor);
							continue;
						}
						
						_ctrlr->reportSize = _processReport(descriptor,descriptorSize);
						if( _ctrlr->reportSize < 0)
						{	
							_ctrlr->reportSize = 0;	
							memset(_inputs, 0, sizeof(input)*MAX_INPUTSIZE);
							iosFree(hId, descriptor);
							continue;
						}
						
						iosFree(hId, descriptor);

						_ctrlr->fd = fd;
						_ctrlr->cb = cb;

						_ctrlr->configuration = ucd->bConfigurationValue;
						_ctrlr->interface = uid->bInterfaceNumber;
						_ctrlr->altInterface = uid->bAlternateSetting;

						_ctrlr->ep = ued->bEndpointAddress;						
						_ctrlr->ep_size = ued->wMaxPacketSize;

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

	iosFree(hId, buffer);

	if (!found)
		return -7;
	
	if (USB_GetConfiguration(_ctrlr->fd, &conf) < 0)
	{
		USBController_Close();
		return -8;
	}

	if (conf != _ctrlr->configuration &&
		USB_SetConfiguration(_ctrlr->fd, _ctrlr->configuration) < 0)
	{
		USBController_Close();
		return -9;
	}

	if (_ctrlr->altInterface != 0 &&
		USB_SetAlternativeInterface(_ctrlr->fd, _ctrlr->interface, _ctrlr->altInterface) < 0)
	{
		USBController_Close();
		return -10;
	}

	if (USB_DeviceRemovalNotifyAsync(_ctrlr->fd, &_disconnect, NULL) < 0)
	{
		USBController_Close();
		return -11;
	}

	_ctrlr->connected = true;

	return 1;
}

//Close the device
void USBController_Close(void)
{
	if (!_ctrlr)
		return;

	if(_ctrlr->fd != -1)
	{
		USB_ClearHalt( _ctrlr->fd, _ctrlr->ep );
		USB_CloseDevice(&_ctrlr->fd);
	}

	free(_ctrlr);
	_ctrlr = NULL;
	return;
}

bool USBController_IsConnected(void)
{
	if (!_ctrlr)
		return false;
	
	return _ctrlr->connected;
}

s32 USBController_GetDescriptorSize(void)
{	
	return _getReportDescriptorSize(_ctrlr->fd, _ctrlr->interface);
}

s32 USBController_GetDescriptor(void* buffer, u16 size)
{
	return USB_GetReportDescriptor(_ctrlr->fd, _ctrlr->interface, buffer, size);
}

s32 USBController_Scan(void)
{
	u8 *buffer = 0;

	if(!_ctrlr || _ctrlr->fd==-1) return -1;
	buffer = iosAlloc(hId, _ctrlr->reportSize);

	if (buffer == NULL)
		return -1;

	if( USB_ReadIntrMsg(_ctrlr->fd, _ctrlr->ep, _ctrlr->reportSize, buffer) < 0)
	{
		iosFree(hId, buffer);
		return -2;
	}
	
	s32 ret = _processReportData(buffer, _ctrlr->reportSize );
	
	iosFree(hId, buffer);
	
	return ret;
}

#endif