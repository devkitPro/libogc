#ifndef __SDCARD_H__
#define __SDCARD_H__

#include <gctypes.h>

#define SDCARD_ERROR_READY				 0
#define SDCARD_ERROR_BUSY				-1
#define SDCARD_ERROR_WRONGDEVICE		-2
#define SDCARD_ERROR_NOCARD				-3
#define SDCARD_ERROR_IOERROR			-5
#define SDCARD_ERROR_FATALERROR			-128

#define SDCARD_OPEN_R                    1
#define SDCARD_OPEN_W                    2

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _dir_entry {
	u8 name[16];
} dir_entry;

typedef u32 f_handle;
typedef void (*SDCCallback)(s32 chn,s32 result);

s32 SDCARD_Init();
s32 SDCARD_Term(s32 drv_no);
void SDCARD_RegisterCallback(u32 drv_no,void (*pFuncIN)(s32),void (*pFuncOUT)(s32));

s32 SDCARD_OpenFile(const char *filename,u32 open_mode,f_handle *p_handle);
s32 SDCARD_ReadDir(const char *dirname,u32 entry_start,u32 entry_cnt,dir_entry *dir_buf,u32 *read_cnt);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
