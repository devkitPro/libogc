#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asm.h"
#include "processor.h"
#include "exi.h"
#include "sdcard.h"
#include "ogcsys.h"

typedef struct _sdcard_cntrl {
	u8 cmd[9];
	u32 cmd_len;
	u32 cmd_mode;
	u8 response[2];
	u8 cid[16];
	u8 csd[16];
	u32 attached;
	s16 result;

	SDCCallback api_cb;
	SDCCallback ext_cb;
	SDCCallback exi_cb;
} sdcard_block;

static u8 clr_flag;
static u8 wp_flag;
static sdcard_block sdcard_map[EXI_CHANNEL_MAX];

extern unsigned long gettick();
extern u32 diff_msec(long long start,long long end);

static __inline__ u32 __check_response(u8 res)
{
	u32 ret;

	ret = 0;
	if(res&0x40) ret |= 0x1000;
	if(res&0x20) ret |= 0x0100;
	if(res&0x08) ret |= 0x0002;
	if(res&0x04) ret |= 0x0001;

	return ret;
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

static __inline__ u32 __sdcard_checktimeout(u32 startT,u32 timeout)
{
	s32 endT;
	s32 msec;

	endT = gettick();
	if(startT>endT) {
		startT--;
		endT += startT;
		startT++;
	} else
		endT -= startT;

	msec = (endT/TB_TIMER_CLOCK);
	if(msec<=timeout) return 0;
	return 1;
}

static void __sdcard_defaultapicallback(s32 chn,s32 result)
{
	return;
}

static u32 __sdcard_exthandler(u32 chn,u32 dev)
{
	printf("card was detached.\n");
	return 1;
}

static s32 __sdcard_writecmd0(s32 chn,void *buf,s32 len)
{
	u8 crc,*ptr;
	u32 cnt;
	u8 dummy[128];
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	ptr = buf;

	clr_flag = 0xff;
	ptr[0] |= 0x40;
	crc = __make_crc7(buf,len);

	if(wp_flag) {
		clr_flag = 0x00;
		for(cnt=0;cnt<len;cnt++) ptr[cnt] ^= -1;
	}

	for(cnt=0;cnt<128;cnt++) dummy[cnt] = clr_flag;

	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_SelectSD(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}


	cnt = 0;
	while(cnt<20) {
		if(EXI_ImmEx(chn,dummy,128,EXI_WRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
		cnt++;
	}
	EXI_Deselect(chn);
	
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	crc |= 0x01;
	if(wp_flag) crc ^= -1;
	printf("sd command: %02x %02x %02x %02x %02x %02x\n",((u8*)buf)[0],((u8*)buf)[1],((u8*)buf)[2],((u8*)buf)[3],((u8*)buf)[4],crc);
	if(EXI_ImmEx(chn,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	if(EXI_ImmEx(chn,&crc,1,EXI_WRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return SDCARD_ERROR_READY;
}

static s32 __sdcard_writecmd(s32 chn,void *buf,s32 len)
{
	u8 crc,*ptr;
	u8 dummy[32];
	u32 cnt;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	ptr = buf;
	ptr[0] |= 0x40;
	crc = __make_crc7(buf,len);

	if(wp_flag) {
		for(cnt=0;cnt<len;cnt++) ptr[cnt] ^= -1;
	}

	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = clr_flag;
	
	if(EXI_ImmEx(chn,dummy,10,EXI_WRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	
	crc |= 0x01;
	if(wp_flag) crc ^= -1;
	printf("sd command: %02x %02x %02x %02x %02x %02x\n",((u8*)buf)[0],((u8*)buf)[1],((u8*)buf)[2],((u8*)buf)[3],((u8*)buf)[4],crc);
	if(EXI_ImmEx(chn,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	if(EXI_ImmEx(chn,&crc,1,EXI_WRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return SDCARD_ERROR_READY;
}

static s32 __sdcard_readresponse(s32 chn,void *buf,s32 len)
{
	u8 *ptr;
	s32 startT;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	ptr = buf;
	*ptr = clr_flag;
	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	printf("sd response: %02x\n",((u8*)buf)[0]);

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
		printf("sd response: %02x\n",((u8*)buf)[0]);
		if(!(*ptr&0x80)) break;
		if(__sdcard_checktimeout(startT,500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
			printf("sd response: %02x\n",((u8*)buf)[0]);
		}
	}
	if(len>1) {
		*(++ptr) = clr_flag;
		if(EXI_ImmEx(chn,ptr,len-1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
	}
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return SDCARD_ERROR_READY;
}

static s32 __sdcard_stopreadresponse(s32 chn,void *buf,s32 len)
{
	u8 *ptr;
	s32 startT;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	ptr = buf;
	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	printf("sd response: %02x\n",((u8*)buf)[0]);

	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	printf("sd response: %02x\n",((u8*)buf)[0]);

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
		printf("sd response: %02x\n",((u8*)buf)[0]);
		if(!(*ptr&0x80)) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
			printf("sd response: %02x\n",((u8*)buf)[0]);
		}
	}

	startT = gettick();
	while(*ptr!=0xff) {
		ptr[1] = ptr[0];
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
		printf("sd response: %02x\n",((u8*)buf)[0]);
		if(*ptr==0xff) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
			printf("sd response: %02x\n",((u8*)buf)[0]);
		}
	}
	ptr[0] = ptr[1];

	if(len>1) {
		*(++ptr) = clr_flag;
		if(EXI_ImmEx(chn,ptr,len-1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
	}
	printf("sd response: %02x\n",((u8*)buf)[0]);
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return SDCARD_ERROR_READY;
}

static s32 __sdcard_dataread(s32 chn,void *buf,u32 len)
{
	u8 *ptr;
	u32 cnt;
	u8 res[2];
	u16 crc,crc_org;
	s32 startT;

	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	ptr = buf;
	for(cnt=0;cnt<len;cnt++) ptr[cnt] = clr_flag;
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	printf("sd response: %02x\n",((u8*)buf)[0]);
	
	startT = gettick();
	while(*ptr!=0xfe) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
		printf("sd response: %02x\n",((u8*)buf)[0]);
		if(*ptr==0xfe) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
			printf("sd response: %02x\n",((u8*)buf)[0]);
		}
	}

	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,len,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}

	/* setalarm, wait */
	
	res[0] = res[1] = clr_flag;
	if(EXI_ImmEx(chn,res,2,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	printf("sd response: %04x\n",*(u16*)res);
	crc_org = ((res[0]<<8)&0xff00)|(res[1]&0xff);

	EXI_Deselect(chn);
	EXI_Unlock(chn);

	crc = __make_crc16(buf,len);
	if(crc==crc_org) return 0;
	
	return -1;
}

static s32 __sdcard_response1(s32 chn)
{
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
	
	__sdcard_readresponse(chn,card->response,1);
	return __check_response(card->response[0]);
}

static s32 __sdcard_response2(s32 chn)
{
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
	
	__sdcard_readresponse(chn,card->response,2);
	return __check_response(card->response[0]);
}

static s32 __sdcard_sendopcond(s32 chn)
{
	u8 cmd[5],res;
	s32 ret,err;
	s32 startT;

	err = 0;
	ret = 0;
	startT = gettick();
	do {
		memset(cmd,0,5);
		cmd[0] = 0x37;
		if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
			printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
			return ret;
		}
		if((err=__sdcard_response1(chn))!=0) return err;

		memset(cmd,0,5);
		cmd[0] = 0x29;
		if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
			printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
			return ret;
		}
		if((ret=__sdcard_readresponse(chn,&res,1))<0) return ret;
		err |= __check_response(res);

		if(err!=0) return err;
		if(!(res&0x01)) return err;
		ret = __sdcard_checktimeout(startT,1500);
	} while(ret==0);

	memset(cmd,0,5);
	cmd[0] = 0x37;
	if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
		printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
		return ret;
	}
	if((err=__sdcard_response1(chn))!=0) return err;
	
	memset(cmd,0,5);
	cmd[0] = 0x29;
	if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
		printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&res,1))<0) return ret;
	err |= __check_response(res);

	if(err!=0) return err;
	if(res&0x01) err |= 0x8000;

	return err;
}

static s32 __sdcard_sendcsd(s32 chn)
{
	u8 res,cmd[5];
	s32 ret,err;
	u8 csd[20];

	ret = 0;
	err = 0;
	
	memset(cmd,0,5);
	cmd[0] = 0x09;
	if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
		printf("__sdcard_sendcsd(%d): sd write cmd failed.\n",ret);
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&res,1))<0) return ret;
	err |= __check_response(res);
	if(err==0) {
		__sdcard_dataread(chn,csd,16);
	}
	return 0;
}

static s32 __sdcard_sendcid(s32 chn)
{
	u8 res,cmd[5];
	s32 ret,err;
	u8 csd[20];

	ret = 0;
	err = 0;
	
	memset(cmd,0,5);
	cmd[0] = 0x0A;
	if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
		printf("__sdcard_sendcsd(%d): sd write cmd failed.\n",ret);
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&res,1))<0) return ret;
	err |= __check_response(res);
	if(err==0) {
		__sdcard_dataread(chn,csd,16);
	}
	return 0;
}

static s32 __sdcard_softreset(s32 chn)
{
	u8 res;
	u8 cmd[5];
	s32 ret;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;

	ret = 0;
	memset(cmd,0,5);
	if((ret=__sdcard_writecmd0(chn,cmd,5))<0) {
		printf("SDCARD_Reset(%d): sd write cmd0 failed.\n",ret);
		return ret;
	}
	__sdcard_readresponse(chn,&res,1);
	ret |= __check_response(res);

	memset(cmd,0,5);
	cmd[0] = 0x0C;
	if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
		printf("SDCARD_Reset(%d): sd write cmd failed.\n",ret);
		return ret;
	}
	__sdcard_stopreadresponse(chn,&res,1);
	ret |= __check_response(res);

	memset(cmd,0,5);
	if((ret=__sdcard_writecmd(chn,cmd,5))<0) {
		printf("SDCARD_Reset(%d): sd write cmd failed.\n",ret);
		return ret;
	}
	__sdcard_readresponse(chn,&res,1);
	ret |= __check_response(res);

	return ret;
}

