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

#define SDCARD_MAX_PATH_LEN				 256

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _dir {
	u8 name[16];
} dir;

typedef struct _file {
	u32 pos;
	u32 size;
	u8 mode;
	u8 path[SDCARD_MAX_PATH_LEN];
	void *wbuffer;
} file;

typedef void (*SDCCallback)(s32 chn,s32 result);

s32 SDCARD_Init();
s32 SDCARD_Term(s32 drv_no);
void SDCARD_RegisterCallback(u32 drv_no,void (*pFuncIN)(s32),void (*pFuncOUT)(s32));

file* SDCARD_OpenFile(const char *filename,const char *mode);
//s32 SDCARD_ReadDir(const char *dirname,u32 entry_start,u32 entry_cnt,dir *dir_buf,u32 *read_cnt);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
