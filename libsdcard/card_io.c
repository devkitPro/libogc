#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ogcsys.h>

#include "asm.h"
#include "processor.h"
#include "exi.h"
#include "lwp.h"
#include "card_cmn.h"
#include "card_fat.h"
#include "card_io.h"

//#define _CARDIO_DEBUG

#define CARDIO_OP_TIMEDOUT					0x4000
#define CARDIO_OP_INITFAILED				0x8000

u8 g_CID[MAX_DRIVE][16];
u8 g_CSD[MAX_DRIVE][16];
u8 g_CardStatus[MAX_DRIVE][64];

u8 g_mCode[MAX_MI_NUM] = { 0x03 };

u16 g_dCode[MAX_MI_NUM][MAX_DI_NUM] = 
{
	{
		0x033f,			/* SD   8Mb */
		0x0383,			/* SD  16Mb */
		0x074b,			/* SD  32Mb */
		0x0edf,			/* SD  64Mb */
		0x0f03			/* SD 128Mb */
	}
};
 
static u8 _wp_flag;
static u8 _clr_flag;
static u32 _card_frq;

static lwpq_t _exi_lock[MAX_DRIVE];

static u32 _cur_page_size[MAX_DRIVE];
static u32 _err_status[MAX_DRIVE];
static u32 _ioFlag[MAX_DRIVE];
static boolean _card_inserted[MAX_DRIVE];

extern unsigned long gettick();

static __inline__ u32 __check_response(s32 drv_no,u8 *res)
{
	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	if(res[0]&0x40) _err_status[drv_no] = (_err_status[drv_no]|0x1000);
	if(res[0]&0x20) _err_status[drv_no] = (_err_status[drv_no]|0x0100);
	if(res[0]&0x08) _err_status[drv_no] = (_err_status[drv_no]|0x0002);
	if(res[0]&0x04) _err_status[drv_no] = (_err_status[drv_no]|0x0001);

	return _err_status[drv_no];
}

static __inline__ u8 __make_crc7(void *buffer,u32 len)
{
	u32 mask,cnt,bcnt;
	u32 res,val;
	u8 *ptr = (u8*)buffer;

	cnt = 0;
	res = 0;
	while(cnt<len) {
		bcnt = 0;
		mask = 0x80;
		while(bcnt<8) {
			res <<= 1;
			val = *ptr^((res>>bcnt)&0xff);
			if(val&mask) {
				res |= 0x01;
				if((res^0x0008)&0x0008) res |= 0x0008;
				else res &= ~0x0008;
				
			} else if(res&0x0008) res |= 0x0008;
			else res &= ~0x0008;
			
			mask >>= 1;
			bcnt++;
		}
		ptr++;
		cnt++;
	}
	return (res<<1)&0xff;
}

static __inline__ u16 __make_crc16(void *buffer,u32 len)
{
	u32 mask,cnt,bcnt;
	u32 res,val,tmp;
	u8 *ptr = (u8*)buffer;

	cnt = 0;
	res = 0;
	while(cnt<len) {
		bcnt = 0;
		mask = 0x80;
		val = *ptr;
		while(bcnt<8) {
			tmp = val^((res>>(bcnt+8))&0xff);
			if(tmp&mask) {
				res = (res<<1)|0x0001;
				if((res^0x0020)&0x0020) res |= 0x0020;
				else res &= ~0x0020;
				if((res^0x1000)&0x1000) res |= 0x1000;
				else res &= ~0x1000;
			} else {
				res = (res<<1)&~0x0001;
				if(res&0x0020) res |= 0x0020;
				else res &= ~0x0020;
				if(res&0x1000) res |= 0x1000;
				else res &= ~0x1000;
			}
			mask >>= 1;
			bcnt++;
		}
		ptr++;
		cnt++;
	}
	
	return (res&0xffff);
}

static __inline__ u32 __card_checktimeout(u32 startT,u32 timeout)
{
	s32 endT,diff;
	s32 msec;

	endT = gettick();
	if(startT>endT) {
		diff = ((endT+(startT-1))+1);
	} else
		diff = (endT-startT);

	msec = (diff/TB_TIMER_CLOCK);
	if(msec<=timeout) return 0;
	return 1;
}

static u32 __exi_unlock(u32 chn,u32 dev)
{
	LWP_WakeThread(_exi_lock[chn]);
	return 1;
}

