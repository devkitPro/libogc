#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "asm.h"
#include "processor.h"
#include "ogcsys.h"
#include "irq.h"
#include "exi.h"
#include "gx.h"
#include "lwp.h"
#include "system.h"
#include "video.h"
#include "video_types.h"

//#define _VIDEO_DEBUG

#define VIDEO_MQ					1

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static const u8 video_timing[6][38] = {
	{
		0x06,0x00,0x00,0xF0,0x00,0x18,0x00,0x19,
		0x00,0x03,0x00,0x02,0x0C,0x0D,0x0C,0x0D,
		0x02,0x08,0x02,0x07,0x02,0x08,0x02,0x07,
		0x02,0x0D,0x01,0xAD,0x40,0x47,0x69,0xA2,
		0x01,0x75,0x7A,0x00,0x01,0x9C
	},
	{
		0x06,0x00,0x00,0xF0,0x00,0x18,0x00,0x18,
		0x00,0x04,0x00,0x04,0x0C,0x0C,0x0C,0x0C,
		0x02,0x08,0x02,0x08,0x02,0x08,0x02,0x08,
		0x02,0x0E,0x01,0xAD,0x40,0x47,0x69,0xA2,
		0x01,0x75,0x7A,0x00,0x01,0x9C
	},
	{
		0x05,0x00,0x01,0x1F,0x00,0x23,0x00,0x24,
		0x00,0x01,0x00,0x00,0x0D,0x0C,0x0B,0x0A,
		0x02,0x6B,0x02,0x6A,0x02,0x69,0x02,0x6C,
		0x02,0x71,0x01,0xB0,0x40,0x4B,0x6A,0xAC,
		0x01,0x7C,0x85,0x00,0x01,0xA4	
	},
	{
		0x05,0x00,0x01,0x1F,0x00,0x21,0x00,0x21,
		0x00,0x02,0x00,0x02,0x0D,0x0B,0x0D,0x0B,
		0x02,0x6B,0x02,0x6D,0x02,0x6B,0x02,0x6D,
		0x02,0x70,0x01,0xB0,0x40,0x4B,0x6A,0xAC,
		0x01,0x7C,0x85,0x00,0x01,0xA4
	},
	{
		0x06,0x00,0x00,0xF0,0x00,0x18,0x00,0x19,
		0x00,0x03,0x00,0x02,0x10,0x0F,0x0E,0x0D,
		0x02,0x06,0x02,0x05,0x02,0x04,0x02,0x07,
		0x02,0x0D,0x01,0xAD,0x40,0x4E,0x70,0xA2,
		0x01,0x75,0x7A,0x00,0x01,0x9C
	},
	{
		0x06,0x00,0x00,0xF0,0x00,0x18,0x00,0x18,
		0x00,0x04,0x00,0x04,0x10,0x0E,0x10,0x0E,
		0x02,0x06,0x02,0x08,0x02,0x06,0x02,0x08,
		0x02,0x0E,0x01,0xAD,0x40,0x4E,0x70,0xA2,
		0x01,0x75,0x7A,0x00,0x01,0x9C
	}
};

static const u16 taps[26] = {
	0x01F0,0x01DC,0x01AE,0x0174,0x0129,0x00DB,
	0x008E,0x0046,0x000C,0x00E2,0x00CB,0x00C0,
	0x00C4,0x00CF,0x00DE,0x00EC,0x00FC,0x0008,
	0x000F,0x0013,0x0013,0x000F,0x000C,0x0008,
	0x0001,0x0000                              
};

struct _horVer {
	u16 dispPosX;
	u16 dispPosY;
	u16 dispSizeX;
	u16 dispSizeY;
	u16 adjustedDispPosX;
	u16 adjustedDispPosY;
	u16 adjustedDispSizeY;
	u16 adjustedPanPosY;
	u16 adjustedPanSizeY;
	u16 fbSizeX;
	u16 fbSizeY;
	u16 panPosX;
	u16 panPosY;
	u16 panSizeX;
	u16 panSizeY;
	u32 fbMode;	
	u32 nonInter;	
	u32 tv;
	u8 wordPerLine;
	u8 std;
	u8 wpl;
	void *bufAddr;
	u32 tfbb;
	u32 bfbb;
	u8 xof;
	s32 black;
	s32 threeD;
	u32 rbufAddr;
	u32 rtfbb;
	u32 rbfbb;
	const u8 *timing;
} HorVer;

