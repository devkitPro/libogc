#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "lwp.h"
#include "irq.h"
#include "ogcsys.h"
#include "system.h"
#include "dvd.h"

#define _DVD_DEBUG

#define DVD_BRK				(1<<0)
#define DVD_DE_MSK			(1<<1)
#define DVD_DE_INT			(1<<2)
#define DVD_TC_MSK			(1<<3)
#define DVD_TC_INT			(1<<4)
#define DVD_BRK_MSK			(1<<5)
#define DVD_BRK_INT			(1<<6)

#define DVD_CVR_INT			(1<<2)
#define DVD_CVR_MSK			(1<<1)
#define DVD_CVR_STATE		(1<<0)

#define DVD_DI_MODE			(1<<2)
#define DVD_DI_DMA			(1<<1)
#define DVD_DI_START		(1<<0)

#define DVD_DISKIDSIZE		0x20

#define DVD_DVDINQUIRY		0x12000000
#define DVD_READSECTOR		0xA8000000
#define DVD_READDISKID		0xA8000040
#define DVD_SEEKSECTOR		0xAB000000
#define DVD_REQUESTERROR	0xE0000000
#define DVD_STOPMOTOR		0xE3000000

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef void (*dvdcallbacklow)(s32);
typedef void (*dvdstatecb)(dvdcmdblk *);

typedef struct _dvdcmdl {
	s32 cmd;
	void *buf;
	u32 len;
	u32 offset;
	dvdcallbacklow cb;
} dvdcmdl;

typedef struct _dvdcmds {
	void *buf;
	u32 len;
	u32 offset;
} dvdcmds;

typedef struct _dvdwaitq {
	dvdcmdblk *next;
	dvdcmdblk *prev;
} dvdwaitq;

typedef struct _dvdapploader {
	u8 revision[16];
	void (*init)(void *,void *,void *);
	u32 len;
	u8 pad[8];
} dvdapploader;

static u32 __dvd_initflag = 0;
static u32 __dvd_stopnextint = 0;
static u32 __dvd_resetoccured = 0;	
static u32 __dvd_waitcoverclose = 0;
static u32 __dvd_breaking = 0;
static u32 __dvd_resetrequired = 0;
static u32 __dvd_canceling = 0;
static s64 __dvd_lastresetend = 0;
static u32 __dvd_lastlen;
static u32 __dvd_nextcmdnum;
static u32 __dvd_workaround;
static u32 __dvd_workaroundseek;
static u32 __dvd_lastcmdwasread;
static u32 __dvd_currcmd;
static lwpq_t __dvd_wait_queue;
static sysalarm __dvd_timeoutalarm;
static sysalarm __dvd_resetalarm;
static dvdcmdblk __dvd_dummycmdblk;
static dvddiskid __dvd_tmpid0 ATTRIBUTE_ALIGN(32);
static dvddrvinfo __dvd_driveinfo ATTRIBUTE_ALIGN(32);
static dvdapploader __dvd_apploader ATTRIBUTE_ALIGN(32);
static dvdcallbacklow __dvd_callback = NULL;
static dvdcallbacklow __dvd_resetcovercb = NULL;
static dvdcallbacklow __dvd_swapdiskcallback = NULL;
static dvdcallbacklow __dvd_finalunlockcb = NULL;
static dvdstatecb __dvd_laststate = NULL;
static dvdcbcallback __dvd_cancelcallback = NULL;
static dvdcmdblk *__dvd_executing = NULL;

static dvdcmdblk __dvd_buffer$15;
static dvdwaitq __dvd_waitingqueue[4];
static dvdcmdl __dvd_cmdlist[4];
static dvdcmds __dvd_cmd_curr,__dvd_cmd_prev;

