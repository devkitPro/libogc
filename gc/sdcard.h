#ifndef __SDCARD_H__
#define __SDCARD_H__

#include <time.h>
#include <gctypes.h>

#define SDCARD_ERROR_READY				 0
#define SDCARD_ERROR_EOF				-1
#define SDCARD_ERROR_BUSY				-2
#define SDCARD_ERROR_WRONGDEVICE		-3
#define SDCARD_ERROR_NOCARD				-4
#define SDCARD_ERROR_IOERROR			-5
#define SDCARD_ERROR_OUTOFMEMORY		-6
#define SDCARD_ERROR_FATALERROR			-128

#define SDCARD_OPEN_R                    1
#define SDCARD_OPEN_W                    2

#define SDCARD_SEEK_SET					 0
#define SDCARD_SEEK_CUR					 1
#define SDCARD_SEEK_END					 2

#define SDCARD_ATTR_ARCHIVE             0x20
#define SDCARD_ATTR_DIR					0x10
#define SDCARD_ATTR_VOL					0x08
#define SDCARD_ATTR_SYSTEM				0x04
#define SDCARD_ATTR_HIDDEN				0x02
#define SDCARD_ATTR_RONLY				0x01

#define SDCARD_MAX_PATH_LEN				256

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _dir {
	u8 fname[SDCARD_MAX_PATH_LEN];
	u32 fsize;
	u32 fattr;
} DIR;

typedef struct _sdstats {
	u32 dev;
	u32 ino;
	u32 attr;
	u32 size;
	u32 blk_cnt;
	u32 blk_sz;
	struct tm ftime;
} SDSTAT;

typedef void sd_file;

s32 SDCARD_Init();
sd_file* SDCARD_OpenFile(const char *filename,const char *mode);
s32 SDCARD_ReadFile(sd_file *file,void *buf,u32 len);
s32 SDCARD_SeekFile(sd_file *file,s32 offset,u32 whence);
s32 SDCARD_WriteFile(sd_file *pfile,const void *buf,u32 len);
s32 SDCARD_GetFileSize(sd_file *file);
s32 SDCARD_ReadDir(const char *dirname,DIR **pdir_list);
s32 SDCARD_CloseFile(sd_file *pfile);
s32 SDCARD_Unmount(const char *devname);
s32 SDCARD_GetStats(sd_file *file,SDSTAT *st);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