GXRModeObj TVNtsc480IntDf = 
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVNtsc480IntAa = 
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 4,         // line n-1
		 8,         // line n-1
		12,         // line n
		16,         // line n
		12,         // line n
		 8,         // line n+1
		 4          // line n+1
	}
};

GXRModeObj TVMpal480IntDf = 
{
    VI_TVMODE_MPAL_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVPal264Ds = 
{
    VI_TVMODE_PAL_DS,       // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL/2 - 528/2)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal528Int = 
{
    VI_TVMODE_PAL_INT,       // viDisplayMode
    640,             // fbWidth
    528,             // efbHeight
    528,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal524IntAa = 
{
	VI_TVMODE_PAL_INT,
	640,
	264,
	524,
	(VI_MAX_WIDTH_PAL-640)/2,
	(VI_MAX_HEIGHT_PAL-528)/2,
	640,
	524,
	VI_XFBMODE_DF,
	GX_FALSE,
	GX_TRUE,
	
    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},
 
	 // vertical filter[7], 1/64 units, 6 bits each
	{
		4,         // line n-1
		8,         // line n-1
		12,        // line n
		16,        // line n
		12,        // line n
		8,         // line n+1
		4          // line n+1
	}
};

GXRModeObj TVPal528IntDf = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    528,             // efbHeight
    528,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVPal574IntDfScale = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    574,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 574)/2,        // viYOrigin
    640,             // viWidth
    574,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

static u16 regs[60];
static u16 shdwRegs[60];
static u32 retraceCount,fbSet = 0;
static u32 encoderType;
static s16 displayOffsetH;
static s16 displayOffsetV;
static u32 currTvMode,changeMode;
static u32 shdwChangeMode,flushFlag;
static u64 changed,shdwChanged;
static const u8 *currTiming;
static lwpq_t video_queue;
static VIRetraceCallback preRetraceCB = NULL;
static VIRetraceCallback postRetraceCB = NULL;

static vu16* const _viReg = (u16*)0xCC002000;

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern u32 __SYS_LockSram();
extern u32 __SYS_UnlockSram(u32 write);

static __inline__ u32 cntlzd(u64 bit)
{
	u32 hi,lo,value = 0;

	hi = (u32)(bit>>32);
	lo = (u32)(bit&-1);

	value = cntlzw(hi);
	if(value>=32) {
		value = cntlzw(lo);
		value += 32;
	}
	return value;
}

static __inline__ void __clear_interrupt()
{
	_viReg[24] &= ~0x8000;
	_viReg[26] &= ~0x8000;
	_viReg[28] &= ~0x8000;
	_viReg[30] &= ~0x8000;
}

static __inline__ u32 __checkclear_interrupt()
{
	u32 ret = 0;

	if(_viReg[24]&0x8000) {
		_viReg[24] &= ~0x8000;
		ret |= 0x01;
	}
	if(_viReg[26]&0x8000) {
		_viReg[26] &= ~0x8000;
		ret |= 0x02;
	}
	if(_viReg[28]&0x8000) {
		_viReg[28] &= ~0x8000;
		ret |= 0x04;
	}
	if(_viReg[30]&0x8000) {
		_viReg[30] &= ~0x8000;
		ret |= 0x08;
	}
	return ret;
}

static __inline__ void __calcFbbs(u32 bufAddr,u16 panPosX,u16 panPosY,u8 wordperline,u32 xfbMode,u16 dispPosY,u32 *tfbb,u32 *bfbb)
{
	u32 bytesPerLine,tmp;

	bytesPerLine = (wordperline<<5)&~0x1f;
	*tfbb = bufAddr+(((panPosX<<1)&~0x1f)+(panPosY*bytesPerLine));
	*bfbb = *tfbb;
	if(xfbMode!=VI_XFBMODE_SF) *bfbb = *tfbb+bytesPerLine;

	if((dispPosY-((dispPosY/2)<<1))==1) {
		tmp = *tfbb;
		*tfbb = *bfbb;
		*bfbb = tmp;
	}
	*tfbb = MEM_VIRTUAL_TO_PHYSICAL(*tfbb);
	*bfbb = MEM_VIRTUAL_TO_PHYSICAL(*bfbb);
}

static const u8* __gettiming(u32 vimode)
{
	if(vimode>0x0015) return NULL;

	switch(vimode) {
		case VI_TVMODE_NTSC_INT:
			return video_timing[0];
			break;
		case VI_TVMODE_NTSC_DS:
			return video_timing[1];
			break;
		case VI_TVMODE_PAL_INT:
			return video_timing[2];
			break;
		case VI_TVMODE_PAL_DS:
			return video_timing[3];
			break;
	}
	return NULL;
}

static void __setFbbRegs(struct _horVer *horVer,u32 *tfbb,u32 *bfbb,u32 *rtfbb,u32 *rbfbb)
{
	u32 flag;
	__calcFbbs((u32)horVer->bufAddr,horVer->panPosX,horVer->adjustedPanPosY,horVer->wordPerLine,horVer->fbMode,horVer->adjustedDispPosY,tfbb,bfbb);
	if(horVer->threeD) __calcFbbs(horVer->rbufAddr,horVer->panPosX,horVer->adjustedPanPosY,horVer->wordPerLine,horVer->fbMode,horVer->adjustedDispPosY,rtfbb,rbfbb);

	flag = 1;
	if((*tfbb)<0x01000000 && (*bfbb)<0x01000000
		&& (*rtfbb)<0x01000000 && (*rbfbb)<0x01000000) flag = 0;

	if(flag) {
		*tfbb >>= 5;
		*bfbb >>= 5;
		*rtfbb >>= 5;
		*rbfbb >>= 5;
	}
	regs[15] = *tfbb&0xffff;
	changed |= (u64)0x00010000<<32;

	regs[14] = (flag<<12)|(horVer->xof<<8)|(*tfbb>>16);
	changed |= (u64)0x00020000<<32;

	regs[19] = *bfbb&0xffff;
	changed |= (u64)0x1000<<32;
	
	regs[18] = (*bfbb>>16);
	changed |= (u64)0x2000<<32;
	
	if(horVer->threeD) {
		regs[17] = *rtfbb&0xffff;
		changed |= (u64)0x4000<<32; 

		regs[16] = *rtfbb>>16;
		changed |= (u64)0x8000<<32; 
	
		regs[21] = *rbfbb&0xffff;
		changed |= (u64)0x0400<<32;
	
		regs[20] = *rbfbb>>16;
		changed |= (u64)0x0800<<32;
	}
}

static void __setVerticalRegs(u16 dispPosY,u16 dispSizeY,u8 equ,u16 acv,u16 prbOdd,u16 prbEven,u16 psbOdd,u16 psbEven,s32 black)
{
	u32 tmp;
	u32 div1,div2;
	u32 psb,prb;
	u32 psbodd,prbodd;
	u32 psbeven,prbeven;

	div1 = 2;
	div2 = 1;
	if(equ>=10) {
		div1 = 1;
		div2 = 2;
	}
	
	tmp = (dispPosY/2)<<1;
	prb = div2*dispPosY;
	psb = div2*(((acv*div1)-dispSizeY)-dispPosY);
	if(!(dispPosY-tmp)) {
		prbodd = prbOdd+prb;
		psbodd = psbOdd+psb;
		prbeven = prbEven+prb;
		psbeven = psbEven+psb;
	} else {
		prbodd = prbEven+prb;
		psbodd = psbEven+psb;
		prbeven = prbOdd+prb;
		psbeven = psbOdd+psb;
	}

	tmp = dispSizeY/div1;
	if(black) {
		prbodd += ((tmp<<1)-2);
		prbeven += ((tmp<<1)-2);
		psbodd += 2;
		psbeven += 2;
		tmp = 0;
	}
	
	regs[0] = ((tmp<<4)&~0x0f)|equ;
	changed |= (u64)0x80000000<<32;

	regs[7] = prbodd;
	changed |= (u64)0x01000000<<32;	
	regs[6] = psbodd;
	changed |= (u64)0x02000000<<32;	
	regs[9] = prbeven;
	changed |= (u64)0x00400000<<32;	
	regs[8] = psbeven;
	changed |= (u64)0x00800000<<32;	
}

static void __VIInit(u32 vimode)
{
	u32 cnt;
	u32 vi_mode,interlace,prog;
	const u8 *cur_timing = NULL;

	vi_mode = vimode>>2;
	interlace = vimode&0x01;
	prog = vimode&0x02;
	
	cur_timing = __gettiming(vimode);

	//reset the interface
	cnt = 0;
	_viReg[1] = 0x02;
	while(cnt<1000) cnt++;
	_viReg[1] = 0x00;

	// now begin to setup the interface
	_viReg[2] = ((cur_timing[29]<<8)|cur_timing[30]);		//set HCS & HCE
	_viReg[3] = ((u16*)cur_timing)[13];						//set Half Line Width

	_viReg[4] = ((((u16*)cur_timing)[16]<<1)&0xFFFE);			//set HBS
	_viReg[5] = ((cur_timing[31]<<7)|cur_timing[28]);		//set HBE & HSY
	
	_viReg[0] = cur_timing[0];

	_viReg[6] = (((u16*)cur_timing)[4]+2);					//set PSB odd field
	_viReg[7] = (((u16*)cur_timing)[2]+((((u16*)cur_timing)[1]<<1)-2));		//set PRB odd field

	_viReg[8] = (((u16*)cur_timing)[5]+2);					//set PSB even field
	_viReg[9] = (((u16*)cur_timing)[3]+((((u16*)cur_timing)[1]<<1)-2));		//set PRB even field

	_viReg[10] = ((((u16*)cur_timing)[10]<<5)|cur_timing[14]);	//set BE3 & BS3
	_viReg[11] = ((((u16*)cur_timing)[8]<<5)|cur_timing[12]);	//set BE1 & BS1
	
	_viReg[12] = ((((u16*)cur_timing)[11]<<5)|cur_timing[15]);	//set BE4 & BS4
	_viReg[13] = ((((u16*)cur_timing)[9]<<5)|cur_timing[13]);	//set BE2 & BS2

	_viReg[24] = (((((u16*)cur_timing)[12]/2)+1)|0x1000);
	_viReg[25] = (((u16*)cur_timing)[13]+1);
	
	_viReg[26] = 0x1001;		//set DI1
	_viReg[27] = 0x0001;		//set DI1
	_viReg[36] = 0x2828;		//set HSR
	
	if(vimode==0x0002 || vimode==0x0003) {
		_viReg[1] = ((vi_mode<<8)|0x0005);		//set MODE & INT & enable
		_viReg[54] = 0x0001;
	} else {
		_viReg[1] = ((vi_mode<<8)|(prog<<2)|0x0001);
		_viReg[54] = 0x0000;
	}
}

static u32 __getCurrentHalfLine()
{
	u32 tmp;
	u32 vpos = 0;
	u32 hpos = 0;

	vpos = _viReg[22]&0x07FF;
	do {
		tmp = vpos;
		hpos = _viReg[23]&0x07FF;
		vpos = _viReg[22]&0x07FF;
	} while(tmp!=vpos);
	
	hpos--;
	vpos--;
	vpos <<= 1;
	tmp = ((u16*)currTiming)[13];

	return vpos+(hpos/tmp);	
}

static u32 __getCurrFieldEvenOdd()
{
	u32 hline;
	u32 nhline;

	hline = __getCurrentHalfLine();
	nhline = ((u16*)currTiming)[12];
	if(hline<nhline) return 1;

	return 0;
}

static u32 __VISetRegs()
{
	u32 val;
	u64 mask;

	if(shdwChangeMode==1){
		if(!__getCurrFieldEvenOdd()) return 0;
	}

	while(shdwChanged) {
		val = cntlzd(shdwChanged);
		_viReg[val] = shdwRegs[val];
		mask = (u64)1<<(63-val);
		shdwChanged &= ~mask;
	}
	shdwChangeMode = 0;
	currTiming = HorVer.timing;
	currTvMode = HorVer.tv;
	
	return 1;
}

static void __VIRetraceHandler(u32 nIrq,void *pCtx)
{
	u32 ret;

	if(!(ret=__checkclear_interrupt()) || ret&0xc) return;

	retraceCount++;
	if(preRetraceCB)
		preRetraceCB(retraceCount);

	if(flushFlag) {
		if(__VISetRegs()) {
			flushFlag = 0;
		}
	}

	if(postRetraceCB)
		postRetraceCB(retraceCount);

	LWP_WakeThread(video_queue);
}

void VIDEO_Init()
{
	u32 div,vimode = 0;
	s16 tmp1,tmp2,tmp3;
	syssram *sram;

	if(!(_viReg[1]&0x0001))
		__VIInit(VI_TVMODE_NTSC_INT);

	retraceCount = 0;
	changed = 0;
	shdwChanged = 0;
	shdwChangeMode = 0;
	flushFlag = 0;
	encoderType = 1;
	
	_viReg[38] = ((taps[1]>>6)|(taps[2]<<4));
	_viReg[39] = (taps[0]|_SHIFTL(taps[1],10,6));
	_viReg[40] = ((taps[4]>>6)|(taps[5]<<4));
	_viReg[41] = (taps[3]|_SHIFTL(taps[4],10,6));
	_viReg[42] = ((taps[7]>>6)|(taps[8]<<4));
	_viReg[43] = (taps[6]|_SHIFTL(taps[7],10,6));
	_viReg[44] = (taps[11]|(taps[12]<<8));
	_viReg[45] = (taps[9]|(taps[10]<<8));
	_viReg[46] = (taps[15]|(taps[16]<<8));
	_viReg[47] = (taps[13]|(taps[14]<<8));
	_viReg[48] = (taps[19]|(taps[20]<<8));
	_viReg[49] = (taps[17]|(taps[18]<<8));
	_viReg[50] = (taps[23]|(taps[24]<<8));
	_viReg[51] = (taps[21]|(taps[22]<<8));
	_viReg[56] = 640;

	sram = (syssram*)__SYS_LockSram();
	displayOffsetV = 0;
	displayOffsetH = (s16)sram->display_offsetH;
	__SYS_UnlockSram(0)	;

	HorVer.nonInter = _SHIFTR(_viReg[1],2,1);
	HorVer.tv = _SHIFTR(_viReg[1],8,2);

	vimode = HorVer.nonInter;
	if(HorVer.tv!=VI_DEBUG) vimode += (HorVer.tv<<2);
	currTiming = (u8*)__gettiming(vimode);
	currTvMode = HorVer.tv;

	regs[1] = _viReg[1];
	HorVer.timing = currTiming;
	HorVer.dispSizeX = 640;
	HorVer.dispSizeY = ((((u16*)currTiming)[1]<<1)&0xfffe);
	HorVer.dispPosX = (VI_MAX_WIDTH_NTSC-HorVer.dispSizeX)/2;
	HorVer.dispPosY = 0;
	
	if((HorVer.dispPosX+displayOffsetH)<=(VI_MAX_WIDTH_NTSC-HorVer.dispSizeX)) {
		if(displayOffsetH>=0) HorVer.adjustedDispPosX = (HorVer.dispPosX+displayOffsetH);
		else HorVer.adjustedDispPosX = 0;
	} else
		HorVer.adjustedDispPosX = (VI_MAX_WIDTH_NTSC-HorVer.dispSizeX);

	tmp3 = (HorVer.dispPosY&0x0001);
	if((HorVer.dispPosY+displayOffsetV)<=tmp3) HorVer.adjustedDispPosY = tmp3;
	else HorVer.adjustedDispPosY = (HorVer.dispPosY+displayOffsetV);

	
	tmp1 = (HorVer.dispPosY + HorVer.dispSizeY + displayOffsetV);
	tmp2 = ((((u16*)currTiming)[1]<<1) - tmp3);
	if((tmp1-tmp2)>0) tmp1 -= tmp2;
	else tmp1 = 0;
	
	tmp2 = (HorVer.dispPosY+displayOffsetV);
	if((tmp2-tmp3)<0)  tmp2 -= tmp3;
	else tmp2 = 0;
	HorVer.adjustedDispSizeY = tmp1-(HorVer.dispSizeY+tmp2);
	
	tmp1 = (HorVer.dispPosY+displayOffsetV);
	if((tmp1-tmp3)<0) tmp1 -= tmp3;
	else tmp1 = 0;

	if(HorVer.fbMode==VI_XFBMODE_SF) div = 2;
	else div = 1;
	HorVer.adjustedPanPosY = HorVer.panPosY-(tmp1/div);
	
	tmp1 = (HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV);
	tmp2 = ((((u16*)currTiming)[1]<<1)-tmp3);
	if((tmp1-tmp2)>0) tmp1 -= tmp2;
	else tmp1 = 0;

	tmp2 = (HorVer.dispPosY+displayOffsetV);
	if((tmp2-tmp3)<0) tmp2 -= tmp3;
	else tmp2 = 0;
	
	tmp2 = (HorVer.panSizeY+(tmp2/div));
	HorVer.adjustedPanSizeY = tmp2-(tmp1/div);
	HorVer.fbSizeX = 640;
	HorVer.fbSizeY = ((((u16*)currTiming)[1]<<1)&0xfffe);
	HorVer.panPosX = 0;
	HorVer.panPosY = 0;
	HorVer.panSizeX = 640;
	HorVer.panSizeY = ((((u16*)currTiming)[1]<<1)&0xfffe);
	HorVer.fbMode = VI_XFBMODE_SF;
	HorVer.wordPerLine = 40;
	HorVer.std = 40;
	HorVer.wpl = 40;
	HorVer.xof = 0;
	HorVer.black = 1;
	HorVer.threeD = 0;
	
	_viReg[24] &= ~0x8000;
	_viReg[26] &= ~0x8000;

	preRetraceCB = NULL;
	postRetraceCB = NULL;
	
	LWP_InitQueue(&video_queue);

	IRQ_Request(IRQ_PI_VI,__VIRetraceHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_VI));
}