static u8 __dvd_patchcode[] = {
	0x00,0x85,0x02,
	0x0c,0xea,0x00,0xfc,0xb4,0x00,0xf3,0xfe,0xc0,0xda,0xfc,0x00,0x02,
	0x0c,0xea,0x15,0xc8,0x0e,0xfc,0xd8,0x54,0xe9,0x03,0xfd,0x86,0x00,
	0x0c,0xc8,0x0e,0xfc,0xf4,0x74,0x77,0x0a,0x08,0xf0,0x00,0xfe,0xf5,
	0x0c,0xd8,0xda,0xfc,0xf5,0xd8,0x44,0xfc,0xf2,0x7c,0xd0,0x04,0xcc,
	0x0c,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0xf4,0x75,0x42,0x85,
	0x0c,0x00,0x51,0x20,0xfe,0xf5,0x5e,0x00,0xf5,0x5f,0x04,0x5d,0x08,
	0x0c,0x5e,0x0c,0x82,0xf2,0xf3,0xf2,0x3e,0xd2,0x02,0xf4,0x75,0x24,
	0x0c,0x00,0x40,0x31,0xd9,0x00,0xe9,0x02,0x8a,0x08,0xf7,0x10,0xff,
	0x0c,0xf7,0x21,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x0a,0x80,0x00,0xf5,
	0x0c,0x10,0x09,0x80,0x00,0xc0,0x20,0x82,0x21,0xd9,0x06,0xe9,0x06,
	0x0c,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,0x74,0x50,0xb1,0x08,0xf4,
	0x0c,0xc8,0xba,0x85,0x00,0xd8,0x29,0xe9,0x05,0xf4,0x74,0x33,0xae,
	0x0c,0x08,0xf7,0x48,0x84,0x00,0xe9,0x05,0xf4,0x74,0x80,0xae,0x08,
	0x0c,0xf0,0x00,0xc8,0x20,0x82,0xd4,0x01,0xc0,0x20,0x82,0xf7,0x48,
	0x0c,0x00,0x06,0xe9,0x0b,0x80,0x00,0xc0,0x20,0x82,0xf8,0x05,0x02,
	0x0c,0xc0,0x44,0x80,0xfe,0x00,0xd3,0xfc,0x0e,0xf4,0xc8,0x03,0x0a,
	0x0c,0x08,0xf4,0x44,0xba,0x85,0x00,0xf4,0x74,0x07,0x85,0x00,0xf7,
	0x0c,0x20,0x4c,0x80,0xf4,0xc0,0x16,0xeb,0x40,0xf7,0x00,0xff,0xef,
	0x0c,0xfd,0xd0,0x00,0xf8,0x0d,0x7f,0xfd,0x97,0x00,0xf8,0x0e,0x7f,
	0x0c,0xfd,0x91,0x00,0xf8,0x0b,0x7f,0xfd,0x8b,0x00,0xf8,0x00,0x02,		//fsd
	0x0c,0xfd,0x73,0x00,0xf8,0x0b,0x7f,0xfd,0x7f,0x00,0xf8,0x0a,0x7f,
	0x0c,0xfd,0x79,0x00,0xf8,0x0d,0x7f,0xfd,0x73,0x00,0xf8,0x80,0x00,
	0x0c,0xfd,0x5b,0x00,0xf4,0xc0,0x16,0xeb,0x40,0xf7,0x40,0x00,0x10,
	0x0c,0xfd,0x94,0x00,0x80,0x0c,0xf4,0xca,0xba,0x85,0x00,0xdc,0x8e,
	0x0c,0x81,0xda,0x29,0xe9,0x03,0xdc,0x9a,0x81,0xf7,0x4a,0x84,0x00,
	0x0c,0xe9,0x03,0xdc,0x96,0x81,0x10,0xf4,0x70,0x30,0xb1,0x00,0xf4,
	0x0c,0xca,0xba,0x85,0x00,0xda,0x29,0xe9,0x05,0xf4,0x70,0x13,0xae,
	0x0c,0x00,0xf7,0x4a,0x84,0x00,0xe9,0x05,0xf4,0x70,0x60,0xae,0x00,
	0x0c,0xc0,0xd2,0xfc,0x80,0x08,0xc0,0xd4,0xfc,0x80,0x0c,0xc4,0xda,
	0x0c,0xfc,0x80,0x00,0xc0,0x20,0x82,0x2e,0xd3,0x04,0xfe,0xf4,0x71,
	0x0c,0xff,0xff,0xff,0xf7,0x1d,0x01,0x00,0xe9,0xfa,0xf7,0x1c,0x01,
	0x0c,0x00,0xe9,0xef,0xfe,0xd3,0xfc,0xf9,0x00,0x10,0x0d,0x4c,0x02,
	0x0c,0xf2,0x7c,0x80,0x02,0xf4,0xca,0xba,0x85,0x00,0xf7,0x4a,0xc9,
	0x0c,0x00,0xe9,0x05,0xf4,0xe1,0x90,0x2a,0x08,0xda,0x29,0xe9,0x05,
	0x0c,0xf4,0xe1,0x6a,0x27,0x08,0xf7,0x4a,0x84,0x00,0xe9,0x05,0xf4,
	0x0c,0xe1,0xac,0x27,0x08,0xd3,0x04,0xfe,0xf4,0x40,0x16,0xeb,0x40,
	0x0c,0xa5,0xf4,0xca,0xba,0x85,0x00,0xf7,0x4a,0xc9,0x00,0xe9,0x05,
	0x0c,0xf4,0xe1,0x0d,0x49,0x08,0xda,0x29,0xe9,0x05,0xf4,0xe1,0x7e,
	0x0c,0x49,0x08,0xf7,0x4a,0x84,0x00,0xe9,0x05,0xf4,0xe1,0x6e,0x49,
	0x06,0x08,0xfe,0xfe,0x74,0x08,0x06,
	0x00,0x80,0x4c,
	0x03,0x02,0x85,0x00,
	0x63
};

static vu32* const _piReg = (u32*)0xCC003000;
static vu32* const _diReg = (u32*)0xCC006000;

static u8 __dvd_unlockcmd$221[12] = {0xff,0x01,'M','A','T','S','H','I','T','A',0x02,0x00};
static u8 __dvd_unlockcmd$222[12] = {0xff,0x00,'D','V','D','-','G','A','M','E',0x03,0x00};

