#ifndef __CARD_H__
#define __CARD_H__

#include <gctypes.h>

/* slots */
#define CARD_SLOTA					0
#define CARD_SLOTB					1

#define CARD_WORKAREA				(5*8*1024)
#define CARD_READSIZE				512
#define CARD_FILENAMELEN			32
#define CARD_MAXFILES				127

/* Errors */
#define CARD_ERROR_UNLOCKED			1
#define CARD_ERROR_READY            0
#define CARD_ERROR_BUSY            -1
#define CARD_ERROR_WRONGDEVICE     -2
#define CARD_ERROR_NOCARD          -3
#define CARD_ERROR_NOFILE          -4
#define CARD_ERROR_IOERROR         -5
#define CARD_ERROR_BROKEN          -6
#define CARD_ERROR_EXIST           -7
#define CARD_ERROR_NOENT           -8
#define CARD_ERROR_INSSPACE        -9
#define CARD_ERROR_NOPERM          -10
#define CARD_ERROR_LIMIT           -11
#define CARD_ERROR_NAMETOOLONG     -12
#define CARD_ERROR_ENCODING        -13
#define CARD_ERROR_CANCELED        -14
#define CARD_ERROR_FATAL_ERROR     -128


/* File attribute defines */
#define CARD_ATTRIB_PUBLIC			0x04
#define CARD_ATTRIB_NOCOPY			0x08
#define CARD_ATTRIB_NOMOVE			0x10

/* Banner & Icon Attributes */
#define CARD_BANNER_NONE			0
#define CARD_BANNER_CI				1
#define CARD_BANNER_RGB				2
#define CARD_BANNER_W				96
#define CARD_BANNER_H				32

#define CARD_MAXICONS				8
#define CARD_ICON_NONE				0
#define CARD_ICON_CI				1
#define CARD_ICON_RGB				2
#define CARD_ICON_LOOP				0
#define CARD_ICON_BOUNCE			4
#define CARD_ICON_W					32
#define CARD_ICON_H					32

#define CARD_SPEED_END				0
#define CARD_SPEED_4				1
#define CARD_SPEED_8				2
#define CARD_SPEED_12				3

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _card_file {
	s32 chn;
	s32 filenum;
	s32 offset;
	s32 len;
	u16 iblock;
} card_file;

typedef struct _card_dir { 
      s32 chn; 
      u32 fileno; 
      u8 filename[CARD_FILENAMELEN+1]; 
      u8 gamecode[5]; 
      u8 company[3]; 
} card_dir; 

typedef void (*cardcallback)(s32 chan,s32 result);

/*new api*/
s32 CARD_Init(const char *gamecode,const char *company);
s32 CARD_Mount(s32 chn,void *workarea,cardcallback detach_cb);
s32 CARD_MountAsync(s32 chn,void *workarea,cardcallback detach_cb,cardcallback attach_cb);
s32 CARD_Unmount(s32 chn);
s32 CARD_Read(card_file *file,void *buffer,u32 len,u32 offset);
s32 CARD_ReadAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback);
s32 CARD_Open(s32 chn,const char *filename,card_file *file);
s32 CARD_Close(card_file *file);
s32 CARD_Create(s32 chn,const char *filename,u32 size,card_file *file);
s32 CARD_CreateAsync(s32 chn,const char *filename,u32 size,card_file *file,cardcallback callback);
s32 CARD_Delete(s32 chn,const char *filename);
s32 CARD_DeleteAsync(s32 chn,const char *filename,cardcallback callback);
s32 CARD_Write(card_file *file,void *buffer,u32 len,u32 offset);
s32 CARD_WriteAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback);
s32 CARD_Format(s32 chn);
s32 CARD_FormatAsync(s32 chn,cardcallback callback);
s32 CARD_GetErrorCode(s32 chn);
s32 CARD_FindFirst(s32 chn, card_dir *dir); 
s32 CARD_FindNext(card_dir *dir); 

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
