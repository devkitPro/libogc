#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asm.h"
#include "processor.h"
#include "exi.h"
#include "lwp.h"
#include "system.h"
#include "ogcsys.h"
#include "sdcard.h"

//#define _SDCARD_DEBUG

#define SDCARD_OP_TIMEDOUT			0x4000
#define SDCARD_OP_INITFAILED		0x8000

typedef struct _csd_register {
	u8 csd_structure;
	u8 taac;
	u8 nasc;
	u8 tran_speed;
	u16 ccc;
	u8 read_bl_len;
	u8 read_bl_part;
	u8 write_bl_misal;
	u8 read_bl_misal;
	u8 dsr_imp;
	u16 c_size;
	u8 vdd_r_curr_min;
	u8 vdd_r_curr_max;
	u8 vdd_w_curr_min;
	u8 vdd_w_curr_max;
	u8 c_size_mult;
	u8 erase_bl_en;
	u8 sector_size;
	u8 wp_grp_size;
	u8 wp_grp_en;
	u8 r2w_fact;
	u8 write_bl_len;
	u8 write_bl_part;
	u8 file_fmt_grp;
	u8 copy;
	u8 perm_write_prot;
	u8 tmp_write_prot;
	u8 file_fmt;
} csd_register;

typedef struct _cid_register {
	u8 mid;
	u8 oid[2];
	u8 pnm[5];
	u8 prv;
	u32 psn;
	u16 mdt;
} cid_register;

typedef struct _sdcard_cntrl {
	u8 cmd[5];
	u32 cmd_len;
	u16 response;
	u32 cidh[4];
	cid_register cid;
	u32 csdh[4];
	csd_register csd;
	u32 statush[16];
	u32 attached;
	u32 mount_step;
	s32 result;
	u32 block_len;
	u32 err_status;
	lwpq_t alarm_queue;
	sysalarm timeout_svc;

	SDCCallback api_cb;
	SDCCallback ext_cb;
	SDCCallback exi_cb;
} sdcard_block;

static u8 clr_flag;
static u8 wp_flag;
static u32 sdcard_inited = 0;
static sdcard_block sdcard_map[2];

extern void __exi_init();
extern unsigned long gettick();
extern long long gettime();
extern u32 diff_msec(long long start,long long end);
extern u32 diff_usec(long long start,long long end);

//#ifdef _SDCARD_DEBUG
static void __print_csdregister(s32 chn)
{
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];

	printf("csd: %08x %08x %08x %08x\n",card->csdh[0],card->csdh[1],card->csdh[2],card->csdh[3]);
	printf("csd_structure: %02x\n",card->csd.csd_structure);
	printf("taac: %02x\n",card->csd.taac);
	printf("nasc: %02x\n",card->csd.nasc);
	printf("tran_speed: %02x\n",card->csd.tran_speed);
	printf("ccc: %04x\n",card->csd.ccc);
	printf("read_bl_len: %02x\n",card->csd.read_bl_len);
	printf("read_bl_part: %02x\n",card->csd.read_bl_part);
	printf("write_bl_misal: %02x\n",card->csd.write_bl_misal);
	printf("read_bl_misal: %02x\n",card->csd.read_bl_misal);
	printf("dsr_imp: %02x\n",card->csd.dsr_imp);
	printf("c_size: %04x\n",card->csd.c_size);
	printf("vdd_r_curr_min: %02x\n",card->csd.vdd_r_curr_min);
	printf("vdd_r_curr_max: %02x\n",card->csd.vdd_r_curr_max);
	printf("vdd_w_curr_min: %02x\n",card->csd.vdd_w_curr_min);
	printf("vdd_w_curr_max: %02x\n",card->csd.vdd_w_curr_max);
	printf("c_size_mult: %02x\n",card->csd.c_size_mult);
	printf("erase_bl_en: %02x\n",card->csd.erase_bl_en);
	printf("sector_size: %02x\n",card->csd.sector_size);
	printf("wp_grp_size: %02x\n",card->csd.wp_grp_size);
	printf("wp_grp_en: %02x\n",card->csd.wp_grp_en);
	printf("r2w_fact: %02x\n",card->csd.r2w_fact);
	printf("write_bl_len: %02x\n",card->csd.write_bl_len);
	printf("write_bl_part: %02x\n",card->csd.write_bl_part);
	printf("file_fmt_grp: %02x\n",card->csd.file_fmt_grp);
	printf("copy: %02x\n",card->csd.copy);
	printf("perm_write_prot: %02x\n",card->csd.perm_write_prot);
	printf("tmp_write_prot: %02x\n",card->csd.tmp_write_prot);
	printf("file_fmt: %02x\n",card->csd.file_fmt);
}