void __dvd_stategettingerror();
void __dvd_statecoverclosed();
void __dvd_stateready();
void __dvd_statemotorstopped();
void __dvd_stateerror(s32 result);
void __dvd_statecoverclosed_cmd(dvdcmdblk *block);
void __dvd_statebusy(dvdcmdblk *block);
s32 __issuecommand(s32 prio,dvdcmdblk *block);
s32 DVD_LowSeek(u32 offset,dvdcallbacklow cb);
s32 DVD_LowRead(void *buf,u32 len,u32 offset,dvdcallbacklow cb);
s32 DVD_LowReadId(dvddiskid *diskID,dvdcallbacklow cb);
s32 DVD_LowRequestError(dvdcallbacklow cb);
s32 DVD_LowStopMotor(dvdcallbacklow cb);
s32 DVD_LowInquiry(dvddrvinfo *info,dvdcallbacklow cb);
s32 DVD_LowWaitCoverClose(dvdcallbacklow cb);
s32 DVD_ReadAbsAsyncPrio(dvdcmdblk *block,void *buf,u32 len,u32 offset,dvdcbcallback cb,s32 prio);

extern void udelay(int us);
extern u32 diff_msec(unsigned long long start,unsigned long long end);
extern long long gettime();
extern void __MaskIrq(u32);
extern void __UnmaskIrq(u32);

extern u8 sega[];

static __inline__ void __dvd_clearwaitingqueue()
{
	u32 i;

	for(i=0;i<4;i++) {
		__dvd_waitingqueue[i].next = (void*)&__dvd_waitingqueue[i];
		__dvd_waitingqueue[i].prev = (void*)&__dvd_waitingqueue[i];
	}
}

static __inline__ s32 __dvd_checkwaitingqueue()
{
	u32 i,level;

	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) {
		if((u32)&__dvd_waitingqueue[i]!=(u32)__dvd_waitingqueue[i].next) {
			_CPU_ISR_Restore(level);
			return 1;
		}
	}
	_CPU_ISR_Restore(level);
	return 0;
}

static __inline__ s32 __dvd_pushwaitingqueue(s32 prio,dvdcmdblk *block)
{
	u32 level;
#ifdef _DVD_DEBUG
	printf("__dvd_pushwaitingqueue(%d,%p)\n",prio,block);
#endif
	_CPU_ISR_Disable(level);
	__dvd_waitingqueue[prio].prev->next = block;
	block->prev = __dvd_waitingqueue[prio].prev;
	block->next = (void*)&__dvd_waitingqueue[prio];
	__dvd_waitingqueue[prio].prev = block;
	_CPU_ISR_Restore(level);
#ifdef _DVD_DEBUG
	printf("__dvd_pushwaitingqueue(%p,%p,%p)\n",&__dvd_waitingqueue[prio],__dvd_waitingqueue[prio].prev,__dvd_waitingqueue[prio].next);
#endif
	return 1;
}

static __inline__ dvdcmdblk* __dvd_popwaitingqueueprio(s32 prio)
{
	u32 level;
	dvdcmdblk *ret = NULL;
#ifdef _DVD_DEBUG
	printf("__dvd_popwaitingqueueprio(%d)\n",prio);
#endif
	_CPU_ISR_Disable(level);
	ret = __dvd_waitingqueue[prio].next;
	__dvd_waitingqueue[prio].next = ret->next;
	ret->next->prev = (void*)&__dvd_waitingqueue[prio];
	_CPU_ISR_Restore(level);
	ret->next = NULL;
	ret->prev = NULL;
	return ret;
}

static __inline__ dvdcmdblk* __dvd_popwaitingqueue()
{
	u32 i,level;
	dvdwaitq *q;
	dvdcmdblk *ret = NULL;
#ifdef _DVD_DEBUG
	printf("__dvd_popwaitingqueue()\n");
#endif
	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) {
		q = &__dvd_waitingqueue[i];
		if((u32)q->next!=(u32)q) {
			_CPU_ISR_Restore(level);
			ret = __dvd_popwaitingqueueprio(i);
			return ret;
		}
	}
	_CPU_ISR_Restore(level);
	
	return NULL;
}

static void __dvd_timeouthandler(sysalarm *alarm)
{
	dvdcallbacklow cb;

	__MaskIrq(IRQMASK(IRQ_PI_DI));
	cb = __dvd_callback;
	if(cb) cb(0x10);
}

static void __SetupTimeoutAlarm(const struct timespec *tp)
{
	SYS_CreateAlarm(&__dvd_timeoutalarm);
	SYS_SetAlarm(&__dvd_timeoutalarm,tp,__dvd_timeouthandler);
}

static void __Read(void *buffer,u32 len,u32 offset,dvdcallbacklow cb)
{
	u32 val;
	struct timespec tb;
#ifdef _DVD_DEBUG
	printf("__Read(%p,%d,%d)\n",buffer,len,offset);
#endif
	__dvd_callback = cb;
	__dvd_stopnextint = 0;
	__dvd_lastcmdwasread = 1;
	
	_diReg[2] = DVD_READSECTOR;
	_diReg[3] = offset>>2;
	_diReg[4] = len;
	_diReg[5] = (u32)buffer;
	_diReg[6] = len;

	__dvd_lastlen = len;

	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);
	val = _diReg[7];
	if(val>0x00a00000) {
		tb.tv_sec = 20;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	} else {
		tb.tv_sec = 10;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	}
}

