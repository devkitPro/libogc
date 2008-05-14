#ifndef __WPAD_H__
#define __WPAD_H__

#include <gctypes.h>

#define WPAD_MAX_IR_DOTS			4

#define WPAD_CHAN_0					0
#define WPAD_CHAN_1					1
#define WPAD_CHAN_2					2
#define WPAD_CHAN_3					3

#define WPAD_BUTTON_2				0x0001
#define WPAD_BUTTON_1               0x0002
#define WPAD_BUTTON_B               0x0004
#define WPAD_BUTTON_A               0x0008
#define WPAD_BUTTON_MINUS           0x0010
#define WPAD_BUTTON_HOME            0x0080
#define WPAD_BUTTON_LEFT			0x0100
#define WPAD_BUTTON_RIGHT           0x0200
#define WPAD_BUTTON_DOWN            0x0400
#define WPAD_BUTTON_UP              0x0800
#define WPAD_BUTTON_PLUS            0x1000
#define WPAD_BUTTON_UNKNOWN         0x8000

#define WPAD_EXP_NONE				0
#define WPAD_EXP_NUNCHAKU			1
#define WPAD_EXP_CLASSIC			2
#define WPAD_EXP_GUITAR_HERO3		3

#define WPAD_FMT_CORE				0
#define WPAD_FMT_CORE_ACC			1
#define WPAD_FMT_CORE_ACC_IR		2

#define WPAD_STATE_DISABLED			0
#define WPAD_STATE_ENABLING			1
#define WPAD_STATE_ENABLED			2

#define WPAD_ERR_NONE				0
#define WPAD_ERR_NO_CONTROLLER		-1
#define WPAD_ERR_NOT_READY			-2
#define WPAD_ERR_TRANSFER			-3

typedef struct _orient_t
{
	f32 roll;
	f32 pitch;
	f32 yaw;
} Orient;

typedef struct _gforce_t
{
	f32 x,y,z;
} GForce;

typedef struct _vec2u8_t
{
	u8 x,y;
} Vec2u8;

typedef struct _vec3u8_t
{
	u8 x,y,z;
} Vec3u8;

typedef struct _joystick_t
{
	Vec2u8 max,min,center;
	f32 ang;
	f32 mag;
} Joystick;

typedef struct _nunchaku_t
{
	u8 btns_d;
	u8 btns_h;
	u8 btns_r;

	Vec3u8 accel;
	Orient orient;
	GForce gforce;

	Joystick js;
} Nunchaku;

typedef struct _expansion_t
{
	s32 type;

	union {
		Nunchaku nunchuk;
	};
} Expansion;

typedef struct _ir_dot_t
{
	s32 x,y;

	u8 order;
	u8 visible;
	u8 size;
} IRDot;

typedef struct _ir_t
{
	IRDot dot[WPAD_MAX_IR_DOTS];

	u8 num_dots;

	s32 x,y;
	f32 z;
	f32 dist;
} IRData;

typedef struct _wpad_data
{
	u16 err;

	u16 btns_d;
	u16 btns_h;
	u16 btns_r;

	IRData ir;
	Vec3u8 accel;
	Orient orient;
	GForce gforce;

	Expansion exp;
} WPADData;

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void WPAD_Init();
void WPAD_Read(WPADData *data);
void WPAD_SetDataFormat(s32 chan,s32 fmt);
void WPAD_SetVRes(s32 chan,u32 xres,u32 yres);
s32 WPAD_GetStatus();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
