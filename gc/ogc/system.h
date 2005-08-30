#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <gctypes.h>
#include <gcutil.h>
#include <time.h>
#include <ogc/lwp_queue.h>
#include "gx_struct.h"

#define R_RESET							*(vu32*)0xCC003024

#define SYS_RESTART						0
#define SYS_HOTRESET					1
#define SYS_SHUTDOWN					2

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
} __attribute__((packed)) syssram;

typedef struct _syssramex {
	u8 flash_id[2][12];
	u32 wirelessKbd_id;
	u16 wirelessPad_id[4];
	u8 dvderr_code;
	u8 __padding0;
	u16 flashID_chksum[2];
	u8 __padding1[4];
} __attribute__((packed)) syssramex;

typedef struct _sysalarm sysalarm;

typedef void (*alarmcallback)(sysalarm *alarm);

struct _sysalarm {
	alarmcallback alarmhandler;
	void* handle;
	u64 ticks;
	u64 periodic;
	u64 start_per;

	struct _sysalarm *prev;
	struct _sysalarm *next;
};

typedef struct _sys_fontheader {
	u16 font_type;
	u16 first_char;
	u16 last_char;
	u16 inval_char;
	u16 asc;
	u16 desc;
	u16 width;
	u16 leading;
    u16 cell_width;
    u16 cell_height;
    u32 sheet_size;
    u16 sheet_format;
    u16 sheet_column;
    u16 sheet_row;
    u16 sheet_width;
    u16 sheet_height;
    u16 width_table;
    u32 sheet_image;
    u32 sheet_fullsize;
    u8  c0;
    u8  c1;
    u8  c2;
    u8  c3;
} __attribute__((packed)) sys_fontheader;

typedef void (*resetcallback)(void);
typedef s32 (*resetfunction)(s32 final);
typedef struct _sys_resetinfo sys_resetinfo;

struct _sys_resetinfo {
	lwp_node node;
	resetfunction func;
	u32 prio;
};

void SYS_Init();
void* SYS_AllocateFramebuffer(GXRModeObj *rmode);
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
u32 SYS_GetFontEncoding();
u32 SYS_InitFont(sys_fontheader *font_header);
void SYS_GetFontTexture(s32 c,void **image,s32 *xpos,s32 *ypos,s32 *width);
void SYS_GetFontTexel(s32 c,void *image,s32 pos,s32 stride,s32 *width);
void SYS_ResetSystem(s32 reset,u32 reset_code,s32 force_menu);
void SYS_RegisterResetFunc(sys_resetinfo *info);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