static void __DoRead(void *buffer,u32 len,u32 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("__DoRead(%p,%d,%d)\n",buffer,len,offset);
#endif
	__dvd_nextcmdnum = 0;
	__dvd_cmdlist[0].cmd = -1;
	__Read(buffer,len,offset,cb);
}

static u32 __ProcessNextCmd()
{
	u32 cmd_num;
	
	cmd_num = __dvd_nextcmdnum;
	if(__dvd_cmdlist[cmd_num].cmd==0x0001) {
		__dvd_nextcmdnum++;
		__Read(__dvd_cmdlist[cmd_num].buf,__dvd_cmdlist[cmd_num].len,__dvd_cmdlist[cmd_num].offset,__dvd_cmdlist[cmd_num].cb);
		return 1;
	}

	if(__dvd_cmdlist[cmd_num].cmd==0x0002) {
		__dvd_nextcmdnum++;
		return 1;
	}
	return 0;
}

static void __DVDLowWATypeSet(u32 workaround,u32 workaroundseek)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_workaround = workaround;
	__dvd_workaroundseek = workaroundseek;
	_CPU_ISR_Restore(level);
}

static void __DVDInitWA()
{
	__dvd_nextcmdnum = 0;
	__DVDLowWATypeSet(0,0);	
}

static void __dvd_stateerrorcb(s32 result)
{
	dvdcmdblk *block;
#ifdef _DVD_DEBUG
	printf("__dvd_stateerrorcb(%d)\n",result);
#endif
	block = __dvd_executing;
	__dvd_executing = &__dvd_dummycmdblk;
	if(block->cb) block->cb(-1,block);
	if(__dvd_canceling) {
		__dvd_canceling = 0;
		if(__dvd_cancelcallback) __dvd_cancelcallback(0,block);
	}
	__dvd_stateready();
}

static void __dvd_stategettingerrorcb(s32 result)
{
	u32 val;
#ifdef _DVD_DEBUG
	printf("__dvd_stategettingerrorcb(%d)\n",result);
#endif
	if(result&0x0002) {
		__dvd_executing->state = -1;
		__dvd_stateerror(0x01234567);
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];
		__dvd_executing->state = -1;
		__dvd_stateerror(val);
		return;
	}
}

static void __dvd_statebusycb(s32 result)
{
	u32 val;
	dvdcmdblk *block;
#ifdef _DVD_DEBUG
	printf("__dvd_statebusycb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_executing->state = -1;
		return;
	}
	if(__dvd_currcmd==0x0003 || __dvd_currcmd==0x000f) {
		if(result&0x0002) {
			__dvd_executing->state = -1;
			return;
		}
		if(result==0x0001) {
			if(__dvd_currcmd==0x000f) __dvd_resetrequired = 1;
		}
	}
	if(result&0x0004) {
		
	}
	if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
		|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e) {
		__dvd_executing->txdsize += (__dvd_executing->currtxsize-_diReg[6]);
	}
	if(result&0x0008) {
		__dvd_canceling = 0;
		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		__dvd_executing->state = 10;
		if(block->cb) block->cb(-3,block);
		if(__dvd_cancelcallback) __dvd_cancelcallback(0,block);
		__dvd_stateready();
		return;
	}
	if(result&0x0001) {
		if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
			|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e) {
			if(__dvd_executing->txdsize!=__dvd_executing->len) {
				__dvd_statebusy(__dvd_executing);
				return;
			}
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = 0;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0009 || __dvd_currcmd==0x000a
			|| __dvd_currcmd==0x000b || __dvd_currcmd==0x000c) {

			val = _diReg[8];
			if(__dvd_currcmd==0x000a || __dvd_currcmd==0x000b) val <<= 2;
			
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = 0;
			if(block->cb) block->cb(val,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0006) {
			if(!__dvd_executing->currtxsize) {
				if(_diReg[8]&0x0001) {
					block = __dvd_executing;
					__dvd_executing = &__dvd_dummycmdblk;
					block->state = 9;
					if(block->cb) block->cb(-2,block);
					__dvd_stateready();
					return;
				}
				
			}
		}

		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		block->state = 0;
		if(block->cb) block->cb(0,block);
		__dvd_stateready();
		return;
	}
	if(result==0x0002) {
#ifdef _DVD_DEBUG
		printf("__dvd_statebusycb(%d,%d)\n",__dvd_executing->txdsize,__dvd_executing->len);
#endif
		if(__dvd_currcmd==0x000e) {
			__dvd_executing->state = -1;
			__dvd_stateerror(0x01234567);
			return;
		}
		if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
			|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e) {
			if(__dvd_executing->txdsize!=__dvd_executing->len) {
				__dvd_stategettingerror();
				return;			
			}
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = 0;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
		}
	}
}