void VIDEO_Configure(GXRModeObj *rmode)
{
	u16 dcr;
	s32 tmp,tmp1,tmp2;
	u32 nonint,vimode,div,tmp4,tmp5,value,level;
	const u8 *curtiming;

	_CPU_ISR_Disable(level);
	HorVer.tv = _SHIFTR(rmode->viTVMode,2,3);
	HorVer.dispPosX = rmode->viXOrigin;
	HorVer.dispPosY = rmode->viYOrigin;
	HorVer.dispSizeX = rmode->viWidth;
	HorVer.fbSizeX = rmode->fbWidth;
	HorVer.fbSizeY = rmode->xfbHeight;
	HorVer.fbMode = rmode->xfbMode;
	HorVer.panSizeX = HorVer.fbSizeX;
	HorVer.panSizeY = HorVer.fbSizeY;
	HorVer.panPosX = 0;
	HorVer.panPosY = 0;

	nonint = (rmode->viTVMode&0x0003);
	if(nonint!=HorVer.nonInter) {
		changeMode = 1;
		HorVer.nonInter = nonint;
	}
	if(HorVer.nonInter==VI_NON_INTERLACE) HorVer.dispPosY = (HorVer.dispPosY<<1)&0xfffe;
	
	if(nonint==VI_PROGRESSIVE || nonint==(VI_NON_INTERLACE|VI_PROGRESSIVE)) tmp = HorVer.panSizeY;
	else if(HorVer.fbMode==VI_XFBMODE_SF) tmp = (HorVer.panSizeY<<1)&0xfffe;
	else tmp = HorVer.panSizeY;
	HorVer.dispSizeY = tmp;

	tmp = 0;
	if(nonint==(VI_NON_INTERLACE|VI_PROGRESSIVE)) tmp = 1;
	HorVer.threeD = tmp;

	vimode = VI_TVMODE(HorVer.tv,HorVer.nonInter);
	curtiming = __gettiming(vimode);
	HorVer.timing = curtiming;

	tmp = (HorVer.dispPosX+displayOffsetH);
	tmp1 = (720 - HorVer.dispSizeX);
	if(tmp>=0 && tmp<=tmp1) HorVer.adjustedDispPosX = tmp;
	else HorVer.adjustedDispPosX = 0;
	
	if(HorVer.fbMode==VI_XFBMODE_SF) div = 2;
	else div = 1;
	
	tmp = HorVer.dispPosY + displayOffsetV;
	tmp1 = (HorVer.dispPosY&0x0001);
	if(tmp<=tmp1) HorVer.adjustedDispPosY = tmp1;
	else HorVer.adjustedDispPosY = tmp;

	tmp = HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV;
	tmp2 = (((u16*)curtiming)[1]<<1) - tmp1;
	if((tmp-tmp2)>0) tmp -= tmp2;
	else tmp = 0;
	
	tmp2 = HorVer.dispPosY+displayOffsetV;
	if((tmp2-tmp1)<0) tmp2 -= tmp1;
	else tmp2 = 0;
	HorVer.adjustedDispSizeY = HorVer.dispSizeY+tmp+tmp2;
	
	tmp = HorVer.dispPosY + displayOffsetV;
	if((tmp-tmp1)<0) tmp -= tmp1;
	else tmp = 0;
	HorVer.adjustedPanPosY = HorVer.panPosY-(tmp/div);

	tmp = HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV;
	tmp2 = (((u16*)curtiming)[1]<<1) - tmp1;
	if((tmp-tmp2)>0) tmp -= tmp2;
	else tmp = 0;

	tmp2 = HorVer.dispPosY+displayOffsetV;
	if((tmp2-tmp1)<0) tmp2 -= tmp1;
	else tmp = 0;
	HorVer.adjustedPanSizeY = (HorVer.panSizeY+(tmp2/div))-(tmp/div);

	if(!encoderType) HorVer.tv = VI_DEBUG;
	tmp4 = ((u16*)curtiming)[12]-((((u16*)curtiming)[12]/2)<<1);
	tmp5 = (((u16*)curtiming)[12]/2)&0xffff;
	if(!(tmp4&0xffff)) tmp4 = 0;
	else tmp4 = ((u16*)curtiming)[13];

	regs[24] = 0x1000|(tmp5+1);
	regs[25] = tmp4+1;
	changed |= (u64)0x0080<<32;
	changed |= (u64)0x0040<<32;

	dcr = regs[1];
	if(HorVer.nonInter==VI_PROGRESSIVE || HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) dcr = (dcr&~0x0004)|0x0004;
	else dcr = (dcr&~0x0004)|_SHIFTL(HorVer.nonInter,2,1);

	dcr = (dcr&~0x0008)|_SHIFTL(HorVer.threeD,3,1);
	dcr &= ~0x0300;
	if(!(HorVer.tv==VI_DEBUG_PAL || HorVer.tv==VI_EURGB60)) dcr |= _SHIFTL(HorVer.tv,8,2);
	regs[1] = dcr;
	changed |= (u64)0x40000000<<32;

	regs[54] &= ~0x0001;
	if(rmode->viTVMode==VI_TVMODE_NTSC_PROG || rmode->viTVMode==VI_TVMODE_NTSC_PROG_DS) regs[54] |= 0x0001;
	changed |= (u64)0x0200;

	tmp = HorVer.panSizeX;
	if(HorVer.threeD) tmp <<= 1;
	if(HorVer.dispSizeX<tmp) {
		regs[37] = 0x1000|((HorVer.dispSizeX+((tmp<<8)-1))/HorVer.dispSizeX);
		changed |= (u64)0x04000000;
		regs[56] = tmp;
		changed |= (u64)0x0080;
	} else {
		regs[37] = 0x0100;
		changed |= (u64)0x04000000;
	}

	regs[3] = ((u16*)curtiming)[13];
	changed |= (u64)0x10000000<<32;
	
	regs[2] = (curtiming[29]<<8)|curtiming[30];
	changed |= (u64)0x20000000<<32;

	value = (curtiming[31]+HorVer.adjustedDispPosX-40);
	regs[5] = ((value<<7)&0xff80)|curtiming[28];
	changed |= (u64)0x04000000<<32;
	
	regs[4] = (value>>9)|(((((u16*)curtiming)[16]+HorVer.adjustedDispPosX+40)-(720-HorVer.dispSizeX))<<1);
	changed |= (u64)0x08000000<<32;

	regs[11] = curtiming[12]|(((u16*)curtiming)[8]<<5);
	changed |= (u64)0x00100000<<32;
	
	regs[10] = curtiming[14]|(((u16*)curtiming)[10]<<5);
	changed |= (u64)0x00200000<<32;

	regs[13] = curtiming[13]|(((u16*)curtiming)[9]<<5);
	changed |= (u64)0x00040000<<32;

	regs[12] = curtiming[15]|(((u16*)curtiming)[11]<<5);
	changed |= (u64)0x00080000<<32;

	HorVer.wordPerLine = tmp = (HorVer.fbSizeX+15)/16;
	
	if(HorVer.fbMode==VI_XFBMODE_DF) tmp = HorVer.wordPerLine;
	HorVer.std = (tmp<<1)&0xfe;
	
	HorVer.xof = HorVer.panPosX-((HorVer.panPosX/16)<<4);
	HorVer.wpl = (HorVer.panSizeX+15)/16;
	regs[36] = (HorVer.wpl<<8)|HorVer.std;
	changed |= (u64)0x08000000;
	
	if(fbSet) __setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);

	__setVerticalRegs(HorVer.adjustedDispPosY,HorVer.adjustedDispSizeY,curtiming[0],((u16*)curtiming)[1],((u16*)curtiming)[2],((u16*)curtiming)[3],((u16*)curtiming)[4],((u16*)curtiming)[5],HorVer.black);
	_CPU_ISR_Restore(level);
}

