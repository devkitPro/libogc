#include <stdlib.h>
#include <stdio.h>

#include "exi.h"
#include "sdcard.h"

static __inline__ u32 __make_crc7(void *buffer)
{
	return 0;
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