static void __dvd_inquirysyncb(s32 result,dvdcmdblk *block)
{
	LWP_WakeThread(__dvd_wait_queue);
}

static void __dvd_readdiskidsynccb(s32 result,dvdcmdblk *block)
{
	LWP_WakeThread(__dvd_wait_queue);
}

static void __dvd_readsynccb(s32 result,dvdcmdblk *block)
{
	LWP_WakeThread(__dvd_wait_queue);
}

static void __dvd_seeksynccb(s32 result,dvdcmdblk *block)
{
	LWP_WakeThread(__dvd_wait_queue);
}

static void __dvd_statemotorstoppedcb(s32 result)
{
	_diReg[1] = 0;
	__dvd_executing->state = 3;
	__dvd_statecoverclosed();
}

static void __dvd_statecoverclosedcb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = -1;
	}
	
}

static void __dvd_statecheckid1cb(s32 result)
{
}

static void __dvd_alarmhandler(sysalarm *alarm)
{
	DVD_Reset();
	DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
	__dvd_laststate = __dvd_statecoverclosed_cmd;
	__dvd_statecoverclosed_cmd(__dvd_executing);
}

static void __DVDInterruptHandler(u32 nIrq,void *pCtx)
{
	s64 now;
	u32 status,ir,irm,irmm,diff;
	dvdcallbacklow cb;

	SYS_CancelAlarm(&__dvd_timeoutalarm);

	irmm = 0;
	if(__dvd_lastcmdwasread) {
		__dvd_cmd_prev.buf = __dvd_cmd_curr.buf;
		__dvd_cmd_prev.len = __dvd_cmd_curr.len;
		__dvd_cmd_prev.offset = __dvd_cmd_curr.offset;
		if(__dvd_stopnextint) irmm |= 0x0008;		
	}
	__dvd_lastcmdwasread = 0;
	__dvd_stopnextint = 0;

	status = _diReg[0];
	irm = (status&(DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK));
	ir = ((status&(DVD_DE_INT|DVD_TC_INT|DVD_BRK_INT))&(irm<<1));
#ifdef _DVD_DEBUG
	printf("__DVDInterruptHandler(status: %08x,irm: %08x,ir: %08x)\n",status,irm,ir);
#endif
	if(ir&DVD_BRK_INT) irmm |= 0x0008;
	if(ir&DVD_TC_INT) irmm |= 0x0001;
	if(ir&DVD_DE_INT) irmm |= 0x0002;

	if(irmm) __dvd_resetoccured = 0;

	_diReg[0] = (ir|irm);

	now = gettime();
	diff = diff_msec(__dvd_lastresetend,now);
	if(__dvd_resetoccured && diff>200) {
		status = _diReg[1];
		irm = status&DVD_CVR_MSK;
		ir = (status&DVD_CVR_INT)&(irm<<1);
		if(ir&0x0004) {
			cb = __dvd_resetcovercb;
			__dvd_resetcovercb = NULL;
			if(cb) {
				cb(0x0004);
			}
		}
		_diReg[1] = _diReg[1];
	} else {
		if(__dvd_waitcoverclose) {
			status = _diReg[1];
			irm = status&DVD_CVR_MSK;
			ir = (status&DVD_CVR_INT)&(irm<<1);
			if(ir&DVD_CVR_INT) irmm |= 0x0004;
			_diReg[1] = (ir|irm);
			__dvd_waitcoverclose = 0;
		} else
			_diReg[1] = 0;
	}
	
	if(irmm&0x0008) {
		if(!__dvd_breaking) irmm &= ~0x0008;
	}

	if(irmm&0x0001) {
		if(__ProcessNextCmd()) return;
	} else {
		__dvd_cmdlist[0].cmd = -1;
		__dvd_nextcmdnum = 0;
	}

	cb = __dvd_callback;
	__dvd_callback = NULL;
	if(cb) cb(irmm);

	__dvd_breaking = 0;
}

static u32 idfin;
static u32 old_samplerate;
static AIDCallback old_audiocb = NULL;
static void __dvd_swapdiskcbstopaudio()
{
	AUDIO_SetDSPSampleRate(old_samplerate);
	AUDIO_RegisterDMACallback(old_audiocb);
}

static void __dvd_swapdiskcb(s32 result)
{
	old_audiocb = AUDIO_RegisterDMACallback(__dvd_swapdiskcbstopaudio);;
	old_samplerate = AUDIO_GetDSPSampleRate();
#ifdef _DVD_DEBUG
	printf("__dvd_swapdiskcb(%d,%d)\n",result,old_samplerate);
#endif
	AUDIO_StopDMA();
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
	AUDIO_InitDMA((u32)sega,209344);
	AUDIO_StartDMA();
}

static void __dvd_unlockdone(s32 result)
{
	idfin = 1;
	LWP_WakeThread(__dvd_wait_queue);
}