void VIDEO_WaitVSync(void)
{
	u32 level;
	u32 retcnt;
	
	_CPU_ISR_Disable(level);
	retcnt = retraceCount;
	while(retraceCount==retcnt) {
		LWP_SleepThread(video_queue);
	}
	_CPU_ISR_Restore(level);
}

void VIDEO_SetFramebuffer(void *fb)
{
	u32 level,flag;
	u32 tfbb,bfbb;

	_CPU_ISR_Disable(level);
	tfbb = MEM_VIRTUAL_TO_PHYSICAL(fb);
	bfbb = tfbb+1280;

	flag = 1;
	if(tfbb<0x01000000 && bfbb<0x01000000) flag = 0;
	if(flag) {
		tfbb >>= 5;
		bfbb >>= 5;
	}

	_viReg[14] = (flag<<12)|(0<<8)|(tfbb>>16);
	_viReg[15] = tfbb;

	_viReg[18] = bfbb>>16;
	_viReg[19] = bfbb&0xffff;
	
	_CPU_ISR_Restore(level);
}

void VIDEO_SetNextFramebuffer(void *fb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	fbSet = 1;
	HorVer.bufAddr = fb;
	__setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);
	_CPU_ISR_Restore(level);
}

void VIDEO_Flush()
{
	u32 level;
	u32 val;
	u64 mask;

	_CPU_ISR_Disable(level);
	shdwChangeMode |= changeMode;
	changeMode = 0;

	shdwChanged |= changed;
	while(changed) {
		val = cntlzd(changed);
		shdwRegs[val] = regs[val];
		mask = (u64)1<<(63-val);
		changed &= ~mask;
	}
	flushFlag = 1;
	_CPU_ISR_Restore(level);
}

