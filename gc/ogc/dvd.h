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
	u32 state;
	u32 offset;
	u32 len;
	void *buf;
	u32 currtxsize;
	u32 txdsize;
	dvddiskid *id;
	dvdcbcallback cb;
	void *usrdata;
};

typedef void (*DVDCallback)(u32);

void DVD_Init();
s32 DVD_Read(void *pDst,s32 nLen,u32 nOffset);
s32 DVD_ReadId(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb);
void DVD_Reset();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