static void __DVDPatchDriveCode(s32 result)
{
	u32 i;
	static u32 cmd;
	static u32 cmd_buf[3];
	static u32 stage = 0;
	static u32 nPos = 0;
	static u32 drv_address = 0;
	dvdcallbacklow cb = NULL;
#ifdef _DVD_DEBUG
	printf("__DVDPatchDriveCode()\n");
#endif
	while(1) {
		if(stage==0x0) {
			cmd = __dvd_patchcode[nPos++];
#ifdef _DVD_DEBUG
			printf("__DVDPatchDriveCode(%02x,%02x,%d)\n",stage,cmd,nPos);
#endif
			if(!cmd) {
				drv_address = _SHIFTL(__dvd_patchcode[nPos],8,8)|__dvd_patchcode[nPos+1];
				nPos += 2;
				continue;
			} else if(cmd==0x03)
				stage |= 0x0002;
			else if(cmd==0x63)
				break;

			for(i=0;i<cmd;i++,nPos++) ((u8*)cmd_buf)[i] = __dvd_patchcode[nPos];

			stage |= 0x0001;
			__dvd_callback = __DVDPatchDriveCode;
			_diReg[2] = 0xfe010100;
			_diReg[3] = drv_address;
			_diReg[4] = _SHIFTL(cmd,16,16);
			_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

			return;
		}		

		if(stage&0x0001) {
#ifdef _DVD_DEBUG
			printf("__DVDPatchDriveCode(%02x)\n",stage);
#endif
			if(stage&0x0002) {
				cb = __dvd_swapdiskcb;
				if(__dvd_swapdiskcallback) cb = __dvd_swapdiskcallback;
			}
			stage = 0;
			drv_address += cmd;
			__dvd_callback = __DVDPatchDriveCode;
			_diReg[2] = cmd_buf[0];
			_diReg[3] = cmd_buf[1];
			_diReg[4] = cmd_buf[2];
			_diReg[7] = DVD_DI_START;
			if(cb) cb(result);
			
			return;
		}
	}
	DCInvalidateRange(&__dvd_driveinfo,DVD_DISKIDSIZE);
	DVD_LowInquiry(&__dvd_driveinfo,__dvd_unlockdone);
}


static void __DVDUnlockDrive(s32 result)
{
	u32 i;
	static u32 stage = 0;
#ifdef _DVD_DEBUG
	printf("__DVDUnlockDrive()\n");
#endif
	if(stage==0x0) {
		stage = 1;

		__dvd_callback = __DVDUnlockDrive;
		_diReg[0] |= (DVD_TC_INT|DVD_DE_INT);
		_diReg[1] = 0;

		for(i=0;i<3;i++) _diReg[2+i] = ((u32*)__dvd_unlockcmd$221)[i];
		_diReg[7] = DVD_DI_START;

		return;
	}

	if(stage==0x1) {
#ifdef _DVD_DEBUG
		printf("__DVDUnlockDrive(1)\n");
#endif
		stage = 2;

		__dvd_callback = __dvd_finalunlockcb;
		for(i=0;i<3;i++) _diReg[2+i] = ((u32*)__dvd_unlockcmd$222)[i];
		_diReg[7] = DVD_DI_START;
	}
}

static void __DVDUnlockDriveLow(s32 result,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("__DVDUnlockDriveLow()\n");
#endif
	__dvd_finalunlockcb = cb;
	__DVDUnlockDrive(result);
}

void __dvd_statebusy(dvdcmdblk *block)
{
	u32 len;
#ifdef _DVD_DEBUG
	printf("__dvd_statebusy(%p)\n",block);
#endif
	__dvd_laststate = __dvd_statebusy;
	if(block->cmd>15) return;

	switch(block->cmd) {
		case 1:					//Read(Sector)
			_diReg[1] = _diReg[1];
			len = block->len-block->txdsize;
			if(len<0x80000) block->currtxsize = len;
			else block->currtxsize = 0x80000;
			DVD_LowRead(block->buf,block->currtxsize,(block->offset+block->txdsize),__dvd_statebusycb);
			return;
		case 2:					//Seek(Sector)
			_diReg[1] = _diReg[1];
			DVD_LowSeek(block->offset,__dvd_statebusycb);
			return;
		case 5:					//ReadDiskID
			_diReg[1] = _diReg[1];
			block->currtxsize = DVD_DISKIDSIZE;
			DVD_LowReadId(block->buf,__dvd_statebusycb);
			return;
		case 14:				//Inquiry
			_diReg[1] = _diReg[1];
			block->currtxsize = 0x20;
			DVD_LowInquiry(block->buf,__dvd_statebusycb);
			return;
	}
}

void __dvd_stateready()
{
#ifdef _DVD_DEBUG
	printf("__dvd_stateready()\n");
#endif
	if(!__dvd_checkwaitingqueue()) {
		__dvd_executing = NULL;
		return;
	}

	__dvd_executing = __dvd_popwaitingqueue();
	__dvd_currcmd = __dvd_executing->cmd;
	
	__dvd_executing->state = 1;
	__dvd_statebusy(__dvd_executing);
}

