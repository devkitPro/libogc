#include <stdlib.h>
#include <stdio.h>

#include "exi.h"
#include "sdcard.h"

typedef struct _sdcard_cntrl {
	u8 cmd[9];
	s16 err_status;
	u32 sd_arg;
} sdcard_block;

static sdcard_block sdcard_map[EXI_CHANNEL_MAX];

static __inline__ u32 __make_crc7(void *buffer,u32 len)
{
	u32 mask,cnt,bcnt;
	u16 res,tmp;
	u8 val,*ptr = (u8*)buffer;

	cnt = 0;
	res = 0;
	while(cnt<len) {
		bcnt = 0;
		mask = 0x80;
		while(bcnt<8) {
			res <<= 1;
			val = *ptr^((res&0xff)>>bcnt);
			if(val&mask) {
				res |= 0x01;
				if((res^0x0008)&0x0008) res |= 0x0008;
				else res &= ~0x0008;
				
			} else if(res&0x0008) res |= 0x0008;
			else res &= ~0x0008;
			
			mask >>= 1;
			bcnt++;
		}
		ptr++;
		cnt++;
	}
	return (res<<1)&0xff;
}

s32 SDCARD_SetModeSPI(s32 chn)
{
	s32 ret;
	u32 err;
	u8 cmd[6] = {0x40,0x00,0x00,0x00,0x00,0x95};

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;

	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) return SDCARD_ERROR_NOCARD;
	
	err = 0;
	if(EXI_ImmEx(chn,cmd,6,EXI_READWRITE)==0) err |= 0x01;
	if(EXI_Deselect(chn)==0) err |= 0x02;

	if(err) ret = SDCARD_ERROR_NOCARD;
	else ret = SDCARD_ERROR_READY;

	return ret;
}

s32 SDCARD_SendCID(u32 chn,u8 *cid)
{
	return -1;
}
