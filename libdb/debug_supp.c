#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gccore.h>
#include <tuxedo/thread.h>

#include "debug_supp.h"

extern KThread* s_firstThread;

extern const u8 hexchars[];

extern u8 __text_start[],__data_start[],__bss_start[];
extern u8 __text_fstart[],__data_fstart[],__bss_fstart[];

s32 hstr2nibble(const char *buf,s32 *nibble)
{
	s32 ch;

	ch = *buf;
	if(ch>='0' && ch<='9') {
		*nibble = ch - '0';
		return 1;
	}
	if(ch>='a' && ch<='f') {
		*nibble = ch - 'a' + 10;
		return 1;
	}
	if(ch>='A' && ch<='F') {
		*nibble = ch - 'A' + 10;
		return 1;
	}
	return 0;
}

s32 hstr2byte(const char *buf,s32 *bval)
{
	s32 hnib,lnib;

	if(!hstr2nibble(buf,&hnib) || !hstr2nibble(buf+1,&lnib)) return 0;

	*bval = (hnib<<4)|lnib;
	return 1;
}

const char* vhstr2int(const char *buf,s32 *ival)
{
	s32 i,val,nibble;
	s32 found0,lim;

	found0 = 0;
	for(i=0;i<8;i++,buf++) {
		if(*buf!='0') break;

		found0 = 1;
	}

	val = 0;
	lim = 8 - i;
	for(i=0;i<lim;i++,buf++) {
		if(!hstr2nibble(buf,&nibble)) {
			if(i==0 && !found0) return NULL;

			*ival = val;
			return buf;
		}
		val = (val<<4)|nibble;
	}
	if(hstr2nibble(buf,&nibble)) return NULL;

	*ival = val;
	return buf;
}

const char* fhstr2int(const char *buf,s32 *ival)
{
	s32 i,val,nibble;

	val = 0;
	for(i=0;i<8;i++,buf++) {
		if(!hstr2nibble(buf,&nibble)) return NULL;

		val = (val<<4)|nibble;
	}

	*ival = val;
	return buf;
}

char* int2fhstr(char *buf,s32 val)
{
	s32 i,nibble,shift;

	for(i=0,shift=28;i<8;i++,shift-=4,buf++) {
		nibble = (val>>shift)&0x0f;
		*buf = hexchars[nibble];
	}
	return buf;
}

char* int2vhstr(char *buf,s32 val)
{
	s32 i,nibble,shift;

	for(i=0,shift=28;i<8;i++,shift-=4) {
		nibble = (val>>shift)&0x0f;
		if(nibble) break;
	}
	if(i==8) {
		*buf++ = '0';
		return buf;
	}

	*buf++ = hexchars[nibble];
	for(i++,shift-=4;i<8;i++,shift-=4,buf++) {
		nibble = (val>>shift)&0x0f;
		*buf = hexchars[nibble];
	}
	return buf;
}

char* mem2hstr(char *buf,const char *mem,s32 count)
{
	s32 i;
	char ch;

	for(i=0;i<count;i++,mem++) {
		ch = *mem;
		*buf++ = hexchars[ch>>4];
		*buf++ = hexchars[ch&0x0f];
	}
	*buf = 0;
	return buf;
}
char* thread2fhstr(char *buf,s32 thread)
{
	s32 i,nibble,shift;

	for(i=0;i<8;i++,buf++) *buf = '0';
	for(i=0,shift=28;i<8;i++,shift-=4,buf++) {
		nibble = (thread>>shift)&0x0f;
		*buf = hexchars[nibble];
	}
	return buf;
}

char* thread2vhstr(char *buf,s32 thread)
{
	s32 i,nibble,shift;

	for(i=0,shift=28;i<8;i++,shift-=4) {
		nibble = (thread>>shift)&0x0f;
		if(nibble) break;
	}
	if(i==8) {
		*buf++ = '0';
		return buf;
	}

	*buf++ = hexchars[nibble];
	for(i++,shift-=4;i<8;i++,shift-=4,buf++) {
		nibble = (thread>>shift)&0x0f;
		*buf = hexchars[nibble];
	}
	return buf;
}

const char* fhstr2thread(const char *buf,s32 *thread)
{
	s32 i,nibble,val;

	for(i=0;i<8;i++,buf++)
		if(*buf!='0') return NULL;

	val = 0;
	for(i=0;i<8;i++,buf++) {
		if(!hstr2nibble(buf,&nibble)) return NULL;

		val = (val<<4)|nibble;
	}

	*thread = val;
	return buf;
}

const char* vhstr2thread(const char *buf,s32 *thread)
{
	s32 i,val,nibble;
	s32 found0,lim;

	found0 = 0;
	for(i=0;i<16;i++,buf++) {
		if(*buf!='0') break;

		found0 = 1;
	}

	val = 0;
	lim = 16 - i;
	for(i=0;i<lim;i++,buf++) {
		if(!hstr2nibble(buf,&nibble)) {
			if(i==0 && found0) return NULL;

			*thread = val;
			return buf;
		}

		val = (val<<4)|nibble;
	}
	if(hstr2nibble(buf,&nibble)) return NULL;

	*thread = val;
	return buf;
}

KThread* gdbstub_indextoid(s32 thread)
{
	if (thread <= 0) return NULL;

	return (KThread*)MEM_PHYSICAL_TO_K0(thread);
}

s32 gdbstub_getcurrentthread(void)
{
	return MEM_VIRTUAL_TO_PHYSICAL(KThreadGetSelf());
}

