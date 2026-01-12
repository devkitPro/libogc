/*-----------------------------------------------------------------------

    wd.h -- Wireless Driver Implementation.

    Copyright:
    
        - (C) 2025-2026 B. Abdelali (Abdelali221) (Author)
        - (C) 2008 Dolphin Emulator Project (For providing data structures)

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

#ifndef __WD_H_
#define __WD_H_
#include <gccore.h>

#define SSID_LENGTH 32
#define BSSID_LENGTH 6

enum MODES
{
    Unintialized = 0,
    DSCommunications = 1,
    NA0 = 2,
    AOSSAPScan = 3,
    NA1 = 4,
    NA2 = 5,
    NA3 = 6
};

typedef struct WDInfo
{
    u8 MAC[6];    
    u16 EnableChannelsMask;
    u16 NTRallowedChannelsMask;
    u8 CountryCode[4];
    u8 channel;
    u8 initialized;
    u8 version[80];
    u8 unknown[48];
} WDInfo;

typedef struct ScanParameters
{
    u16         ChannelBitmap;
    u16         MaxChannelTime;
    u8          BSSID[BSSID_LENGTH];
    u16         ScanType;
    
    u16         SSIDLength;
    u8          SSID[SSID_LENGTH];
    u8          SSIDMatchMask[6];

} ScanParameters;

typedef struct BSSDescriptor
{
    u16         length;
    u16         RSSI;
    u8          BSSID[6];
    u16         SSIDLength;
    u8          SSID[32];
    u16         Capabilities;
    struct
    {
        u16     basic;
        u16     support;
    } rateSet;
    u16 beacon_period;
    u16 DTIM_period;
    u16 channel;
    u16 CF_period;
    u16 CF_max_duration;
    u16 element_info_length;
    u16 element_info[1];
}  BSSDescriptor;

s32 NCD_LockWirelessDriver();
u32 NCD_UnlockWirelessDriver(s32 lockid);
int WD_Init(u8 mode);
void WD_Deinit();
int WD_GetInfo(WDInfo* inf);
u8 WD_GetRadioLevel(BSSDescriptor* Bss);
int WD_Scan(ScanParameters *settings, u8* buff, u16 buffsize);
int WD_ScanOnce(ScanParameters *settings, u8* buff, u16 buffsize);
void WD_SetDefaultScanParameters(ScanParameters* set);

#endif