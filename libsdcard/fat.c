#include <stdlib.h>
#include <stdio.h>

#include "fat.h"

typedef struct _sdcard_cntrl {
	fat_fsinfo *fsinfo;
} sdcard_cntrl;

sdcard_cntrl sdcardmap[2];

s32 fat_init_volume(s32 chn)
{
	sdcard_cntrl *card = NULL;

	if(chn<FAT_DEVCHANNEL_0 || chn>=FAT_DEVCHANNEL_2) return -3;
	card = &sdcardmap[chn];
	
	return 1;
}