static void __print_cidregister(s32 chn)
{
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];

	printf("cid: %08x %08x %08x %08x\n",card->cidh[0],card->cidh[1],card->cidh[2],card->cidh[3]);
	printf("mid: %02x\n",card->cid.mid);
	printf("oid: %c%c\n",card->cid.oid[0],card->cid.oid[1]);
	printf("pnm: %c%c%c%c%c\n",card->cid.pnm[0],card->cid.pnm[1],card->cid.pnm[2],card->cid.pnm[3],card->cid.pnm[4]);
	printf("prv: %d.%d\n",((card->cid.prv>>4)&0x0f),(card->cid.prv&0x0f));
	printf("psn: %08x\n",card->cid.psn);
	printf("mdt: %04x\n",card->cid.mdt);
}
//#endif

static void __fill_csdregister(s32 chn)
{
	sdcard_block *card = NULL;
	u8 *csd;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];

	csd = (u8*)card->csdh;
	card->csd.csd_structure = (csd[0]>>6)&0x3;
	card->csd.taac = csd[1];
	card->csd.nasc = csd[2];
	card->csd.tran_speed = csd[3];
	card->csd.ccc = ((*(u16*)&(csd[4])>>4)&0x0fff);
	card->csd.read_bl_len = csd[5]&0x0f;
	card->csd.read_bl_part = (csd[6]>>7)&0x01;
	card->csd.write_bl_misal = (csd[6]>>6)&0x01;
	card->csd.read_bl_misal = (csd[6]>>5)&0x01;
	card->csd.dsr_imp = (csd[6]>>4)&0x01;
	card->csd.c_size = (u32)(((csd[6]&0x03)<<10)|(csd[7]<<2)|((csd[8]>>6)&0x03));
	card->csd.vdd_r_curr_min = (csd[8]>>3)&0x07;
	card->csd.vdd_r_curr_max = csd[8]&0x07;
	card->csd.vdd_w_curr_min = (csd[9]>>5)&0x07;
	card->csd.vdd_w_curr_max = (csd[9]>>2)&0x07;
	card->csd.c_size_mult = ((csd[9]&0x03)<<1)|((csd[10]>>7)&0x01);
	card->csd.erase_bl_en = (csd[10]>>6)&0x01;
	card->csd.sector_size = ((csd[10]&0x3f)<<1)|((csd[11]>>1)&0x01);
	card->csd.wp_grp_size = csd[11]&0x7f;
	card->csd.wp_grp_en = (csd[12]>>7)&0x01;
	card->csd.r2w_fact = (csd[12]>>2)&0x07;
	card->csd.write_bl_len = ((csd[12]&0x03)<<2)|((csd[13]>>6)&0x03);
	card->csd.write_bl_part = (csd[13]>>5)&0x01;
	card->csd.file_fmt_grp = (csd[14]>>7)&0x01;
	card->csd.copy = (csd[14]>>6)&0x01;
	card->csd.perm_write_prot = (csd[14]>>5)&0x01;
	card->csd.tmp_write_prot = (csd[14]>>4)&0x01;
	card->csd.file_fmt = (csd[14]>>2)&0x03;
}

static void __fill_cidregister(s32 chn)
{
	sdcard_block *card = NULL;
	u8 *cid;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];

	cid = (u8*)card->cidh;
	card->cid.mid = cid[0];
	card->cid.oid[0] = cid[1];
	card->cid.oid[1] = cid[2];
	card->cid.prv = cid[8];
	card->cid.psn = *(u32*)&(cid[9]);
	card->cid.mdt = (*(u16*)&(cid[13])&0x0fff);
	memcpy(card->cid.pnm,cid+3,5);
}

static void __timeout_handler(sysalarm *alarm)
{
	u32 chn;
	sdcard_block *card = NULL;
	
	chn = 0;
	while(chn<2) {
		card = &sdcard_map[chn];
		if((&card->timeout_svc)==alarm) break;
		chn++;
	}
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;

	LWP_WakeThread(card->alarm_queue);
}

