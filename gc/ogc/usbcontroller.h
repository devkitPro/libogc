/*-------------------------------------------------------------

usbcontroller.h -- Usb controller support

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

#ifndef __USBCONTROLLER_H__
#define __USBCONTROLLER_H__

#if defined(HW_RVL)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef enum
{
	USBCONTROLLER_PRESSED = 0,
	USBCONTROLLER_RELEASED,
	USBCONTROLLER_DISCONNECTED
} USBController_eventType;

typedef enum 
{
    UNKNOWN = 0,
    NONE,
    BUTTON,
    HAT,
    AXIS
} inputType;

typedef struct
{
	USBController_eventType eventType;
	inputType inputType;
	u16 number;
	u16 value;
} USBController_event;

typedef void (*eventcallback) (USBController_event event);

s32 USBController_Initialize(void);
s32 USBController_Deinitialize(void);

s32 USBController_Open(const eventcallback cb);
void USBController_Close(void);

bool USBController_IsConnected(void);
s32 USBController_Scan(void);

s32 USBController_GetDescriptorSize();
s32 USBController_GetDescriptor(void* buffer, u16 size);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* __USBCONTROLLER_H__ */

#endif /* Wii mode */