#ifndef __DVD_H__
#define __DVD_H__

#include <gctypes.h>
#include <ogc/lwp_queue.h>

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
	lwp_node node;
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
	u8  pad[24];
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
void DVD_StartNormal();
void DVD_StartNormalAsync(dvdcbcallback cb);
void DVD_StartUnlocked(dvdcbcallback swapcb);
void DVD_StartUnlockedAsync(dvdcbcallback swapcb,dvdcbcallback cb);
s32 DVD_GetDriveStatus();
s32 DVD_GetCmdBlockStatus(dvdcmdblk *block);
s32 DVD_Inquiry(dvdcmdblk *block,dvddrvinfo *info);
s32 DVD_InquiryAsync(dvdcmdblk *block,dvddrvinfo *info,dvdcbcallback cb);
s32 DVD_ReadPrio(dvdfileinfo *info,void *buf,u32 len,u32 offset,s32 prio);
s32 DVD_SeekPrio(dvdfileinfo *info,u32 offset,s32 prio);
s32 DVD_CancelAllAsync(dvdcbcallback cb);
s32 DVD_StopStreamAtEndAsync(dvdcmdblk *block,dvdcbcallback cb);
s32 DVD_StopStreamAtEnd(dvdcmdblk *block);
dvddiskid* DVD_GetCurrentDiskID();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
