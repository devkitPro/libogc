#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "lwp_queue.h"
#include "lwp_wkspace.h"
#include "card_cmn.h"
#include "card_buf.h"
#include "card_io.h"
#include "card_fat.h"

#include "libogcsys/iosupp.h"

#include "sdcard.h"

#define SDCARD_FILEHANDLE_MAX			64

struct _sd_file {
	lwp_node node;
	u32 offset;
	u32 size;
	u8 mode;
	u8 path[SDCARD_MAX_PATH_LEN];
	F_HANDLE handle;
};

static u32 sdcard_inited = 0;
static lwp_queue sdcard_filehandle_queue;
static struct _sd_file filehandle_array[SDCARD_FILEHANDLE_MAX];

//#define _SDCARD_DEBUG

extern const devoptab_t dotab_sdcardio;
extern u32 card_convertUniToStr(u8 *dest,u16 *src);

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

		__lwp_queue_initialize(&sdcard_filehandle_queue,filehandle_array,SDCARD_FILEHANDLE_MAX,sizeof(struct _sd_file));
	}	
	return CARDIO_ERROR_READY;
}

sd_file* SDCARD_OpenFile(const char *filename,const char *mode)
{
	u32 created;
	s32 err,oldoffset;
	struct _sd_file *rfile = NULL;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_OpenFile(%s,%c)\n",filename,mode[0]);
#endif
	rfile = (struct _sd_file*)__lwp_queue_get(&sdcard_filehandle_queue);
	if(!rfile) return NULL;

	created = 0;
	rfile->handle = -1;
	err = CARDIO_ERROR_FATALERROR;
	if(mode[0]=='r') {
		err = card_openFile(filename,OPEN_R,&rfile->handle);
		if(err==CARDIO_ERROR_READY) {
			rfile->size = 0;
			rfile->offset = 0;
			rfile->mode = 'r';
			strncpy((char*)rfile->path,filename,SDCARD_MAX_PATH_LEN);
			card_getFileSize(filename,&rfile->size);
#ifdef _SDCARD_DEBUG
			printf("filesize = %d, handle = %d\n",rfile->size,rfile->handle);
#endif
			return (sd_file*)rfile;
		}
	}
	if(mode[0]=='w') {
		if(mode[1]=='+') err = card_openFile(filename,OPEN_W,&rfile->handle);
		if(err!=CARDIO_ERROR_READY) {
			err = card_createFile(filename,ALWAYS_CREATE,&rfile->handle);
			if(err==CARDIO_ERROR_READY) created = 1;
		}
		if(err==CARDIO_ERROR_READY) {
			rfile->offset = 0;
			rfile->size = 0;
			rfile->mode = 'w';
			strncpy((char*)rfile->path,filename,SDCARD_MAX_PATH_LEN);

			if(!created && mode[1]=='+') {
				card_getFileSize(filename,&rfile->size);
				card_seekFile(rfile->handle,FROM_BEGIN,rfile->size,&oldoffset);
				rfile->offset = rfile->size;
			}
			return (sd_file*)rfile;
		}
	}
	__lwp_queue_append(&sdcard_filehandle_queue,&rfile->node);
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
		if(ret!=CARDIO_ERROR_READY && ret!=CARDIO_ERROR_EOF) return SDCARD_ERROR_IOERROR;
		else if(ret==CARDIO_ERROR_EOF && (s32)readcnt<=0) return SDCARD_ERROR_EOF;
		
		ifile->offset += readcnt;
	}
	return readcnt;
}

s32 SDCARD_WriteFile(sd_file *pfile,const void *buf,u32 len)
{
	u32 ret = SDCARD_ERROR_IOERROR;
	struct _sd_file *ifile = (struct _sd_file*)pfile;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_WriteFile(%p,%p,%d)\n",pfile,buf,len);
#endif
	if(ifile && ifile->mode=='w' && ifile->handle!=-1) {
		ret = card_writeFile(ifile->handle,buf,len);
		if(ret!=SDCARD_ERROR_READY) return SDCARD_ERROR_EOF;

		ifile->offset += len;
		ifile->size += len;
	}
	return len;
}

s32 SDCARD_SeekFile(sd_file *pfile,s32 offset,u32 whence)
{
	s32 oldoffset;
	s32 len,ret;
	struct _sd_file *ifile = (struct _sd_file*)pfile;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_SeekFile(%p,%d,%d)\n",ifile,offset,whence);
#endif
	if(ifile) {
		if(whence==SDCARD_SEEK_SET) {
			if(offset>0 && offset<=ifile->size) ifile->offset = offset;
			else ifile->offset = 0;
		} else if(whence==SDCARD_SEEK_CUR) {
			len = ifile->offset+offset;
			if(len>0 && len<ifile->size) ifile->offset = len;
		} else if(whence==SDCARD_SEEK_END) {
			len = ifile->size+offset;
			if(len>0 && len<ifile->size) ifile->offset = len;
			else ifile->offset = ifile->size;
		} else
			return SDCARD_ERROR_EOF;

		ret = card_seekFile(ifile->handle,FROM_BEGIN,ifile->offset,&oldoffset);
		if(ret!=CARDIO_ERROR_READY && ret!=CARDIO_ERROR_EOF) return SDCARD_ERROR_IOERROR;

		return SDCARD_ERROR_READY;
	}
	return SDCARD_ERROR_EOF;
}

s32 SDCARD_GetFileSize(sd_file *pfile)
{
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile) return ifile->size;
	return SDCARD_ERROR_EOF;
}

s32 SDCARD_ReadDir(const char *dirname,DIR **pdir_list)
{
	s32 ret,slen;
	u32 size;
	u32 count = 0;
	u32 cnt,entries = 0;
	file_stat stats;
	dir_entryex dent;
	DIR *pdir,*ent;
	u8 fpath[SDCARD_MAX_PATH_LEN];
	u8 buffer[SDCARD_MAX_PATH_LEN];

	if(!pdir_list || !dirname) return SDCARD_ERROR_FATALERROR;

	strcpy((char*)fpath,dirname);
	slen = strlen((const char*)fpath);

	if(slen<SDCARD_MAX_PATH_LEN && fpath[slen-1]!='\\') {
		fpath[slen++] = '\\';
		fpath[slen] = 0;
	}

	*pdir_list = NULL;
	ret = card_getListNumDir((const char*)fpath,&entries);
	if(ret!=0) return SDCARD_ERROR_FATALERROR;
	
	pdir = (DIR*)malloc(entries*sizeof(DIR));
	if(!pdir) return SDCARD_ERROR_FATALERROR;
	
	cnt = 0;
	while(cnt<entries) {
		ent = &pdir[cnt];
		card_readDir((const char*)fpath,cnt,1,&dent,&count);
		card_convertUniToStr(ent->fname,dent.long_name);
		if(ent->fname[0]==0) strncpy((char*)ent->fname,(const char*)dent.name,16);

		sprintf((char*)buffer,"%s%s",fpath,dent.name);
		card_getFileSize((const char*)buffer,&size);
		ent->fsize = size;

		card_readStat((const char*)buffer,&stats);
		ent->fattr = stats.attr;
		
		cnt++;
	}
	
	*pdir_list = pdir;
	return entries;
}

s32 SDCARD_CloseFile(sd_file *pfile)
{
	struct _sd_file *ifile = (struct _sd_file*)pfile;

	if(ifile) {
		if(ifile->handle!=-1) card_closeFile(ifile->handle);
		__lwp_queue_append(&sdcard_filehandle_queue,&ifile->node);
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
	if(card_readStat((const char*)ifile->path,&stats)==CARDIO_ERROR_READY) {
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
