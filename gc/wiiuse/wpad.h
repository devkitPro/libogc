#ifndef __WPAD_H__
#define __WPAD_H__

#include <gctypes.h>
#include <wiiuse/wiiuse.h>

#define WPAD_MAX_IR_DOTS						4
											
#define MAX_WIIMOTES							4
											
#define WPAD_CHAN_0								0
#define WPAD_CHAN_1								1
#define WPAD_CHAN_2								2
#define WPAD_CHAN_3								3
											
#define WPAD_BUTTON_2							0x0001
#define WPAD_BUTTON_1							0x0002
#define WPAD_BUTTON_B							0x0004
#define WPAD_BUTTON_A							0x0008
#define WPAD_BUTTON_MINUS						0x0010
#define WPAD_BUTTON_HOME						0x0080
#define WPAD_BUTTON_LEFT						0x0100
#define WPAD_BUTTON_RIGHT						0x0200
#define WPAD_BUTTON_DOWN						0x0400
#define WPAD_BUTTON_UP							0x0800
#define WPAD_BUTTON_PLUS						0x1000
#define WPAD_BUTTON_UNKNOWN						0x8000
											
#define WPAD_NUNCHUK_BUTTON_Z					(0x0001<<16)
#define WPAD_NUNCHUK_BUTTON_C					(0x0002<<16)
											
#define WPAD_CLASSIC_BUTTON_UP					(0x0001<<16)
#define WPAD_CLASSIC_BUTTON_LEFT				(0x0002<<16)
#define WPAD_CLASSIC_BUTTON_ZR					(0x0004<<16)
#define WPAD_CLASSIC_BUTTON_X					(0x0008<<16)
#define WPAD_CLASSIC_BUTTON_A					(0x0010<<16)
#define WPAD_CLASSIC_BUTTON_Y					(0x0020<<16)
#define WPAD_CLASSIC_BUTTON_B					(0x0040<<16)
#define WPAD_CLASSIC_BUTTON_ZL					(0x0080<<16)
#define WPAD_CLASSIC_BUTTON_FULL_R				(0x0200<<16)
#define WPAD_CLASSIC_BUTTON_PLUS				(0x0400<<16)
#define WPAD_CLASSIC_BUTTON_HOME				(0x0800<<16)
#define WPAD_CLASSIC_BUTTON_MINUS				(0x1000<<16)
#define WPAD_CLASSIC_BUTTON_FULL_L				(0x2000<<16)
#define WPAD_CLASSIC_BUTTON_DOWN				(0x4000<<16)
#define WPAD_CLASSIC_BUTTON_RIGHT				(0x8000<<16)

#define WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP		(0x0001<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_YELLOW		(0x0008<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_GREEN			(0x0010<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_BLUE			(0x0020<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_RED			(0x0040<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_ORANGE		(0x0080<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_PLUS			(0x0400<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_MINUS			(0x1000<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN	(0x4000<<16)

#define WPAD_EXP_NONE							0
#define WPAD_EXP_NUNCHAKU						1
#define WPAD_EXP_CLASSIC						2
#define WPAD_EXP_GUITAR_HERO3					3
											
#define WPAD_FMT_CORE							0
#define WPAD_FMT_CORE_ACC						1
#define WPAD_FMT_CORE_ACC_IR					2
				
#define WPAD_DEV_CORE							0
#define WPAD_DEV_NUNCHAKU						1
#define WPAD_DEV_CLASSIC						2							
#define WPAD_DEV_GUITARHERO3					3
#define WPAD_DEV_UNKNOWN						255

#define WPAD_STATE_DISABLED						0
#define WPAD_STATE_ENABLING						1
#define WPAD_STATE_ENABLED						2
											
#define WPAD_ERR_NONE							0
#define WPAD_ERR_NO_CONTROLLER					-1
#define WPAD_ERR_NOT_READY						-2
#define WPAD_ERR_TRANSFER						-3

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _wpad_data
{
	u16 err;
	
	u32 btns_d;
	u32 btns_h;
	u32 btns_r;
	u32 btns_l;

	struct ir_t ir;
	struct vec3b_t accel;
	struct orient_t orient;
	struct gforce_t gforce;
	struct expansion_t exp;
} WPADData;

typedef void (*wpadsamplingcallback)(s32 chan);

s32 WPAD_GetStatus();
void WPAD_Init();
void WPAD_Shutdown();
void WPAD_Disconnect(s32 chan);
void WPAD_SetSleepTime(u32 sleep);
void WPAD_Read(s32 chan,WPADData *data);
void WPAD_SetDataFormat(s32 chan,s32 fmt);
void WPAD_SetVRes(s32 chan,u32 xres,u32 yres);
void WPAD_SetSamplingBufs(s32 chan,void *bufs,u32 cnt);
u32 WPAD_GetLatestBufIndex(s32 chan);
s32 WPAD_Probe(s32 chan,u32 *type);

wpadsamplingcallback WPAD_SetSamplingCallback(s32 chan,wpadsamplingcallback cb);

u32 WPAD_ScanPads();

u32 WPAD_ButtonsUp(int pad);
u32 WPAD_ButtonsDown(int pad);
u32 WPAD_ButtonsHeld(int pad);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