static __inline__ u32 __check_response(s32 chn,u8 *res)
{
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	if(*res&0x40) card->err_status |= 0x1000;
	if(*res&0x20) card->err_status |= 0x0100;
	if(*res&0x08) card->err_status |= 0x0002;
	if(*res&0x04) card->err_status |= 0x0001;

	return card->err_status;
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

static void __sdcard_defaultapicallback(s32 chn,s32 result)
{
	return;
}

static u32 __sdcard_exthandler(u32 chn,u32 dev)
{
	SDCCallback cb;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	if(card->attached) {
		card->attached = 0;
		EXI_RegisterEXICallback(chn,NULL);
		SYS_CancelAlarm(&card->timeout_svc);
		
		cb = card->exi_cb;
		if(cb) {
			card->exi_cb = NULL;
			cb(chn,SDCARD_ERROR_NOCARD);
		}

		cb = card->ext_cb;
		if(cb) {
			card->ext_cb = NULL;
			cb(chn,SDCARD_ERROR_NOCARD);
		}
		printf("card was detached.\n");
	}
	return 1;
}

static s32 __sdcard_getcntrlblock(s32 chn,sdcard_block **card)
{
	s32 ret;
	u32 level;
	sdcard_block *rcard = NULL;
	
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_FATALERROR;

	_CPU_ISR_Disable(level);
	rcard = &sdcard_map[chn];
	if(!rcard->attached) {
		_CPU_ISR_Restore(level);
		return SDCARD_ERROR_NOCARD;
	}

	ret = SDCARD_ERROR_BUSY;
	if(rcard->result!=SDCARD_ERROR_BUSY) {
		rcard->result = SDCARD_ERROR_BUSY;
		rcard->api_cb = NULL;
		*card = rcard;
		ret = SDCARD_ERROR_READY;
	}
	_CPU_ISR_Restore(level);
	return ret;
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
#ifdef _SDCARD_DEBUG
	printf("sd command: %02x %02x %02x %02x %02x %02x\n",((u8*)buf)[0],((u8*)buf)[1],((u8*)buf)[2],((u8*)buf)[3],((u8*)buf)[4],crc);
#endif
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
#ifdef _SDCARD_DEBUG
	printf("sd command: %02x %02x %02x %02x %02x %02x\n",((u8*)buf)[0],((u8*)buf)[1],((u8*)buf)[2],((u8*)buf)[3],((u8*)buf)[4],crc);
#endif
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
	s32 startT,ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	ret = SDCARD_ERROR_READY;
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
#ifdef _SDCARD_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
#ifdef _SDCARD_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(!(*ptr&0x80)) break;
		if(__sdcard_checktimeout(startT,500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
#ifdef _SDCARD_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr&0x80) ret = SDCARD_OP_TIMEDOUT;
			break;
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
	usleep(40);
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return ret;
}

static s32 __sdcard_stopreadresponse(s32 chn,void *buf,s32 len)
{
	u8 *ptr;
	s32 startT,ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	ptr = buf;
	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	ret = SDCARD_ERROR_READY;
	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
#ifdef _SDCARD_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
#ifdef _SDCARD_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
#ifdef _SDCARD_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(!(*ptr&0x80)) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
#ifdef _SDCARD_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr&0x80) ret = SDCARD_OP_TIMEDOUT;
			break;
		}
	}

	while(*ptr!=0xff) {
		ptr[1] = ptr[0];
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
#ifdef _SDCARD_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(*ptr==0xff) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
#ifdef _SDCARD_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr!=0xff) ret = SDCARD_OP_TIMEDOUT;
			break;
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
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return ret;
}

static s32 __sdcard_dataread(s32 chn,void *buf,u32 len)
{
	u8 *ptr;
	u32 cnt;
	u8 res[2];
	u16 crc,crc_org;
	s32 startT,ret;
	struct timespec tb;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	ret = SDCARD_ERROR_READY;
	ptr = buf;
	for(cnt=0;cnt<len;cnt++) ptr[cnt] = clr_flag;
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
#ifdef _SDCARD_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
	
	startT = gettick();
	while(*ptr!=0xfe) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
#ifdef _SDCARD_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(*ptr==0xfe) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
#ifdef _SDCARD_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr!=0xfe) ret = SDCARD_OP_TIMEDOUT;
			break;
		}
	}

	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,len,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}

	/* setalarm, wait */
	tb.tv_sec = 0;
	tb.tv_nsec = 40*TB_NSPERMS;
	SYS_SetAlarm(&card->timeout_svc,&tb,__timeout_handler);
	LWP_SleepThread(card->alarm_queue);

	res[0] = res[1] = clr_flag;
	if(EXI_ImmEx(chn,res,2,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
#ifdef _SDCARD_DEBUG
	printf("sd response: %04x\n",*(u16*)res);
#endif
	crc_org = ((res[0]<<8)&0xff00)|(res[1]&0xff);

	EXI_Deselect(chn);
	EXI_Unlock(chn);

	crc = __make_crc16(buf,len);
	if(crc==crc_org) {
#ifdef _SDCARD_DEBUG
		printf("crc ok: %04x : %04x\n",crc_org,crc);
#endif
		return SDCARD_ERROR_READY;
	}
	
	return SDCARD_ERROR_FATALERROR;
}

static s32 __sdcard_datareadfinal(s32 chn,void *buf,u32 len)
{
	s32 startT,ret;
	u32 cnt;
	u8 *ptr,cmd[6];
	u16 crc_org,crc;
	struct timespec tb;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==0) return SDCARD_ERROR_FATALERROR;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return SDCARD_ERROR_NOCARD;
	}

	ret = SDCARD_ERROR_READY;
	ptr = buf;
	for(cnt=0;cnt<len;cnt++) ptr[cnt] = clr_flag;
	
	if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
