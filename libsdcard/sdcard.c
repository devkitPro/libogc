#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "lwp_wkspace.h"
#include "card_cmn.h"
#include "card_buf.h"
#include "card_io.h"
#include "card_fat.h"

#include "libogcsys/iosupp.h"

#include "sdcard.h"

struct _sd_file {
	u32 pos;
	u32 size;
	u8 mode;
	u8 path[SDCARD_MAX_PATH_LEN];
	F_HANDLE handle;
	void *wbuffer;
};

static u32 sdcard_inited = 0;

//#define _SDCARD_DEBUG

extern const devoptab_t dotab_sdcardio;

s32 SDCARD_Init()
{
	time_t now;

	if(!sdcard_inited) {
#ifdef _SDCARD_DEBUG
		printf("card_init()\n");
#endif
		sdcard_inited = 1;

		devoptab_list[STD_SDCARD] = &dotab_sdcardio;
		
		now = time(NULL);
		srand(now);

		card_initBufferPool();

		card_initIODefault();
		card_initFATDefault();
	}	
	return CARDIO_ERROR_READY;
}

sd_file* SDCARD_OpenFile(const char *filename,const char *mode)
{
	s32 err;
	struct _sd_file *rfile = NULL;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_OpenFile(%s,%c)\n",filename,mode[0]);
#endif
	rfile = (struct _sd_file*)__lwp_wkspace_allocate(sizeof(struct _sd_file));
	if(!rfile)
		return NULL;

	rfile->handle = -1;
	if(mode[0]=='r') {
		err = card_openFile(filename,OPEN_R,&rfile->handle);
		if(err==CARDIO_ERROR_READY) {
			card_getFileSize(filename,&rfile->size);
			strncpy(rfile->path,filename,SDCARD_MAX_PATH_LEN);
			rfile->pos = 0;
			rfile->mode = 'r';
			rfile->wbuffer = NULL;
#ifdef _SDCARD_DEBUG
			printf("filesize = %d, handle = %d\n",rfile->size,rfile->handle);
#endif
			return (sd_file*)rfile;
		}
	}
	if(mode[0]=='w') {
		err = card_openFile(filename,OPEN_W,&rfile->handle);
		if(err==CARDIO_ERROR_READY) {
			strncpy(rfile->path,filename,SDCARD_MAX_PATH_LEN);
			rfile->pos = 0;
			rfile->size = 0;
			rfile->mode = 'w';
			rfile->wbuffer = NULL;
		}
		return (sd_file*)rfile;
	}
	__lwp_wkspace_free(rfile);
	return NULL;
}

s32 SDCARD_ReadFile(sd_file *pfile,void *buf,u32 len)
{
	u32 readcnt,ret;
	struct _sd_file *ifile = (struct _sd_file*)pfile;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_ReadFile(%p,%p,%d)\n",pfile,buf,len);
#endif
	readcnt = SDCARD_ERROR_EOF;
	if(ifile && ifile->mode=='r' && ifile->handle!=-1) {
		ret = card_readFile(ifile->handle,buf,len,&readcnt);
		if(ret!=CARDIO_ERROR_READY) return SDCARD_ERROR_EOF;
		else if(ret==CARDIO_ERROR_EOF) return SDCARD_ERROR_EOF;
		ifile->pos += readcnt;
	}
	return readcnt;
}

s32 SDCARD_SeekFile(sd_file *pfile,s32 offset,u32 whence)
{
	s32 len;
	u32 sd_offset = -1;
	u32 mode = FROM_BEGIN;
	struct _sd_file *ifile = (struct _sd_file*)pfile;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_SeekFile(%p,%d,%d)\n",ifile,offset,whence);
#endif
	if(ifile) {
		if(whence==SDCARD_SEEK_SET) {
			if(offset>0 && offset<=ifile->size) ifile->pos = offset;
			else ifile->pos = 0;
		} else if(whence==SDCARD_SEEK_CUR) {
			len = offset+ifile->pos;
			if(len>0 && len<ifile->size) ifile->pos += offset;
		} else if(whence==SDCARD_SEEK_END) {
			len = ifile->size-offset;
			if(len>0) ifile->pos = ifile->size-offset;
		} else
			return SDCARD_ERROR_EOF;

		card_seekFile(ifile->handle,mode,ifile->pos,&sd_offset);
#ifdef _SDCARD_DEBUG
		printf("SDCARD_SeekFile(%d,%d)\n",ifile->pos,sd_offset);
#endif
		return ifile->pos;
	}
	return SDCARD_ERROR_EOF;
}

s32 SDCARD_GetFileSize(sd_file *pfile)
{
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile) return ifile->size;
	return SDCARD_ERROR_EOF;
}

s32 SDCARD_ReadDir(const char *dirname,DIR *pdir_list)
{
	s32 ret;
	u32 size;
	u8 buffer[32+16];
	u32 count = 0;
	u32 cnt,entries = 0;
	dir_entry dent;

	ret = card_getListNumDir(dirname,&entries);
	if(ret!=0) return SDCARD_ERROR_FATALERROR;
	
	if(entries>128) entries = 128;

	cnt = 0;
	while(cnt<entries) {
		card_readDir(dirname,cnt,1,&dent,&count);
		strcpy(pdir_list->name[cnt],dent.name);

		sprintf(buffer,"%s%s",dirname,dent.name);
		card_getFileSize(buffer,&size);
		pdir_list->size[cnt] = size;
		cnt++;
	}
	return entries;
}

s32 SDCARD_CloseFile(sd_file *pfile)
{
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile) {
		if(ifile->handle!=-1) {
			if(ifile->mode=='r') {
				card_closeFile(ifile->handle);
			}
			else if(ifile->mode=='w') {
			}
		}
		__lwp_wkspace_free(ifile);
	}
	return SDCARD_ERROR_READY;
}

s32 SDCARD_Unmount(const char *devname)
{
	return card_unmountFAT(devname);
}

s32 SDCARD_GetStats(sd_file *file,SDSTAT *st)
{
	s32 ret;
	file_stat stats;
	struct _sd_file *ifile = (struct _sd_file*)file;

	if(!file || !st) return SDCARD_ERROR_FATALERROR;
	
	ret = SDCARD_ERROR_EOF;
	memset(st,0,sizeof(SDSTAT));
	if(card_readStat(ifile->path,&stats)==CARDIO_ERROR_READY) {
		st->dev = (ifile->handle>>24)&0x7f;
		st->ino = stats.cluster;
		st->size = stats.size;
		st->attr = stats.attr;
		st->ftime = stats.time;
		
		st->blk_sz = SECTOR_SIZE;
		st->blk_cnt = (st->size/SECTOR_SIZE);
		if(st->size%SECTOR_SIZE) st->blk_cnt++;
#ifdef _SDCARD_DEBUG
		printf("st->dev = %d\nst->ino = %d\nst->size = %d\nst->attr = %02x\nst->blk_cnt = %d\n",st->dev,st->ino,st->size,st->attr,st->blk_cnt);
#endif
		ret = SDCARD_ERROR_READY;
	}
	return ret;
}
