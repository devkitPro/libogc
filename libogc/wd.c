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

#define DEFAULT_CHANNEL_BITMAP 0xfffe

extern void usleep(u32 t);

s32 wd_fd = -1;

u8 NCDcommonbuff[0x20] __attribute__((aligned(32)));

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
    
    IOS_Close(wd_fd);
    wd_fd = -1;
}

u8 WD_GetRadioLevel(BSSDescriptor* Bss) {
    if (Bss->RSSI >= 0xc4)
        return 3; // Strong

    if (Bss->RSSI >= 0xb5)
        return 2; // Normal

    if (Bss->RSSI >= 0xab)
        return 1; // Weak
    
    return 0; // Very Weak

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

u8 WD_GetNumberOfIEs(BSSDescriptor* Bss) {
    u8 ret = 0;

    u8* ptr = (u8*)Bss;
    IE_hdr* hdr = (IE_hdr*)&ptr[sizeof(BSSDescriptor)];
    u16 offset = 0;

    while(offset < Bss->IEs_length && hdr->len != 0 )
    {
        hdr = (IE_hdr*)&ptr[sizeof(BSSDescriptor) + offset];
        offset += hdr->len + sizeof(IE_hdr);
        ret++;
    }

    return ret;
}

int WD_GetIELength(BSSDescriptor* Bss, u8 ID) {
    u16 IEslen = Bss->IEs_length;

    u8* ptr = (u8*)Bss;
    IE_hdr* hdr = (IE_hdr*)&ptr[sizeof(BSSDescriptor)];
    u16 offset = 0;

    while(hdr->ID != ID && (offset + hdr->len) < IEslen && hdr->len != 0)
    {
        hdr = (IE_hdr*)&ptr[sizeof(BSSDescriptor) + offset];
        offset += hdr->len + sizeof(IE_hdr);
    }

    if(hdr->ID != ID) return -1;

    return hdr->len;
}

int WD_GetIE(BSSDescriptor* Bss, u8 ID, u8* buff, u8 buffsize) {
    if(!buff) return -2;

    u16 IEslen = Bss->IEs_length;

    u8* ptr = (u8*)Bss;
    IE_hdr* hdr = (IE_hdr*)&ptr[sizeof(BSSDescriptor)];
    u16 offset = 0;

    while(hdr->ID != ID && (offset + hdr->len) < IEslen && hdr->len != 0)
    {
        hdr = (IE_hdr*)&ptr[sizeof(BSSDescriptor) + offset];
        offset += hdr->len + sizeof(IE_hdr);
    }

    if(hdr->ID != ID) return -1;

    memset(buff, 0, buffsize);
    memcpy(buff, &ptr[offset + sizeof(BSSDescriptor) + sizeof(IE_hdr)], hdr->len);

    return 0;
}

#endif