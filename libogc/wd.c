/*-----------------------------------------------------------------------

    wd.h -- Wireless Driver Implementation.

    Copyright (C) 2025-2026 B. Abdelali (Abdelali221)

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

-----------------------------------------------------------------------*/

#if defined(HW_RVL)

#include "wd.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <string.h>
#include <ogc/machine/processor.h>

//#define WD_HEAP_SIZE 0x1000

#define DEFAULT_CHANNEL_BITMAP 0xfffe

enum WDIOCTLV
{
    IOCTLV_WD_INVALID = 0x1000,
    IOCTLV_WD_GET_MODE = 0x1001,          // WD_GetMode
    IOCTLV_WD_SET_LINKSTATE = 0x1002,     // WD_SetLinkState
    IOCTLV_WD_GET_LINKSTATE = 0x1003,     // WD_GetLinkState
    IOCTLV_WD_SET_CONFIG = 0x1004,        // WD_SetConfig
    IOCTLV_WD_GET_CONFIG = 0x1005,        // WD_GetConfig
    IOCTLV_WD_CHANGE_BEACON = 0x1006,     // WD_ChangeBeacon
    IOCTLV_WD_DISASSOC = 0x1007,          // WD_DisAssoc
    IOCTLV_WD_MP_SEND_FRAME = 0x1008,     // WD_MpSendFrame
    IOCTLV_WD_SEND_FRAME = 0x1009,        // WD_SendFrame
    IOCTLV_WD_SCAN = 0x100a,              // WD_Scan
    IOCTLV_WD_CALL_WL = 0x100c,           // WD_CallWL
    IOCTLV_WD_MEASURE_CHANNEL = 0x100b,   // WD_MeasureChannel
    IOCTLV_WD_GET_LASTERROR = 0x100d,     // WD_GetLastError
    IOCTLV_WD_GET_INFO = 0x100e,          // WD_GetInfo
    IOCTLV_WD_CHANGE_GAMEINFO = 0x100f,   // WD_ChangeGameInfo
    IOCTLV_WD_CHANGE_VTSF = 0x1010,       // WD_ChangeVTSF
    IOCTLV_WD_RECV_FRAME = 0x8000,        // WD_ReceiveFrame
    IOCTLV_WD_RECV_NOTIFICATION = 0x8001  // WD_ReceiveNotification
};

extern void usleep(u32 t);

s32 wd_fd = -1;
s32 wd_heap = -1;

s32* WD_GetWork() {
  return &wd_fd;
}

u8 NCDcommonbuff[0x20] __attribute__((aligned(32)));
//s32 NCDheap; 

s32 NCD_LockWirelessDriver() {
    s32 NCD = IOS_Open("/dev/net/ncd/manage", 0);
    
    memset(NCDcommonbuff, 0, 0x20);
    
    ioctlv vector = {0};
    vector.data = NCDcommonbuff;
    vector.len = 0x20;

    IOS_Ioctlv(NCD, 1, 0, 1, &vector);
    
    s32 lockid = 0;
    memcpy(&lockid, NCDcommonbuff, 4);
    
    IOS_Close(NCD);
    NCD = -1;
    
    return lockid;
}

u32 NCD_UnlockWirelessDriver(s32 lockid) {
    s32 NCD = IOS_Open("/dev/net/ncd/manage", 0);
    if (NCD < 0) return -1;
    
    u8 NCDresult[0x20] __attribute__((aligned(32)));
    memcpy(NCDresult, &lockid, 4);
    
    ioctlv vectors[2] = {0};
    vectors[1].data = NCDresult;
    vectors[1].len = 4;
    vectors[0].data = NCDcommonbuff;
    vectors[0].len = 0x20;
    
    IOS_Ioctlv(NCD, 2, 1, 1, vectors);
    
    u32 ret;
    memcpy(&ret, NCDresult, 4);
    
    IOS_Close(NCD);
    
    NCD = -1;
    return ret;
}
/*
s32 WD_CreateHeap() { // Currently unused.
    s32 heap;
    u32 level;
    
    _CPU_ISR_Disable(level);
    heap = iosCreateHeap(WD_HEAP_SIZE);
    _CPU_ISR_Restore(level);
    
    return heap;
}
*/

void WD_SetDefaultScanParameters(ScanParameters* set) {
    set->ChannelBitmap = DEFAULT_CHANNEL_BITMAP;
    set->MaxChannelTime = 100;

    memset(set->BSSID, 0xff, BSSID_LENGTH);
 
    set->ScanType = 0;
    set->SSIDLength = 0;
    
    memset(set->SSID, 0x00, SSID_LENGTH);
    memset(set->SSIDMatchMask, 0xff, 6);
}

int WD_Init(u8 mode) {
    if(wd_fd < 0) {
        wd_fd = IOS_Open("/dev/net/wd/command", 0x10000 | mode);
        if (wd_fd < 0) return -1;
    }
    return 0;
}

void WD_Deinit() {
    if(wd_fd < 0) return;
    
    u32 level;
    
    _CPU_ISR_Disable(level);
    IOS_Close(*WD_GetWork());
    wd_fd = -1;
    _CPU_ISR_Restore(level);
}

u8 WD_GetRadioLevel(BSSDescriptor* Bss) {
    u8 ret;
    
    if (Bss->RSSI < 0xc4) {
        if (Bss->RSSI < 0xb5) {
            if (Bss->RSSI < 0xab) {
                ret = 0; // Very Weak
            } else {
                ret = 1; // Weak
            }
        } else {
            ret = 2; // Medium
        }
    } else {
        ret = 3; // Strong
    }
    return ret;
}

int WD_GetInfo(WDInfo* info) {
    s32 lockid = NCD_LockWirelessDriver();
    
    if(WD_Init(AOSSAPScan) < 0) return -1;
    
    u8 inf[sizeof(WDInfo)] __attribute__((aligned(32)));
    
    ioctlv vector;
    vector.data = inf;
    vector.len = sizeof(WDInfo);
	
    IOS_Ioctlv(wd_fd, IOCTLV_WD_GET_INFO, 0, 1, &vector);
    memcpy(info, inf, sizeof(WDInfo));

    WD_Deinit();
    NCD_UnlockWirelessDriver(lockid);
    
    return 0;
}

int WD_Scan(ScanParameters *settings, u8* buff, u16 buffsize) {
    if(wd_fd < 0) return -1;

    u8 buf[buffsize + 2] __attribute__((aligned(32)));
    u8 settingsbuf[0x4e] __attribute__((aligned(32)));
    
    memset(settingsbuf, 0, 0x4e);
    memset(buf, 0, buffsize + 2);
    memcpy(settingsbuf, settings, 0x1a);
     
    ioctlv vectors[2];
    vectors[0].data = settingsbuf;
    vectors[0].len = 0x4e;
    vectors[1].data = buf;
    vectors[1].len = buffsize;
    
    IOS_Ioctlv(wd_fd, IOCTLV_WD_SCAN, 1, 1, vectors);
    usleep(100000);
    memcpy(buff, buf, buffsize);
	
    return 0;
}

int WD_ScanOnce(ScanParameters *settings, u8* buff, u16 buffsize) {
    s32 lockid = NCD_LockWirelessDriver();
    
    if(WD_Init(AOSSAPScan) < 0) return -1;
    
    WD_Scan(settings, buff, buffsize);
    
    WD_Deinit();
    NCD_UnlockWirelessDriver(lockid);
    
    return 0;
}

#endif