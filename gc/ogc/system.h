#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <gctypes.h>
#include <time.h>

#define R_RESET							*(vu32*)0xCC003024

#define SYS_RESTART						R_RESET = 0x00000000;
#define SYS_HOTRESET					R_RESET = 0x00000001; //1?
#define SYS_SHUTDOWN					R_RESET = 0x00000002;

#define SYS_PROTECTCHAN0				0
#define SYS_PROTECTCHAN1				1
#define SYS_PROTECTCHAN2				2
#define SYS_PROTECTCHAN3				3
#define SYS_PROTECTCHANMAX				4

#define SYS_PROTECTNONE					0x00000000
#define SYS_PROTECTREAD					0x00000001		//OK to read
#define SYS_PROTECTWRITE				0x00000002		//OK to write
#define SYS_PROTECTRDWR					(SYS_PROTECTREAD|SYS_PROTECTWRITE)

#define MEM_VIRTUAL_TO_PHYSICAL(x)		(((u32)(x))&~0xC0000000)

#define MEM_PHYSICAL_TO_K0(x)			(void*)((u32)(x)|0x80000000)
#define MEM_PHYSICAL_TO_K1(x)			(void*)((u32)(x)|0xC0000000)

#define MEM_K0_TO_K1(x)					(void*)((u32)(x)|0x40000000)
#define MEM_K1_TO_K0(x)					(void*)((u32)(x)&~0x40000000)


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _syssram {
	u16 checksum;
	u16 checksum_inv;
	u32 ead0;
	u32 ead1;
	u32 counter_bias;
	s8 display_offsetH;
	u8 ntd;
	u8 lang;
	u8 flags;
} syssram;

typedef struct _syssramex {
	u8 flash_id[2][12];
	u32 wirelessKbd_id;
	u16 wirelessPad_id[4];
	u8 dvderr_code;
	u8 __padding0;
	u16 flashID_chksum[2];
	u8 __padding1[4];
} syssramex;

typedef struct _sysalarm sysalarm;

typedef void (*alarmcallback)(sysalarm *alarm);

struct _sysalarm {
	struct _sysalarm *prev;
	struct _sysalarm *next;
	alarmcallback alarmhandler;
	void *handle;
	u32 ticks;
	u32 periodic;
	u32 start_per;
};

typedef void (*resetcallback)(void);

void SYS_Init();
void SYS_SetArenaLo(void *newLo);
void* SYS_GetArenaLo();
void SYS_SetArenaHi(void *newHi);
void* SYS_GetArenaHi();
void SYS_ProtectRange(u32 chan,void *addr,u32 bytes,u32 cntrl);
resetcallback SYS_SetResetCallback(resetcallback cb);
void SYS_StartPMC(u32 mcr0val,u32 mcr1val);
void SYS_DumpPMC();
void SYS_StopPMC();
void SYS_CreateAlarm(sysalarm *alarm);
void SYS_SetAlarm(sysalarm *alarm,const struct timespec *tp,alarmcallback cb);
void SYS_SetPeriodicAlarm(sysalarm *alarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb);
void SYS_RemoveAlarm(sysalarm *alarm);
void SYS_CancelAlarm(sysalarm *alarm);
void SYS_SetWirelessID(u32 chan,u32 id);
u32 SYS_GetWirelessID(u32 chan);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
