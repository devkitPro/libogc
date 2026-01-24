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

// Error Codes :

#define WD_SUCCESS 0
#define WD_UINITIALIZED -1
#define WD_INVALIDBUFF -2
#define WD_BUFFTOOSMALL -3
#define WD_NOTFOUND -4

// Capability flags :

#define CAPAB_SECURED_FLAG 0x10

// Information Elements IDs :

#define IEID_SSID 0x0
#define IEID_COUNTRY 0x7
#define IEID_SECURITY_RSN 0x30
#define IEID_VENDORSPECIFIC 0xDD

// Signal Strength :

#define WD_SIGNAL_STRONG 3
#define WD_SIGNAL_NORMAL 2
#define WD_SIGNAL_FAIR 1
#define WD_SIGNAL_WEAK 0

// WD Modes :

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

// WD Information :

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

// Scan parameters :

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

// BSS Descriptor :

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
    u16 IEs_length;
} BSSDescriptor;

// Information Element Header :

typedef struct IE_hdr
{
    u8 ID;
    u8 len;
} IE_hdr;

// General Purpose :

s32 NCD_LockWirelessDriver();
u32 NCD_UnlockWirelessDriver(s32 lockid);
int WD_Init(u8 mode);
void WD_Deinit();
int WD_GetInfo(WDInfo* inf);

// AP Scan related : 

u8 WD_GetRadioLevel(BSSDescriptor* Bss);
int WD_Scan(ScanParameters *settings, u8* buff, u16 buffsize);
int WD_ScanOnce(ScanParameters *settings, u8* buff, u16 buffsize);
void WD_SetDefaultScanParameters(ScanParameters* set);

// IE related :

u8 WD_GetNumberOfIEs(BSSDescriptor* Bss);
int WD_GetIELength(BSSDescriptor* Bss, u8 ID);
int WD_GetIE(BSSDescriptor* Bss, u8 ID, u8* buff, u8 buffsize);
int WD_GetIEIDList(BSSDescriptor* Bss, u8* buff, u8 buffsize);

#endif