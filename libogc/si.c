#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "asm.h"
#include "processor.h"
#include "video.h"
#include "si.h"

typedef void (*SICallback)(s32);
typedef struct _sipacket {
	s32 chan;
	void *out;
	u32 out_bytes;
	void *in;
	u32 in_bytes;
	SICallback callback;
	u64 fire;
} sipacket;

static struct _sicntrl {
	s32 chan;
	u32 poll;
	u32 in_bytes;
	void *in;
	SICallback callback;
} sicntrl = {
	-1,
	0,
	0,
	NULL,
	NULL
};

static struct _xy {
	u16 line;
	u8 cnt;
} xy[2][12] = {
	{
		{0x00F6,0x02},{0x000F,0x12},{0x001E,0x09},{0x002C,0x06},
		{0x0034,0x05},{0x0041,0x04},{0x0057,0x03},{0x0057,0x03},
		{0x0057,0x03},{0x0083,0x02},{0x0083,0x02},{0x0083,0x02}
	},

	{
		{0x0128,0x02},{0x000F,0x15},{0x001D,0x0B},{0x002D,0x07},
		{0x0034,0x06},{0x003F,0x05},{0x004E,0x04},{0x0068,0x03},
		{0x0068,0x03},{0x0068,0x03},{0x0068,0x03},{0x009C,0x02}
	}
};

static u32 sampling_rate = 0;

static vu32* const _siReg = (u32*)0xCC006400;
static vu16* const _viReg = (u16*)0xCC002000;

static __inline__ struct _xy* __si_getxy()
{
	switch(VIDEO_GetCurrentTvMode()) {
		case VI_NTSC:
		case VI_MPAL:
		case VI_EURGB60:
			return xy[0];
			break;
		case VI_PAL:
			return xy[1];
			break;
	}
	return NULL;
}

void SISetXY(u16 line,u8 cnt)
{
	u32 level;

	_CPU_ISR_Disable(level);
	sicntrl.poll = (sicntrl.poll&~0x3ffff00)|(line<<16)|(cnt<<8);
	_siReg[12] = sicntrl.poll;
	_CPU_ISR_Restore(level);
}

void SISetSamplingRate(u32 samping_rate)
{
	u32 div,level;
	struct _xy *xy = __si_getxy();
	
	if(samping_rate>11) samping_rate = 11;

	_CPU_ISR_Disable(level);
	if(_viReg[54]&0x0001) div = 2;
	else div = 1;

	SISetXY(div*xy[samping_rate].line,xy[samping_rate].cnt);
	_CPU_ISR_Restore(level);
}

void SIRefreshSamplingRate()
{
	SISetSamplingRate(sampling_rate);
}