void __dvd_statecoverclosed()
{
	dvdcmdblk *blk;
	struct timespec tb;

	if(__dvd_currcmd==0x0004 || __dvd_currcmd==0x0005
		|| __dvd_currcmd==0x000d || __dvd_currcmd==0x000f) {
		__dvd_clearwaitingqueue();
		blk = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(-4,blk);
		__dvd_stateready();
	} else {
		DVD_Reset();
		SYS_CreateAlarm(&__dvd_resetalarm);
		
		tb.tv_sec = 1;
		tb.tv_nsec = 150*TB_NSPERMS;
		SYS_SetAlarm(&__dvd_resetalarm,&tb,__dvd_alarmhandler);
	}
}

void __dvd_statecoverclosed_cmd(dvdcmdblk *block)
{
	DVD_LowReadId(&__dvd_tmpid0,__dvd_statecoverclosedcb);
}

void __dvd_statemotorstopped()
{
	DVD_LowWaitCoverClose(__dvd_statemotorstoppedcb);
}

void __dvd_stateerror(s32 result)
{
	DVD_LowStopMotor(__dvd_stateerrorcb);
}

void __dvd_stategettingerror()
{
#ifdef _DVD_DEBUG
	printf("__dvd_stategettingerror()\n");
#endif
	DVD_LowRequestError(__dvd_stategettingerrorcb);
}

void __dvd_statecheckid()
{
	if(__dvd_currcmd==0x0003) {
		if(memcmp(&__dvd_dummycmdblk,&__dvd_executing,sizeof(dvdcmdblk))) {
			DVD_LowStopMotor(__dvd_statecheckid1cb);
			return;
		}
	}
}
s32 __issuecommand(s32 prio,dvdcmdblk *block)
{
	s32 ret;
	u32 level;
#ifdef _DVD_DEBUG
	printf("__issuecommand(%d,%p)\n",prio,block);
#endif
	if(block->cmd==0x0001 || block->cmd==0x00004
		|| block->cmd==0x0005 || block->cmd==0x000e) DCInvalidateRange(block->buf,block->len);

	_CPU_ISR_Disable(level);
	block->state = 0x0002;
	ret = __dvd_pushwaitingqueue(prio,block);
	if(!__dvd_executing) __dvd_stateready();
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_LowRead(void *buf,u32 len,u32 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowRead(%p,%d,%d)\n",buf,len,offset);
#endif
	_diReg[6] = len;
	
	__dvd_cmd_curr.buf = buf;
	__dvd_cmd_curr.len = len;
	__dvd_cmd_curr.offset = offset;
	__DoRead(buf,len,offset,cb);
	return 1;
}

s32 DVD_LowSeek(u32 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowSeek(%d)\n",offset);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;
	
	_diReg[2] = DVD_SEEKSECTOR;
	_diReg[3] = offset>>1;
	_diReg[7] = DVD_DI_START;	
	
	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);
	
	return 1;
}

s32 DVD_LowReadId(dvddiskid *diskID,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowReadId(%p)\n",diskID);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;
	
	_diReg[2] = DVD_READDISKID;
	_diReg[3] = 0;
	_diReg[4] = DVD_DISKIDSIZE;
	_diReg[5] = (u32)diskID;
	_diReg[6] = DVD_DISKIDSIZE;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);
	
	return 1;
}