#ifdef _SDCARD_DEBUG
	printf("sd response: %02x\n",((u8*)buf)[0]);
#endif

	startT = gettick();
	while(*ptr!=0xfe) {
		*ptr = clr_flag;
		if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return SDCARD_ERROR_IOERROR;
		}
#ifdef _SDCARD_DEBUG
		printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
		if(*ptr==0xfe) break;
		if(__sdcard_checktimeout(startT,1500)!=0) {
			*ptr = clr_flag;
			if(EXI_ImmEx(chn,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(chn);
				EXI_Unlock(chn);
				return SDCARD_ERROR_IOERROR;
			}
#ifdef _SDCARD_DEBUG
			printf("sd response: %02x\n",((u8*)buf)[0]);
#endif
			if(*ptr!=0xfe) ret = SDCARD_OP_TIMEDOUT;
			break;
		}
	}

	*ptr = clr_flag;
	if(EXI_ImmEx(chn,ptr,(len-4),EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}

	memset(cmd,0,6);
	cmd[0] = 0x4C;
	cmd[5] = __make_crc7(cmd,5);
	cmd[5] |= 0x0001;
	if(wp_flag) {
		for(cnt=0;cnt<6;cnt++) cmd[cnt] ^= -1;
	}
	if(EXI_ImmEx(chn,cmd,6,EXI_READWRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return SDCARD_ERROR_IOERROR;
	}
	ptr[len-4] = cmd[0];
	ptr[len-3] = cmd[1];
	ptr[len-2] = cmd[2];
	ptr[len-1] = cmd[3];
	crc_org = ((cmd[4]<<8)&0xff00)|(cmd[5]&0xff);
	
	/* setalarm, wait */
	tb.tv_sec = 0;
	tb.tv_nsec = 40*TB_NSPERMS;
	SYS_SetAlarm(&card->timeout_svc,&tb,__timeout_handler);
	LWP_SleepThread(card->alarm_queue);

	EXI_Deselect(chn);
	EXI_Unlock(chn);

	crc = __make_crc16(buf,len);
	if(crc==crc_org) {
#ifdef _SDCARD_DEBUG
		printf("crc ok: %04x : %04x\n",crc_org,crc);
#endif
		return ret;
	}
	
	return ret;
}

static s32 __sdcard_response1(s32 chn)
{
	s32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
	
	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	return __check_response(chn,(u8*)&card->response);
}

static s32 __sdcard_response2(s32 chn)
{
	u32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
	
	if((ret=__sdcard_readresponse(chn,&card->response,2))!=0) return ret;
	if(!(card->response&0x7c) && !(card->response&0x9e00)) return SDCARD_ERROR_READY;
	return SDCARD_ERROR_FATALERROR;
}

static s32 __sdcard_sendopcond(s32 chn)
{
	s32 ret;
	s32 startT;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("__sdcard_sendopcond(%d)\n",chn);
#endif
	ret = 0;
	startT = gettick();
	do {
		memset(card->cmd,0,5);
		card->cmd[0] = 0x37;
		card->cmd_len = 5;
		if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
			printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
			return ret;
		}
		if((ret=__sdcard_response1(chn))!=0) return ret;

		memset(card->cmd,0,5);
		card->cmd[0] = 0x29;
		card->cmd_len = 5;
		if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
			printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
			return ret;
		}
		if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
		ret = __check_response(chn,(u8*)&card->response);

		if(ret!=0) return ret;
		if(!(((u8*)&card->response)[0]&0x01)) return SDCARD_ERROR_READY;
		ret = __sdcard_checktimeout(startT,1500);
	} while(ret==0);

	memset(card->cmd,0,5);
	card->cmd[0] = 0x37;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__sdcard_response1(chn))!=0) return ret;
	
	memset(card->cmd,0,5);
	card->cmd[0] = 0x29;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_sendopcond(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);

	if(ret!=0) return ret;
	if((((u8*)&card->response)[0]&0x01)) ret |= SDCARD_OP_INITFAILED;

	return ret;
}

