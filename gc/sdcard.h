#ifndef __SDCARD_H__
#define __SDCARD_H__

#include <gctypes.h>

#define SDCARD_ERROR_READY				 0
#define SDCARD_ERROR_BUSY				-1
#define SDCARD_ERROR_WRONGDEVICE		-2
#define SDCARD_ERROR_NOCARD				-3
#define SDCARD_ERROR_IOERROR			-5
#define SDCARD_ERROR_OUTOFMEMORY		-6
#define SDCARD_ERROR_FATALERROR			-128

#define SDCARD_OPEN_R                    1
#define SDCARD_OPEN_W                    2

#define SDCARD_SEEK_SET					 0
#define SDCARD_SEEK_CUR					 1
#define SDCARD_SEEK_END					 2

#define SDCARD_MAX_PATH_LEN				 256

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _dir {
	u8 name[128][32];
	u32 size[128];
} DIR;


typedef void sd_file;
typedef void (*SDCCallback)(s32 chn,s32 result);

s32 SDCARD_Init();

sd_file* SDCARD_OpenFile(const char *filename,const char *mode);
s32 SDCARD_ReadFile(sd_file *file,void *buf,u32 len);
s32 SDCARD_SeekFile(sd_file *file,u32 offset,u32 whence);
s32 SDCARD_GetFileSize(sd_file *file);
s32 SDCARD_ReadDir(const char *dirname,DIR *pdir_list);
s32 SDCARD_CloseFile(sd_file *pfile);
s32 SDCARD_Term(s32 drv_no);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