static void __exi_wait(s32 drv_no)
{
	u32 ret;
	u32 level;

	_CPU_ISR_Disable(level);
	do {
		if((ret=EXI_Lock(drv_no,EXI_DEVICE_0,__exi_unlock))==1) break;
		LWP_SleepThread(_exi_lock[drv_no]);
	} while(ret==0);
	_CPU_ISR_Restore(level);
}

static u32 __card_exthandler(u32 chn,u32 dev)
{
	printf("sd card removed.\n");
	card_ejectedCB(chn);
	return 1;
}

static s32 __card_writecmd0(s32 drv_no,void *buf,s32 len)
{
	u8 crc,*ptr;
	u32 cnt;
	u8 dummy[128];

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ptr = buf;

	_clr_flag = 0xff;
	ptr[0] |= 0x40;
	crc = __make_crc7(buf,len);

	if(_wp_flag) {
		_clr_flag = 0x00;
		for(cnt=0;cnt<len;cnt++) ptr[cnt] ^= -1;
	}

	for(cnt=0;cnt<128;cnt++) dummy[cnt] = _clr_flag;

	__exi_wait(drv_no);

	if(EXI_SelectSD(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	
	cnt = 0;
	while(cnt<20) {
		if(EXI_ImmEx(drv_no,dummy,128,EXI_WRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		cnt++;
	}
	EXI_Deselect(drv_no);
	
	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	crc |= 0x01;
	if(_wp_flag) crc ^= -1;
#ifdef _CARDIO_DEBUG
	printf("command: %02x %02x %02x %02x %02x %02x\n",((u8*)buf)[0],((u8*)buf)[1],((u8*)buf)[2],((u8*)buf)[3],((u8*)buf)[4],crc);
#endif
	if(EXI_ImmEx(drv_no,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	if(EXI_ImmEx(drv_no,&crc,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return CARDIO_ERROR_READY;
}

static s32 __card_writecmd(s32 drv_no,void *buf,s32 len)
{
	u8 crc,*ptr;
	u8 dummy[32];
	u32 cnt;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ptr = buf;
	ptr[0] |= 0x40;
	crc = __make_crc7(buf,len);

	if(_wp_flag) {
		for(cnt=0;cnt<len;cnt++) ptr[cnt] ^= -1;
	}

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _clr_flag;
	
	if(EXI_ImmEx(drv_no,dummy,10,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	crc |= 0x01;
	if(_wp_flag) crc ^= -1;
#ifdef _CARDIO_DEBUG
	printf("sd command: %02x %02x %02x %02x %02x %02x\n",((u8*)buf)[0],((u8*)buf)[1],((u8*)buf)[2],((u8*)buf)[3],((u8*)buf)[4],crc);
#endif
	if(EXI_ImmEx(drv_no,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	if(EXI_ImmEx(drv_no,&crc,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return CARDIO_ERROR_READY;
}

static s32 __card_readresponse(s32 drv_no,void *buf,s32 len)
{
	u8 *ptr;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ret = CARDIO_ERROR_READY;
	ptr = buf;
	*ptr = _clr_flag;
	
	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
#ifdef _CARDIO_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
#ifdef _CARDIO_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(!(*ptr&0x80)) break;
		if(__card_checktimeout(startT,500)!=0) {
			*ptr = _clr_flag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
#ifdef _CARDIO_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr&0x80) ret = CARDIO_OP_TIMEDOUT;
			break;
		}
	}
	if(len>1) {
		*(++ptr) = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,len-1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
	}

	/* setalarm, wait */
	usleep(40);
	
	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return ret;
}

static s32 __card_stopreadresponse(s32 drv_no,void *buf,s32 len)
{
	u8 *ptr;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ptr = buf;
	
	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	*ptr = _clr_flag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
#ifdef _CARDIO_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	*ptr = _clr_flag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
#ifdef _CARDIO_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
#ifdef _CARDIO_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(!(*ptr&0x80)) break;
		if(__card_checktimeout(startT,1500)!=0) {
			*ptr = _clr_flag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
#ifdef _CARDIO_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr&0x80) ret = CARDIO_OP_TIMEDOUT;
			break;
		}
	}

	while(*ptr!=0xff) {
		ptr[1] = ptr[0];
		*ptr = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
#ifdef _CARDIO_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(*ptr==0xff) break;
		if(__card_checktimeout(startT,1500)!=0) {
			*ptr = _clr_flag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
#ifdef _CARDIO_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr!=0xff) ret = CARDIO_OP_TIMEDOUT;
			break;
		}
	}
	ptr[0] = ptr[1];

	if(len>1) {
		*(++ptr) = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,len-1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
	}
	
	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return ret;
}

static s32 __card_stopresponse(s32 drv_no)
{
	u8 res;
	s32 ret;

	ret = __card_stopreadresponse(drv_no,&res,1);
	ret |= __check_response(drv_no,&res);

	return ret;
}

static s32 __card_dataread(s32 drv_no,void *buf,u32 len)
{
	u8 *ptr;
	u32 cnt;
	u8 res[2];
	u16 crc,crc_org;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	__exi_wait(drv_no);
	
	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	ptr = buf;
	for(cnt=0;cnt<len;cnt++) ptr[cnt] = _clr_flag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
#ifdef _CARDIO_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
	
	startT = gettick();
	while(*ptr!=0xfe) {
		*ptr = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
#ifdef _CARDIO_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(*ptr==0xfe) break;
		if(__card_checktimeout(startT,1500)!=0) {
			*ptr = _clr_flag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
#ifdef _CARDIO_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr!=0xfe) ret = CARDIO_OP_TIMEDOUT;
			break;
		}
	}

	*ptr = _clr_flag;
	if(EXI_ImmEx(drv_no,ptr,len,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	/* setalarm, wait */
	usleep(40);

	res[0] = res[1] = _clr_flag;
	if(EXI_ImmEx(drv_no,res,2,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
#ifdef _CARDIO_DEBUG
	printf("sd response: %04x\n",*(u16*)res);
#endif
	crc_org = ((res[0]<<8)&0xff00)|(res[1]&0xff);

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	crc = __make_crc16(buf,len);
	if(crc==crc_org) {
#ifdef _CARDIO_DEBUG
		printf("crc ok: %04x : %04x\n",crc_org,crc);
#endif
		return CARDIO_ERROR_READY;
	}
	
	return CARDIO_ERROR_FATALERROR;
}

static s32 __card_datareadfinal(s32 drv_no,void *buf,u32 len)
{
	s32 startT,ret;
	u32 cnt;
	u8 *ptr,cmd[6];
	u16 crc_org,crc;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	ptr = buf;
	for(cnt=0;cnt<len;cnt++) ptr[cnt] = _clr_flag;
	
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
#ifdef _CARDIO_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	startT = gettick();
	while(*ptr!=0xfe) {
		*ptr = _clr_flag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
#ifdef _CARDIO_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(*ptr==0xfe) break;
		if(__card_checktimeout(startT,1500)!=0) {
			*ptr = _clr_flag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
#ifdef _CARDIO_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr!=0xfe) ret = CARDIO_OP_TIMEDOUT;
			break;
		}
	}

	*ptr = _clr_flag;
	if(EXI_ImmEx(drv_no,ptr,(len-4),EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	memset(cmd,0,6);
	cmd[0] = 0x4C;
	cmd[5] = __make_crc7(cmd,5);
	cmd[5] |= 0x0001;
	if(_wp_flag) {
		for(cnt=0;cnt<6;cnt++) cmd[cnt] ^= -1;
	}
	if(EXI_ImmEx(drv_no,cmd,6,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	ptr[len-4] = cmd[0];
	ptr[len-3] = cmd[1];
	ptr[len-2] = cmd[2];
	ptr[len-1] = cmd[3];
	crc_org = ((cmd[4]<<8)&0xff00)|(cmd[5]&0xff);
	
	/* setalarm, wait */
	usleep(40);

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	crc = __make_crc16(buf,len);
	if(crc==crc_org) {
#ifdef _CARDIO_DEBUG
		printf("crc ok: %04x : %04x\n",crc_org,crc);
#endif
		return ret;
	}
	
	return ret;
}

static s32 __card_datawrite(s32 drv_no,void *buf,u32 len)
{
	u8 dummy[32];
	u16 crc;
	u32 cnt;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _clr_flag;
	crc = __make_crc16(buf,len);
	
	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	dummy[0] = 0xfe;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	if(EXI_ImmEx(drv_no,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	/* setalarm, wait */
	usleep(40);

	ret = CARDIO_ERROR_READY;
	if(EXI_ImmEx(drv_no,&crc,2,EXI_WRITE)==0) ret = CARDIO_ERROR_IOERROR;

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	return ret;
}

static s32 __card_multidatawrite(s32 drv_no,void *buf,u32 len)
{
	u8 dummy[32];
	u16 crc;
	u32 cnt;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _clr_flag;
	crc = __make_crc16(buf,len);
	
	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	dummy[0] = 0xfc;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	if(EXI_ImmEx(drv_no,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	/* setalarm, wait */
	usleep(40);

	ret = CARDIO_ERROR_READY;
	if(EXI_ImmEx(drv_no,&crc,2,EXI_WRITE)==0) ret = CARDIO_ERROR_IOERROR;

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	return ret;
}

static s32 __card_multiwritestop(s32 drv_no)
{
	s32 ret,cnt,startT;
	u8 dummy[32];

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _clr_flag;

	__exi_wait(drv_no);
	
	if(EXI_Select(drv_no,EXI_DEVICE_0,_card_frq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}
	
	ret = CARDIO_ERROR_READY;
	dummy[0] = 0xfd;
	if(_wp_flag) dummy[0] = 0x02;		//!0xfd
	if(EXI_ImmEx(drv_no,dummy,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	dummy[0] = _clr_flag;	
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	dummy[0] = _clr_flag;	
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	dummy[0] = _clr_flag;	
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	dummy[0] = _clr_flag;	
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	
	startT = gettick();
	while(dummy[0]==0) {
		dummy[0] = _clr_flag;	
		if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(dummy[0]) break;
		if(__card_checktimeout(startT,1500)!=0) {
			dummy[0] = _clr_flag;	
			if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(!dummy[0]) ret = CARDIO_OP_TIMEDOUT;
			break;
		}
	}

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return ret;
}

static s32 __card_response1(s32 drv_no)
{
	u8 res;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	
	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	return __check_response(drv_no,&res);
}

static s32 __card_response2(s32 drv_no)
{
	u16 res;
	u32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	
	if((ret=__card_readresponse(drv_no,&res,2))!=0) return ret;
	if(!(res&0x7c) && !(res&0x9e00)) return CARDIO_ERROR_READY;
	return CARDIO_ERROR_FATALERROR;
}

static s32 __card_sendopcond(s32 drv_no)
{
	u8 cmd[5];
	u8 res;
	s32 ret;
	s32 startT;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("__card_sendopcond(%d)\n",drv_no);
#endif
	ret = 0;
	startT = gettick();
	do {
		memset(cmd,0,5);
		cmd[0] = 0x37;
		if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
			printf("__card_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
			return ret;
		}
		if((ret=__card_response1(drv_no))!=0) return ret;

		memset(cmd,0,5);
		cmd[0] = 0x29;
		if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
			printf("__card_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
			return ret;
		}
		if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
		ret = __check_response(drv_no,&res);

		if(ret!=0) return ret;
		if(!(res&0x01)) return CARDIO_ERROR_READY;
		ret = __card_checktimeout(startT,1500);
	} while(ret==0);

	memset(cmd,0,5);
	cmd[0] = 0x37;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__card_response1(drv_no))!=0) return ret;
	
	memset(cmd,0,5);
	cmd[0] = 0x29;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);

	if(ret!=0) return ret;
	if(res&0x01) ret |= CARDIO_OP_INITFAILED;

	return ret;
}

static s32 __card_sendappcmd(s32 drv_no)
{
	s32 ret;
	u8 res;
	u8 cmd[5];

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	memset(cmd,0,5);
	cmd[0] = 0x37;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0){
#ifdef _CARDIO_DEBUG
		printf("__card_sendappcmd(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);

	return ret;
}

static s32 __card_sendcmd(s32 drv_no,u8 cmd,u8 *arg)
{
	u8 ccmd[5];

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	memset(ccmd,0,5);
	ccmd[0] = cmd;
	if(arg) {
		ccmd[1] = arg[0];
		ccmd[2] = arg[1];
		ccmd[3] = arg[2];
		ccmd[4] = arg[3];
	}
	return __card_writecmd(drv_no,ccmd,5);
}

static s32 __card_setblocklen(s32 drv_no,u32 block_len)
{
	u8 cmd[5];
	u8 res;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("__card_setblocklen(%d,%d)\n",drv_no,block_len);
#endif
	memset(cmd,0,5);
	cmd[0] = 0x10;
	cmd[1] = (block_len>>24)&0xff;
	cmd[2] = (block_len>>16)&0xff;
	cmd[3] = (block_len>>8)&0xff;
	cmd[4] = block_len&0xff;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_setblocklen(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__card_readresponse(drv_no,&res,1))<0) return ret;
	ret = __check_response(drv_no,&res);
	
	return ret;
}

static s32 __card_readcsd(s32 drv_no)
{
	u8 cmd[5];
	u8 res;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("__card_readcsd(%d)\n",drv_no);
#endif
	ret = 0;
	memset(cmd,0,5);
	cmd[0] = 0x09;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_readcsd(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);
	if(ret==0) {
		if((ret=__card_dataread(drv_no,g_CSD[drv_no],16))!=0) return ret;
	}
	return ret;
}

static s32 __card_readcid(s32 drv_no)
{
	u8 cmd[5];
	u8 res;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("__card_readcid(%d)\n",drv_no);
#endif
	ret = 0;
	memset(cmd,0,5);
	cmd[0] = 0x0A;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_readcid(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);
	if(ret==0) {
		if((ret=__card_dataread(drv_no,g_CID[drv_no],16))!=0) return ret;
	}
	return ret;
}

static s32 __card_sd_status(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("__card_sd_status(%d)\n",drv_no);
#endif
	if(_cur_page_size[drv_no]!=64) {
		_cur_page_size[drv_no] = 64;
		if((ret=__card_setblocklen(drv_no,_cur_page_size[drv_no]))!=0) return ret;
	}
	if((ret=__card_sendappcmd(drv_no))!=0) return ret;
	if((ret=__card_sendcmd(drv_no,0x0d,NULL))!=0) return ret;
	if((ret=__card_response2(drv_no))!=0) return ret;
	ret = __card_dataread(drv_no,g_CardStatus[drv_no],64);
	
	return ret;
}

static s32 __card_softreset(s32 drv_no)
{
	u8 cmd[5];
	u8 res;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("__card_softreset(%d)\n",drv_no);
#endif
	ret = 0;
	memset(cmd,0,5);
	if((ret=__card_writecmd0(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_softreset(%d): sd write cmd0 failed.\n",ret);
#endif
		return ret;
	}
	
	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);

	memset(cmd,0,5);
	cmd[0] = 0x0C;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) return ret;
	
	if((ret=__card_stopreadresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);

	memset(cmd,0,5);
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
#ifdef _CARDIO_DEBUG
		printf("__card_softreset(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}

	if((ret=__card_readresponse(drv_no,&res,1))!=0) return ret;
	ret = __check_response(drv_no,&res);

	return ret;
}

static boolean __card_check(s32 drv_no)
{
	s32 ret;
	
	if(drv_no<0 || drv_no>=MAX_DRIVE) return FALSE;

	printf("__card_check(%d)\n",drv_no);
	
	while((ret=EXI_ProbeEx(drv_no))==0);
	if(ret!=1) return FALSE;

	if(!(EXI_GetState(drv_no)&EXI_FLAG_ATTACH)) {
		if(EXI_Attach(drv_no,__card_exthandler)==0) return FALSE;
		printf("__card_check(%d, attached)\n",drv_no);
		card_insertedCB(drv_no);
	}
	return TRUE;
}

void card_initIODefault()
{
	u32 i;
	
	printf("card_initIODefault()\n");

	for(i=0;i<MAX_DRIVE;++i) {
		_card_inserted[i] = FALSE;
		_ioFlag[i] = NOT_INITIALIZED;
		LWP_InitQueue(&_exi_lock[i]);
	}
}

s32 card_initIO(s32 drv_no)
{
	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	_card_inserted[drv_no] = __card_check(drv_no);

	if(_card_inserted[drv_no]==TRUE) {
		_card_frq = EXI_SPEED16MHZ;
		_wp_flag = 0;
		if(__card_softreset(drv_no)!=0) {
			_wp_flag = 1;
			if(__card_softreset(drv_no)!=0) goto exit;
		}
		
		if(__card_sendopcond(drv_no)!=0) goto exit;
		if(__card_readcsd(drv_no)!=0) goto exit;
		if(__card_readcid(drv_no)!=0) goto exit;

		_cur_page_size[drv_no] = 1<<WRITE_BL_LEN(drv_no);
		if(__card_setblocklen(drv_no,_cur_page_size[drv_no])!=0) goto exit; 

		if(__card_sd_status(drv_no)!=0) goto exit;

		_ioFlag[drv_no] = INITIALIZED;
		return CARDIO_ERROR_READY;
exit:
		card_doUnmount(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	return CARDIO_ERROR_NOCARD; 
}

s32 card_preIO(s32 drv_no)
{
	s32 ret;

	if(_ioFlag[drv_no]!=INITIALIZED) {
		ret = card_initIO(drv_no);
		if(ret!=CARDIO_ERROR_READY) {
			printf("card_preIO(%d,ret = %d)\n",drv_no,ret);
			return ret;
		}
	}
	return CARDIO_ERROR_READY;
}

s32 card_readCID(s32 drv_no)
{
	s32 ret;

	if(drv_no<EXI_CHANNEL_0 || drv_no>=EXI_CHANNEL_2) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("card_readCID(%d)\n",drv_no);
#endif
	ret = card_preIO(drv_no);
	if(ret!=0) return ret;
	
	return __card_readcid(drv_no);
}

s32 card_readCSD(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("card_readCSD(%d)\n",drv_no);
#endif
	ret = card_preIO(drv_no);
	if(ret!=0) return ret;

	return __card_readcsd(drv_no);
}

s32 card_readStatus(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("card_readCSD(%d)\n",drv_no);
#endif
	ret = card_preIO(drv_no);
	if(ret!=0) return ret;

	return __card_sd_status(drv_no);
}

s32 card_readBlock(s32 drv_no,u32 block_no,u32 offset,u8 *buf,u32 len) 
{
	s32 ret;
	u8 *ptr;
	u8 arg[4];
	u32 sect_start;
	u32 sects_per_block;
	u32 blocks,read_len;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("card_readBlock(%d,%d,%d,%p,%d)\n",drv_no,block_no,offset,buf,len);
#endif
	ret = card_preIO(drv_no);
	if(ret!=0) return ret;

	read_len = (1<<READ_BL_LEN(drv_no));
	sects_per_block = (1<<(C_SIZE_MULT(drv_no)+2));

	sect_start = block_no*sects_per_block;
	sect_start += (offset/read_len);
#ifdef _CARDIO_DEBUG
	printf("sect_start = %d\n",sect_start);
#endif
	if(len<=read_len)
		return card_readSector(drv_no,sect_start,buf,len);
	
	if(read_len!=_cur_page_size[drv_no]) {
		_cur_page_size[drv_no] = read_len;
		if((ret=__card_setblocklen(drv_no,_cur_page_size[drv_no]))!=0) return ret;
	}
#ifdef _CARDIO_DEBUG
	printf("_cur_page_size[drv_no] = %d\n",_cur_page_size[drv_no]);
#endif
	arg[0] = (sect_start>>15)&0xff;
	arg[1] = (sect_start>>7)&0xff;
	arg[2] = (sect_start<<1)&0xff;
	arg[3] = (sect_start<<9)&0xff;

	if((ret=__card_sendcmd(drv_no,0x12,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))==0) {
		ptr = buf;
		for(blocks=0;blocks<(len/read_len);++blocks) {
			if((ret=__card_dataread(drv_no,ptr,_cur_page_size[drv_no]))!=0) break;
			ptr += _cur_page_size[drv_no];
		}
		if((ret=__card_datareadfinal(drv_no,ptr,_cur_page_size[drv_no]))!=0) return ret;
		ret = __card_stopresponse(drv_no);
	}
	return ret;
}

s32 card_readSector(s32 drv_no,u32 sector_no,u8 *buf,u32 len)
{
	s32 ret;
	u8 arg[4];
	u32 read_len;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ret = card_preIO(drv_no);
	if(ret!=0) return ret;

	read_len = (1<<READ_BL_LEN(drv_no));
	if(len<1 || len>read_len) return CARDIO_ERROR_INTERNAL;
#ifdef _CARDIO_DEBUG
	printf("card_readSector(%d,%d,%d(%d),%d)\n",drv_no,sector_no,len,read_len,_cur_page_size[drv_no]);
#endif
	arg[0] = (sector_no>>15)&0xff;
	arg[1] = (sector_no>>7)&0xff;
	arg[2] = (sector_no<<1)&0xff;
	arg[3] = (sector_no<<9)&0xff;

	if(len!=_cur_page_size[drv_no]) {
		_cur_page_size[drv_no] = len;
		if((ret=__card_setblocklen(drv_no,_cur_page_size[drv_no]))!=0) return ret;
	}
	if((ret=__card_sendcmd(drv_no,0x11,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))!=0) return ret;

	ret = __card_dataread(drv_no,buf,_cur_page_size[drv_no]);
	return ret;
}

s32 card_updateBlock(s32 drv_no,u32 block_no,u32 offset,const void *buf,u32 len)
{
	s32 ret;
	u8 *ptr;
	u8 arg[4];
	u32 sect_start,i;
	u32 sects_per_block;
	u32 blocks,write_len;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
#ifdef _CARDIO_DEBUG
	printf("card_updateBlock(%d,%d,%d,%p,%d)\n",drv_no,block_no,offset,buf,len);
#endif
	ret = card_preIO(drv_no);
	if(ret!=0) return ret;

	write_len = (1<<WRITE_BL_LEN(drv_no));
	if(len<write_len) return CARDIO_ERROR_INTERNAL;

	sects_per_block = (1<<(C_SIZE_MULT(drv_no)+2));

	sect_start = block_no*sects_per_block;
	sect_start += (offset/write_len);
	blocks = len/write_len;
	
	if(write_len!=_cur_page_size[drv_no]) {
		_cur_page_size[drv_no] = write_len;
		if((ret=__card_setblocklen(drv_no,_cur_page_size[drv_no]))!=0) return ret;
	}

	arg[0] = 0;
	arg[1] = (blocks>>24)&0x7f;
	arg[2] = (blocks>>16)&0xff;
	arg[3] = blocks&0xff;

	if((ret=__card_sendappcmd(drv_no))!=0) return ret;
	if((ret=__card_sendcmd(drv_no,0x17,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))!=0) return ret;

	arg[0] = (sect_start>>15)&0xff;
	arg[1] = (sect_start>>7)&0xff;
	arg[2] = (sect_start<<1)&0xff;
	arg[3] = (sect_start<<9)&0xff;
	if(blocks>1) {
		if((ret=__card_sendcmd(drv_no,0x19,arg))!=0) return ret;
		if((ret=__card_response1(drv_no))==0) {
			ptr = (u8*)buf;
			for(i=0;i<blocks;i++) {
				if((ret=__card_multidatawrite(drv_no,ptr,_cur_page_size[drv_no]))!=0) break;
				ptr += _cur_page_size[drv_no];
			}
			if((ret=__card_multiwritestop(drv_no))!=0) return ret;
			ret = __card_sendcmd(drv_no,0x0d,NULL);
			ret |= __card_response2(drv_no);
		}
		return ret;
	}
	return card_updateSector(drv_no,sect_start,buf,len);
	
}

s32 card_updateSector(s32 drv_no,u32 sector_no,const void *buf,u32 len)
{
	s32 ret;
	u8 arg[4];
	u32 write_len;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ret = card_preIO(drv_no);
	if(ret!=0) return ret;

	write_len = (1<<WRITE_BL_LEN(drv_no));
	if(len<write_len) return CARDIO_ERROR_INTERNAL;
#ifdef _CARDIO_DEBUG
	printf("card_updateSector(%d,%d,%d(%d),%d)\n",drv_no,sector_no,len,write_len,_cur_page_size[drv_no]);
#endif
	arg[0] = (sector_no>>15)&0xff;
	arg[1] = (sector_no>>7)&0xff;
	arg[2] = (sector_no<<1)&0xff;
	arg[3] = (sector_no<<9)&0xff;

	if(write_len!=_cur_page_size[drv_no]) {
		_cur_page_size[drv_no] = write_len;
		if((ret=__card_setblocklen(drv_no,_cur_page_size[drv_no]))!=0) return ret;
	}
	if((ret=__card_sendcmd(drv_no,0x18,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))!=0) return ret;

	ret = __card_datawrite(drv_no,(void*)buf,_cur_page_size[drv_no]);
	return ret;
}

s32 card_doUnmount(s32 drv_no)
{
	u32 level;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	
	_CPU_ISR_Disable(level);
	if(_card_inserted[drv_no]==TRUE) {
		_card_inserted[drv_no] = FALSE;
		_ioFlag[drv_no] = NOT_INITIALIZED;
		EXI_Detach(drv_no);
	}
	_CPU_ISR_Restore(level);
	return CARDIO_ERROR_READY;
}
