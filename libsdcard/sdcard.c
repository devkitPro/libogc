#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

static s32 __sdcard_read(F_HANDLE handle,void *dest,u32 offset,u32 size)
{
	u32 read_cnt = 0;
	
	if(offset>0) {
		u32 sd_offset = -1;
		u32 mode = 1;
		card_seekFile(handle,mode,offset,&sd_offset);
	}

	card_readFile(handle,dest,size,&read_cnt);
	return read_cnt;
}

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
	
	printf("filename = %s, mode = %c\n",filename,mode[0]);

	rfile = (struct _sd_file*)__lwp_wkspace_allocate(sizeof(struct _sd_file));
	if(!rfile)
		return NULL;

	rfile->handle = -1;
	if(mode[0]=='r') {
		err = card_openFile(filename,OPEN_R,&rfile->handle);
		if(err==1) {
			card_getFileSize(filename,&rfile->size);
			strncpy(rfile->path,filename,SDCARD_MAX_PATH_LEN);
			rfile->pos = 0;
			rfile->mode = 'r';
			rfile->wbuffer = NULL;
			return (sd_file*)rfile;
		}
	}
	if(mode[0]=='w') {
		err = card_openFile(filename,OPEN_W,&rfile->handle);
		if(err==1) {
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
	s32 ret = SDCARD_ERROR_READY; 
	u8 *buffer = (u8*)buf;
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile && ifile->mode=='r' && ifile->handle!=-1) {
		ret = __sdcard_read(ifile->handle,buffer,ifile->pos,len);
		ifile->pos += len;
	}
	return ret;
}

s32 SDCARD_SeekFile(sd_file *pfile,u32 offset,u32 whence)
{
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile) {
		if(whence==SDCARD_SEEK_SET) ifile->pos = 0;
		else if(whence==SDCARD_SEEK_CUR) ifile->pos += offset;
		else if(whence==SDCARD_SEEK_END) ifile->pos = ifile->size;
		else return -1;
		return 0;
	}
	return -1;
}

s32 SDCARD_GetFileSize(sd_file *pfile)
{
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile) return ifile->size;
	return 0;
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

	if(ifile && ifile->handle!=-1) {
		if(ifile->mode=='r') {
			card_closeFile(ifile->handle);
		}
		else if(ifile->mode=='w') {
		}
		__lwp_wkspace_free(ifile);
	}
	return SDCARD_ERROR_READY;
}

s32 SDCARD_Unmount(const char *devname)
{
	return card_unmountFAT(devname);
}