void VIDEO_SetBlack(boolean black)
{
	u32 level;
	const u8 *timing;
	
	_CPU_ISR_Disable(level);
	HorVer.black = black;
	timing = HorVer.timing;
	__setVerticalRegs(HorVer.adjustedDispPosY,HorVer.dispSizeY,timing[0],((u16*)timing)[1],((u16*)timing)[2],((u16*)timing)[3],((u16*)timing)[4],((u16*)timing)[5],HorVer.black);
	_CPU_ISR_Restore(level);
}

u32 VIDEO_GetNextField()
{
	u32 level,val;
	u16 tdpv,tdph;
	u16 tmp,dpv,dph;

	_CPU_ISR_Disable(level);
	tmp = (_viReg[22]&0x7ff);
	dph = (_viReg[23]&0x7ff);
	while((dpv=(_viReg[22]&0x7ff))!=tmp);
	
	dpv--;	dph--;
	tdph = ((u16*)currTiming)[12];
	tdpv = (dpv<<1)+(dph/((u16*)currTiming)[13]);

	if(tdpv>=tdph) val = 0;
	else val = 1;
	_CPU_ISR_Restore(level);
	
	return ((val^1)^(HorVer.adjustedDispPosY&0x0001));
}

u32 VIDEO_GetCurrentTvMode()
{
	u32 mode;
	u32 level;
	u32 tv;

	_CPU_ISR_Disable(level);
	mode = currTvMode;
	if(mode==VI_DEBUG) tv = VI_NTSC;
	else if(mode==VI_EURGB60) tv = mode;
	else if(mode==VI_PAL) tv = VI_PAL;
	else if(mode==VI_NTSC) tv = VI_NTSC;
	else tv = VI_PAL;
	_CPU_ISR_Restore(level);

	return tv;
}

VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback)
{
	u32 level = 0;
	VIRetraceCallback ret = preRetraceCB;
	_CPU_ISR_Disable(level);
	preRetraceCB = callback;
	_CPU_ISR_Restore(level);
	return ret;
}

VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback)
{
	u32 level = 0;
	VIRetraceCallback ret = postRetraceCB;
	_CPU_ISR_Disable(level);
	postRetraceCB = callback;
	_CPU_ISR_Restore(level);
	return ret;
}