s32 SDCARD_Reset(s32 chn)
{
	if(EXI_Attach(chn,__sdcard_exthandler)==0) return -1;

	wp_flag = 0;
	if(__sdcard_softreset(chn)!=0) {
		wp_flag = 1;
		if(__sdcard_softreset(chn)!=0) return -1;
	}
	__sdcard_sendopcond(chn);
	__sdcard_sendcsd(chn);
	__sdcard_sendcid(chn);
	
	return 0;
}

s32 SDCARD_Mount(s32 chn,SDCCallback detach_cb)
{
	u32 level;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_FATALERROR;
	card = &sdcard_map[chn];

	_CPU_ISR_Disable(level);
	if(card->result==SDCARD_ERROR_BUSY) {
		_CPU_ISR_Restore(level);
		return SDCARD_ERROR_BUSY;
	}

	if(card->attached || !(EXI_GetState(chn)&EXI_FLAG_ATTACH)) {
		card->result = SDCARD_ERROR_BUSY;
		card->ext_cb = detach_cb;
		card->api_cb = __sdcard_defaultapicallback;
		card->exi_cb = NULL;
		
		if(!card->attached) {
			if(EXI_Attach(chn,__sdcard_exthandler)==0) {
				card->result = SDCARD_ERROR_NOCARD;
				_CPU_ISR_Restore(level);
				return SDCARD_ERROR_NOCARD;
			}
		}
		card->attached = 1;
	}
	return -1;
}

void SDCARD_Init()
{
	memset(sdcard_map,0,EXI_CHANNEL_MAX*sizeof(sdcard_block));
}
