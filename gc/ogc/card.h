#ifndef __CARD_H__
#define __CARD_H__

#include <gctypes.h>

/* slots */
#define CARD_SLOTA         0
#define CARD_SLOTB         1

/* Errors */
#define CARD_ERROR_NONE			  0
#define CARD_ERROR_NOCARD		  1
#define CARD_ERROR_CORRUPT		  2
#define CARD_ERROR_FATAL		  3
#define CARD_ERROR_INIT			  4
#define CARD_ERROR_NOSPACE		  5
#define CARD_ERROR_NOENTRY		  6
#define CARD_ERROR_EXISTS		  7

#define CARD_FILENAME			 32
#define CARD_MAXFILES			127
#define CARD_MAXICONS			  8
#define CARD_SECTORSIZE		   8192
#define CARD_SYSAREA			  5

#define CARD_WRITESIZE			128
#define CARD_READSIZE			512

/* File attribute defines */
#define CARD_ATTRIB_PUBLIC		0x04
#define CARD_ATTRIB_NOCOPY		0x08
#define CARD_ATTRIB_NOMOVE		0x10

/* Banner & Icon Attributes */
#define CARD_BANNER_NONE		   0
#define CARD_BANNER_CI			   1
#define CARD_BANNER_RGB			   2
#define CARD_BANNER_W			  96
#define CARD_BANNER_H			  32

#define CARD_ICON_NONE			   0
#define CARD_ICON_CI			   1
#define CARD_ICON_RGB			   2
#define CARD_ICON_LOOP			   0
#define CARD_ICON_BOUNCE		   4
#define CARD_ICON_W				  32
#define CARD_ICON_H				  32

#define CARD_SPEED_END			   0
#define CARD_SPEED_4			   1
#define CARD_SPEED_8			   2
#define CARD_SPEED_12			   3

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _carddirentry {
	u8 gamecode[4];
	u8 company[2];
	u8 pad_ff;
	u8 bannerfmt;
	u8 filename[CARD_FILENAME];
	u32 lastmodified;
	u32 iconaddr;
	u16 iconfmt;
	u16 iconspeed;
	u8 attribute;
	u8 pad_00;
	u16 block;
	u16 length;
	u16 pad_ffff;
	u32 commentaddr;
} CardDirEntry;

typedef struct _carddirblock {
	struct _carddirentry entries[CARD_MAXFILES];
	u8 pad[58];
	u16 num;
	u16 chksum1;
	u16 chksum2;
} CardDirBlock;

typedef struct _cardfatblock {
	u16 chksum1;
	u16 chksum2;
	u16 num;
	u16 freeblocks;
	u16 lastalloc;
	u16 fat[0xffb];
} CardFatBlock;

typedef struct _cardfile {
	u8 gamecode[5];
	u8 company[3];
	u8 bannerfmt;
	u8 filename[CARD_FILENAME+1];
	u32 lastmodified;
	u32 iconaddr;
	u16 iconfmt;
	u16 iconspeed;
	u8 attribute;
	u32 commentaddr;
	u32 size;
	u32 filenum;
} CardFile;

typedef struct _cardoffset {
	u32 banner;
	u32 banner_tlut;
	u32 icons[CARD_MAXICONS];
	u32 icons_tlut;
	u32 data;
} CardOffset;

typedef void (*CARDCallback)(s32 chan,s32 result);

u32 CARD_Init(u32 nChn);
u16 CARD_RetrieveID(u32 nChn);
u8 CARD_ReadStatus(u32 nChn);
void CARD_ClearStatus(u32 nChn);
void CARD_ChipErase(u32 nChn);
void CARD_SectorErase(u32 nChn,u32 nSector);
void CARD_SectorProgram(u32 nChn,u32 nAddress,u8 *buff,u32 size);
void CARD_ReadArray(u32 nChn,u32 nAddress,u8 *buff,u32 size);
u32 CARD_IsPresent(u32 nChn);
void CARD_UpdateDir(u32 nChn);
void CARD_UpdateFat(u32 nChn);
u32 CARD_CreateFile(CardFile *pFile,CardOffset *pOffset);
u32 CARD_OpenFile(CardFile *pFile,CardOffset *pOffset);
u32 CARD_OpenFileByNum(CardFile *pFile,CardOffset *pOffset,u32 nFilenum);
u32 CARD_ReadFile(CardFile *pFile,void *pBuf);
u32 CARD_WriteFile(CardFile *pFile,void *pBuf);
u32 CARD_DeleteFile(CardFile *pFile);
u32 CARD_FileAddress(CardFile *pFile);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