static s32 __sdcard_sendappcmd(s32 chn)
{
	u32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	memset(card->cmd,0,5);
	card->cmd[0] = 0x37;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0){
#ifdef _SDCARD_DEBUG
		printf("__sdcard_sendappcmd(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);

	return ret;
}

static s32 __sdcard_sendcmd(s32 chn,u8 cmd,u32 arg)
{
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	memset(card->cmd,0,5);
	card->cmd[0] = cmd;
	card->cmd[1] = (arg>>24)&0xff;
	card->cmd[2] = (arg>>16)&0xff;
	card->cmd[3] = (arg>>8)&0xff;
	card->cmd[4] = arg&0xff;
	card->cmd_len = 5;
	return __sdcard_writecmd(chn,card->cmd,card->cmd_len);
}

static s32 __sdcard_sendcsd(s32 chn)
{
	s32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("__sdcard_sendcsd(%d)\n",chn);
#endif
	ret = 0;
	
	memset(card->cmd,0,5);
	card->cmd[0] = 0x09;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_sendcsd(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);
	if(ret==0) {
		if((ret=__sdcard_dataread(chn,card->csdh,16))!=0) return ret;
		__fill_csdregister(chn);
	}
	return ret;
}

static s32 __sdcard_sendcid(s32 chn)
{
	s32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("__sdcard_sendcid(%d)\n",chn);
#endif
	ret = 0;
	
	memset(card->cmd,0,5);
	card->cmd[0] = 0x0A;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_sendcid(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);
	if(ret==0) {
		if((ret=__sdcard_dataread(chn,card->cidh,16))!=0) return ret;
		__fill_cidregister(chn);
	}
	return ret;
}

static s32 __sdcard_setblocklen(s32 chn,u32 block_len)
{
	s32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("__sdcard_setblocklen(%d,%d)\n",chn,block_len);
#endif
	memset(card->cmd,0,5);
	card->cmd[0] = 0x10;
	card->cmd[1] = (block_len>>24)&0xff;
	card->cmd[2] = (block_len>>16)&0xff;
	card->cmd[3] = (block_len>>8)&0xff;
	card->cmd[4] = block_len&0xff;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_setblocklen(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}
	if((ret=__sdcard_readresponse(chn,&card->response,1))<0) return ret;
	ret = __check_response(chn,(u8*)&card->response);
	
	return ret;
}

static s32 __sdcard_softreset(s32 chn)
{
	s32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("__sdcard_softreset(%d)\n",chn);
#endif
	ret = 0;
	memset(card->cmd,0,5);
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd0(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_softreset(%d): sd write cmd0 failed.\n",ret);
#endif
		return ret;
	}
	
	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);

	memset(card->cmd,0,5);
	card->cmd[0] = 0x0C;
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) goto exit;
	
	if((ret=__sdcard_stopreadresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);

exit:
	memset(card->cmd,0,5);
	card->cmd_len = 5;
	if((ret=__sdcard_writecmd(chn,card->cmd,card->cmd_len))!=0) {
#ifdef _SDCARD_DEBUG
		printf("__sdcard_softreset(%d): sd write cmd failed.\n",ret);
#endif
		return ret;
	}

	if((ret=__sdcard_readresponse(chn,&card->response,1))!=0) return ret;
	ret = __check_response(chn,(u8*)&card->response);

	return ret;
}

static s32 __sdcard_sd_status(s32 chn)
{
	s32 ret;
	u32 bl_tmp;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	bl_tmp = card->block_len;
	card->block_len = 64;
	if((ret=__sdcard_setblocklen(chn,card->block_len))!=0) return ret;
	if((ret=__sdcard_sendappcmd(chn))!=0) return ret;
	if((ret=__sdcard_sendcmd(chn,0x0d,0))!=0) return ret;
	if((ret=__sdcard_response2(chn))!=0) return ret;
	ret = __sdcard_dataread(chn,card->statush,card->block_len);

	card->block_len = bl_tmp;
	ret |= __sdcard_setblocklen(chn,card->block_len);
	
	return ret;
}

