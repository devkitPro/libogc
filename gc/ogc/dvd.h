#ifndef __DVD_H__
#define __DVD_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _dvddiskid dvddiskid;

struct _dvddiskid {
	s8 gamename[4];
	s8 company[2];
	u8 disknum;
	u8 gamever;
	u8 streaming;
	u8 streambufsize;
	u8 pad[22];
};

typedef struct _dvdcmdblk dvdcmdblk;

typedef void (*dvdcbcallback)(s32 result,dvdcmdblk *block);

struct _dvdcmdblk {
	dvdcmdblk *next;
	dvdcmdblk *prev;
	u32 cmd;
	s32 state;
	s32 offset;
	u32 len;
	void *buf;
	u32 currtxsize;
	u32 txdsize;
	dvddiskid *id;
	dvdcbcallback cb;
	void *usrdata;
};

typedef struct _dvddrvinfo dvddrvinfo;

struct _dvddrvinfo {
	u16 rev_level;
	u16 dev_code;
	u32 rel_date;
};

typedef struct _dvdfileinfo dvdfileinfo;

typedef void (*dvdcallback)(s32 result,dvdfileinfo *info);

struct _dvdfileinfo {
	dvdcmdblk block;
	u32 addr;
	u32 len;
	dvdcallback cb;
};

void DVD_Init();
void DVD_Reset();
void DVD_Pause();
void DVD_UnlockDrive(dvdcallback swapcb);
s32 DVD_Inquiry(dvdcmdblk *block,dvddrvinfo *info);
s32 DVD_InquiryAsync(dvdcmdblk *block,dvddrvinfo *info,dvdcbcallback cb);
s32 DVD_ReadId(dvdcmdblk *block,dvddiskid *id);
s32 DVD_ReadIDAsync(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb);
s32 DVD_ReadPrio(dvdfileinfo *info,void *buf,u32 len,u32 offset,s32 prio);
s32 DVD_SeekPrio(dvdfileinfo *info,u32 offset,s32 prio);
s32 DVD_CancelAllAsync(dvdcbcallback cb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
