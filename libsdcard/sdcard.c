#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lwp_wkspace.h"
#include "card_cmn.h"
#include "card_buf.h"
#include "card_io.h"
#include "card_fat.h"

#include "sdcard.h"

//#define _SDCARD_DEBUG
/*
static void __fill_csdregister(s32 chn)
{
	sdcard_block *card = NULL;
	u8 *csd;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];

	csd = (u8*)card->csdh;
	card->csd.csd_structure = (csd[0]>>6)&0x3;
	card->csd.taac = csd[1];
	card->csd.nasc = csd[2];
	card->csd.tran_speed = csd[3];
	card->csd.ccc = ((*(u16*)&(csd[4])>>4)&0x0fff);
	card->csd.read_bl_len = csd[5]&0x0f;
	card->csd.read_bl_part = (csd[6]>>7)&0x01;
	card->csd.write_bl_misal = (csd[6]>>6)&0x01;
	card->csd.read_bl_misal = (csd[6]>>5)&0x01;
	card->csd.dsr_imp = (csd[6]>>4)&0x01;
	card->csd.c_size = (u32)(((csd[6]&0x03)<<10)|(csd[7]<<2)|((csd[8]>>6)&0x03));
	card->csd.vdd_r_curr_min = (csd[8]>>3)&0x07;
	card->csd.vdd_r_curr_max = csd[8]&0x07;
	card->csd.vdd_w_curr_min = (csd[9]>>5)&0x07;
	card->csd.vdd_w_curr_max = (csd[9]>>2)&0x07;
	card->csd.c_size_mult = ((csd[9]&0x03)<<1)|((csd[10]>>7)&0x01);
	card->csd.erase_bl_en = (csd[10]>>6)&0x01;
	card->csd.sector_size = ((csd[10]&0x3f)<<1)|((csd[11]>>1)&0x01);
	card->csd.wp_grp_size = csd[11]&0x7f;
	card->csd.wp_grp_en = (csd[12]>>7)&0x01;
	card->csd.r2w_fact = (csd[12]>>2)&0x07;
	card->csd.write_bl_len = ((csd[12]&0x03)<<2)|((csd[13]>>6)&0x03);
	card->csd.write_bl_part = (csd[13]>>5)&0x01;
	card->csd.file_fmt_grp = (csd[14]>>7)&0x01;
	card->csd.copy = (csd[14]>>6)&0x01;
	card->csd.perm_write_prot = (csd[14]>>5)&0x01;
	card->csd.tmp_write_prot = (csd[14]>>4)&0x01;
	card->csd.file_fmt = (csd[14]>>2)&0x03;
}

static void __fill_cidregister(s32 chn)
{
	sdcard_block *card = NULL;
	u8 *cid;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];

	cid = (u8*)card->cidh;
	card->cid.mid = cid[0];
	card->cid.oid[0] = cid[1];
	card->cid.oid[1] = cid[2];
	card->cid.prv = cid[8];
	card->cid.psn = *(u32*)&(cid[9]);
	card->cid.mdt = (*(u16*)&(cid[13])&0x0fff);
	memcpy(card->cid.pnm,cid+3,5);
}

static s32 __sdcard_writeD(s32 chn,void *buffer,u32 blocks,u32 sector)
{
	u8 *ptr;
	s32 ret,cnt;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	if((ret=__sdcard_sendcmd(chn,0x19,(sector*card->sector_size)))!=0) return ret;
	if((ret=__sdcard_response1(chn))==0) {
		cnt = 0;
		ptr = buffer;
		while(cnt<blocks) {
			if((ret=__sdcard_multidatawrite(chn,ptr,card->sector_size))!=0) break;
			ptr += card->sector_size;
			cnt++;
		}
	}
	
	return ret;


	return SDCARD_ERROR_READY;
}
*/

static s32 __sdcard_read(const char *filename,void *dest,u32 offset,u32 size)
{
	u32 oflags = 1;
	u32 read_cnt = 0;
	F_HANDLE handle;
	
	if(card_openFile(filename,oflags,&handle)!=0) return 0;
	if(offset>0) {
		u32 sd_offset = -1;
		u32 mode = 1;
		card_seekFile(handle,mode,offset,&sd_offset);
	}

	card_readFile(handle,dest,size,&read_cnt);
	card_closeFile(handle);

	return read_cnt;
}

s32 SDCARD_Init()
{
	time_t now;

	printf("card_init()\n");

	now = time(NULL);
	srand(now);

	card_initBufferPool();

	card_initIODefault();
	card_initFATDefault();
	
	return CARDIO_ERROR_READY;
}

file* SDCARD_OpenFile(const char *filename,const char *mode)
{
	s32 err;
	u32 size;
	u8 buffer[8];
	file *rfile = NULL;
	
	rfile = (file*)__lwp_wkspace_allocate(sizeof(file));
	if(!rfile)
		return NULL;

	if(mode[0]=='r') {
		err = __sdcard_read(filename,buffer,0,1);
		if(err==1) {
			card_getFileSize(filename,&size);
			strncpy(rfile->path,filename,SDCARD_MAX_PATH_LEN);
			rfile->pos = 0;
			rfile->mode = 'r';
			rfile->size = size;	
			return rfile;
		}
	}
	if(mode[0]=='w') {
		strncpy(rfile->path,filename,SDCARD_MAX_PATH_LEN);
		rfile->pos = 0;
		rfile->size = 0;
		rfile->mode = 'w';
		rfile->wbuffer = NULL;
		return rfile;
	}
	__lwp_wkspace_free(rfile);
	return NULL;
}