s32 DVD_LowRequestError(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowRequestError()\n");
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_REQUESTERROR;
	_diReg[7] = DVD_DI_START;
	
	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowStopMotor(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowStopMotor()\n");
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_STOPMOTOR;
	_diReg[7] = DVD_DI_START;
	
	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowInquiry(dvddrvinfo *info,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowInquiry(%p)\n",info);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_DVDINQUIRY;
	_diReg[4] = 0x20;
	_diReg[5] = (u32)info;
	_diReg[6] = 0x20;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);
	
	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowWaitCoverClose(dvdcallbacklow cb)
{
	__dvd_callback = cb;
	__dvd_waitcoverclose = 1;
	__dvd_stopnextint = 0;
	_diReg[1] = 2;
	return 1;
}

void DVD_LowReset()
{
	u32 val;
#ifdef _DVD_DEBUG
	printf("DVD_LowReset()\n");
#endif
	_diReg[1] = DVD_CVR_MSK;
	val = _piReg[9];
	_piReg[9] = ((val&~0x0004)|0x0001);

	udelay(12);

	val |= 0x0004;
	val |= 0x0001;
	_piReg[9] = val;
	__dvd_resetoccured = 1;
	__dvd_lastresetend = gettime();
}

s32 DVD_ReadAbsAsyncPrio(dvdcmdblk *block,void *buf,u32 len,u32 offset,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadAbsAsyncPrio(%p,%p,%d,%d,%d)\n",block,buf,len,offset,prio);
#endif
	block->cmd = 0x0001;
	block->buf = buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_SeekAbsAsyncPrio(dvdcmdblk *block,u32 offset,dvdcbcallback cb,s32 prio)
{
	block->cmd = 0x0002;
	block->offset = offset;
	block->cb = cb;
	
	return __issuecommand(prio,block);
}

s32 DVD_InquiryAsync(dvdcmdblk *block,dvddrvinfo *info,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_InquiryAsync(%p,%p)\n",block,info);
#endif
	block->cmd = 0x000e;
	block->buf = info;
	block->len = 0x20;
	block->txdsize = 0;
	block->cb = cb;
	return __issuecommand(2,block);
}

s32 DVD_Inquiry(dvdcmdblk *block,dvddrvinfo *info)
{
	u32 state,ret,level;
#ifdef _DVD_DEBUG
	printf("DVD_InquiryAsync(%p,%p)\n",block,info);
#endif
	ret = DVD_InquiryAsync(block,info,__dvd_inquirysyncb);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0) ret = block->txdsize;
		else if(state==-1) ret = -1;
		else if(state==10) ret = -3;
		else LWP_SleepThread(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_ReadIDAsync(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadIDAsync(%p,%p)\n",block,id);
#endif
	if(!block || !id) return 0;

	block->cmd = 0x0005;
	block->buf = id;
	block->len = 0x0020;
	block->offset = 0;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_ReadID(dvdcmdblk *block,dvddiskid *id)
{
	u32 state,ret,level;
#ifdef _DVD_DEBUG
	printf("DVD_ReadID(%p,%p)\n",block,id);
#endif
	ret = DVD_ReadIDAsync(block,id,__dvd_readdiskidsynccb);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0) ret = block->txdsize;
		else if(state==-1) ret = -1;
		else LWP_SleepThread(__dvd_wait_queue);
	} while(state!=0 && state!=-1);
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_ReadPrio(dvdfileinfo *info,void *buf,u32 len,u32 offset,s32 prio)
{
	dvdcmdblk *block;
	u32 state,ret,level;
#ifdef _DVD_DEBUG
	printf("DVD_ReadPrio(%p,%p,%d,%d,%d)\n",info,buf,len,offset,prio);
#endif
	block = &info->block;
	ret = DVD_ReadAbsAsyncPrio(block,buf,len,offset,__dvd_readsynccb,prio);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0) ret = block->txdsize;
		else if(state==-1) ret = -1;
		else if(state==10) ret = -3;
		else LWP_SleepThread(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_SeekPrio(dvdfileinfo *info,u32 offset,s32 prio)
{
	dvdcmdblk *block;
	u32 state,ret,level;
#ifdef _DVD_DEBUG
	printf("DVD_SeekPrio(%p,%d,%d)\n",info,offset,prio);
#endif
	if(offset>0 && offset<info->len) {
		block = &info->block;
		ret = DVD_SeekAbsAsyncPrio(block,(info->addr+offset),__dvd_seeksynccb,prio);
		if(!ret) return -1;

		_CPU_ISR_Disable(level);
		do {
			state = block->state;
			if(state==0) ret = 0;
			else if(state==-1) ret = -1;
			else if(state==10) ret = -3;
			else LWP_SleepThread(__dvd_wait_queue);
		} while(state!=0 && state!=-1 && state!=10);
		_CPU_ISR_Restore(level);

		return ret;
	}
	return -1;
}

void DVD_Reset()
{
#ifdef _DVD_DEBUG
	printf("DVD_Reset()\n");
#endif
	DVD_LowReset();

	_diReg[0] = (DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK);
	_diReg[1] = _diReg[1];
}

static void __dvd_unlockcb(s32 result)
{
	__DVDUnlockDriveLow(result,__DVDPatchDriveCode);
}

static void alarmcb(sysalarm *alarm)
{
	DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
	DVD_LowReadId(&__dvd_tmpid0,__dvd_unlockcb);
}

static u8 buf[2048] ATTRIBUTE_ALIGN(32);

void DVD_Init()
{
	u32 level;
	struct timespec tb;
	dvdcmdblk cmd;
	dvdfileinfo info;

	if(__dvd_initflag) return;
	__dvd_initflag = 1;
#ifdef _DVD_DEBUG
	printf("DVD_Init()\n");
#endif
	__dvd_clearwaitingqueue();
	__DVDInitWA();

	IRQ_Request(IRQ_PI_DI,__DVDInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_DI));

	LWP_InitQueue(&__dvd_wait_queue);

	idfin = 0;
	DVD_Reset();
	SYS_CreateAlarm(&__dvd_resetalarm);

	tb.tv_sec = 1;
	tb.tv_nsec = 150*TB_NSPERMS;
	SYS_SetAlarm(&__dvd_resetalarm,&tb,alarmcb);
	_CPU_ISR_Disable(level);
	while(!idfin)
		LWP_SleepThread(__dvd_wait_queue);
	_CPU_ISR_Restore(level);

//	DVD_ReadID(&cmd,&__dvd_tmpid0);
	printf("%04x\n%04x\n%08x\n\n",__dvd_driveinfo.rev_level,__dvd_driveinfo.dev_code,__dvd_driveinfo.rel_date);

	DVD_ReadPrio(&info,&__dvd_apploader,32,9280,2);
	printf("%s\n%p\n%d\n",__dvd_apploader.revision,__dvd_apploader.init,__dvd_apploader.len);
}