static s32 __sdcard_readD(s32 chn,void *buffer,u32 address,u32 blocks)
{
	u8 *ptr;
	s32 ret,cnt;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];

	if((ret=__sdcard_sendcmd(chn,0x12,address))!=0) return ret;
	if((ret=__sdcard_response1(chn))!=0) return ret;

	cnt = 0;
	ptr = buffer;
	while(cnt<(blocks-1)) {
		if((ret=__sdcard_dataread(chn,ptr,card->block_len))!=0) break;
		ptr += card->block_len;
		cnt++;
	}
	return SDCARD_ERROR_READY;
}

static void __sdcard_dounmount(s32 chn,s32 result)
{
	u32 level;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &sdcard_map[chn];
	
	_CPU_ISR_Disable(level);
	if(card->attached) {
		card->attached = 0;
		EXI_RegisterEXICallback(chn,NULL);
		EXI_Detach(chn);
		SYS_CancelAlarm(&card->timeout_svc);
	}
	_CPU_ISR_Restore(level);
}

s32 SDCARD_Reset(s32 chn)
{
	u32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_NOCARD;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("SDCARD_Reset(%d)\n",chn);
#endif
	wp_flag = 0;
	if(__sdcard_softreset(chn)!=0) {
		wp_flag = 1;
		if(__sdcard_softreset(chn)!=0) return -1;
	}

	if(__sdcard_sendopcond(chn)!=0) return -1;
	if(__sdcard_sendcsd(chn)!=0) return -1;
	if(__sdcard_sendcid(chn)!=0) return -1;
//#ifdef _SDCARD_DEBUG
	__print_csdregister(chn);
	__print_cidregister(chn);
//#endif

	card->block_len = 512;
	ret = __sdcard_setblocklen(chn,card->block_len);

	ret = __sdcard_sd_status(chn);
	return ret;
}

s32 SDCARD_Mount(s32 chn,SDCCallback detach_cb)
{
	s32 ret;
	sdcard_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_FATALERROR;
	card = &sdcard_map[chn];
#ifdef _SDCARD_DEBUG
	printf("SDCARD_Mount(): mounting card.\n");
#endif
	if(card->result==SDCARD_ERROR_BUSY) return SDCARD_ERROR_BUSY;

	while((ret=EXI_ProbeEx(chn))==0);
	if(ret!=1) return SDCARD_ERROR_NOCARD;
	

	ret = SDCARD_ERROR_BUSY;
	if(card->attached || !(EXI_GetState(chn)&EXI_FLAG_ATTACH)) {
		card->result = SDCARD_ERROR_BUSY;
		card->ext_cb = detach_cb;
		card->api_cb = __sdcard_defaultapicallback;
		card->exi_cb = NULL;
		
		if(!card->attached) {
			if(EXI_Attach(chn,__sdcard_exthandler)==0) {
				card->result = SDCARD_ERROR_NOCARD;
				return SDCARD_ERROR_NOCARD;
			}
		}
		card->attached = 1;
		card->mount_step = 0;
		ret = SDCARD_Reset(chn);
	}
	
	if(ret) ret = SDCARD_ERROR_NOCARD;
	return ret;
}

s32 SDCARD_Unmount(s32 chn)
{
	s32 ret;
	sdcard_block *card;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return SDCARD_ERROR_FATALERROR;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_Unmount(%d)\n",chn);
#endif
	if((ret=__sdcard_getcntrlblock(chn,&card))<0) ret = SDCARD_ERROR_NOCARD;
	__sdcard_dounmount(chn,SDCARD_ERROR_READY);
	
	return SDCARD_ERROR_READY;
}

s32 SDCARD_Init()
{
	u32 i,level;
	
	if(sdcard_inited) return SDCARD_ERROR_BUSY;
#ifdef _SDCARD_DEBUG
	printf("SDCARD_Init()\n");
#endif
	_CPU_ISR_Disable(level);
	memset(sdcard_map,0,sizeof(sdcard_block)*2);
	for(i=0;i<2;i++) {
		sdcard_map[i].result = SDCARD_ERROR_NOCARD;
		LWP_InitQueue(&sdcard_map[i].alarm_queue);
		SYS_CreateAlarm(&sdcard_map[i].timeout_svc);
	}
	sdcard_inited = 1;
	_CPU_ISR_Restore(level);

	return SDCARD_ERROR_READY;
}