s32 gdbstub_getnextthread(s32 athread)
{
	if (athread <= 0) {
		return MEM_VIRTUAL_TO_PHYSICAL(s_firstThread);
	}

	KThread* t = (KThread*)MEM_PHYSICAL_TO_K0(athread);
	return t->next ? MEM_VIRTUAL_TO_PHYSICAL(t->next) : 0;
}

s32 gdbstub_getoffsets(char **textaddr,char **dataaddr,char **bssaddr)
{
	*textaddr = (char*)((u32)__text_fstart - (u32)__text_fstart);
	*dataaddr = (char*)((u32)__data_fstart - (u32)__text_fstart);
	*bssaddr = (char*)((u32)__bss_fstart - (u32)__text_fstart);

	return 1;
}

s32 gdbstub_getthreadinfo(s32 thread,struct gdbstub_threadinfo *info)
{
	KThread *th;
	char tmp_buf[20];

	if(thread<=0) return 0;

	th = (KThread*)MEM_PHYSICAL_TO_K0(thread);

	strcpy(info->display,"libogc task:   control at: 0x");
	tmp_buf[0] = hexchars[(((int)th)>>28)&0x0f];
	tmp_buf[1] = hexchars[(((int)th)>>24)&0x0f];
	tmp_buf[2] = hexchars[(((int)th)>>20)&0x0f];
	tmp_buf[3] = hexchars[(((int)th)>>16)&0x0f];
	tmp_buf[4] = hexchars[(((int)th)>>12)&0x0f];
	tmp_buf[5] = hexchars[(((int)th)>>8)&0x0f];
	tmp_buf[6] = hexchars[(((int)th)>>4)&0x0f];
	tmp_buf[7] = hexchars[((int)th)&0x0f];
	tmp_buf[8] = 0;
	strcat(info->display,tmp_buf);

	info->name[0] = 0;
	info->name[1] = 0;
	info->name[2] = 0;
	info->name[3] = 0;
	info->name[4] = 0;

	info->more_display[0] = 0;
	return 1;
}

s32 parsezbreak(const char *in,s32 *type,char **addr,u32 *len)
{
	s32 ttmp,atmp,ltmp;

	in++;
	if(!hstr2nibble(in,&ttmp) || *(in+1)!=',') return 0;

	in += 2;
	in = vhstr2int(in,&atmp);
	if(in==NULL || *in!=',') return 0;

	in++;
	in = vhstr2int(in,&ltmp);
	if(in==NULL || ltmp<1) return 0;

	*type = ttmp;
	*addr = (char*)atmp;
	*len = ltmp;

	return 1;
}

s32 parseqp(const char *in,s32 *mask,s32 *thread)
{
	const char *ptr;

	ptr = fhstr2int(in+2,mask);
	if(ptr==NULL) return 0;

	ptr = fhstr2thread(ptr,thread);
	if(ptr==NULL) return 0;

	return 1;
}

void packqq(char *out,s32 mask,s32 thread,struct gdbstub_threadinfo *info)
{
	s32 len;

	*out++ = 'q';
	*out++ = 'Q';
	out = int2fhstr(out,mask);
	out = thread2fhstr(out,thread);

	if(mask&0x01) {
		memcpy(out,"00000001",8);
		out += 8;
		*out++ = '1';
		*out++ = '0';
		out = thread2fhstr(out,thread);
	}
	if(mask&0x02) {
		memcpy(out,"00000002",8);
		out += 8;
		*out++ = '0';
		*out++ = '1';
		*out++ = '1';
	}
	if(mask&0x04) {
		memcpy(out,"00000004",8);
		out += 8;
		
		info->display[sizeof(info->display)-1] = 0;			//for god sake
		len = strlen(info->display);

		*out++ = hexchars[(len>>4)&0x0f];
		*out++ = hexchars[len&0x0f];

		memcpy(out,info->display,len);
		out += len;
	}
	if(mask&0x08) {
		memcpy(out,"00000008",8);
		out += 8;
		
		info->display[sizeof(info->name)-1] = 0;			//for god sake
		len = strlen(info->name);

		*out++ = hexchars[(len>>4)&0x0f];
		*out++ = hexchars[len&0x0f];

		memcpy(out,info->name,len);
		out += len;
	}
	if(mask&0x10) {
		memcpy(out,"00000010",8);
		out += 8;
		
		info->display[sizeof(info->more_display)-1] = 0;			//for god sake
		len = strlen(info->more_display);

		*out++ = hexchars[(len>>4)&0x0f];
		*out++ = hexchars[len&0x0f];

		memcpy(out,info->more_display,len);
		out += len;
	}
	*out = 0;
}

s32 parseql(const char *in,s32 *first,s32 *max_cnt,s32 *athread)
{
	const char *ptr;

	ptr = in+2;
	if(!hstr2nibble(ptr,first)) return 0;

	ptr++;
	if(!hstr2byte(ptr,max_cnt)) return 0;

	ptr += 2;
	ptr = fhstr2thread(ptr,athread);
	if(ptr==NULL) return 0;

	return 1;
}

char* reserve_qmheader(char *out)
{
	return (out+21);
}

char* packqmthread(char *out,s32 thread)
{
	return thread2fhstr(out,thread);
}

void packqmheader(char *out,s32 count,s32 done,s32 athread)
{
	*out++ = 'q';
	*out++ = 'M';
	*out++ = hexchars[(count>>4)&0x0f];
	*out++ = hexchars[count&0x0f];

	if(done) *out++ = '1';
	else *out++ = '0';

	thread2fhstr(out,athread);
}
