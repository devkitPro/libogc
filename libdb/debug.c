#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include "asm.h"
#include "processor.h"
#include "spinlock.h"
#include "lwp.h"
#include "lwp_threads.h"
#include "context.h"
#include "cache.h"
#include "video.h"
#include "ogcsys.h"
#include "debug.h"

#include "uIP/bba.h"
#include "uIP/memr.h"
#include "uIP/tcpip.h"
#include "uIP/uip_ip.h"
#include "uIP/uip_arp.h"
#include "uIP/uip_pbuf.h"
#include "uIP/uip_netif.h"

#define GEKKO_MAX_BP	1024

#define PC_REGNUM		64
#define SP_REGNUM		1

#define STACKSIZE		32768
#define BUFMAX			2048		//we take the same as in ppc-stub.c

#define STR_BPCODE		"7d821008"

static s32 dbg_currthr = -1;
static s32 dbg_active = 0;
static s32 dbg_instep = 0;
static s32 dbg_initialized = 0;

static s32 dbg_datasock = -1;
static s32 dbg_listensock = -1;
static struct uip_netif g_netif;

static u8 remcomInBuffer[BUFMAX];
static u8 remcomOutBuffer[BUFMAX];

static const u8 hexchars[]="0123456789abcdef";

static struct hard_trap_info {
	u32 tt;
	u8 signo;
} hard_trap_info[] = {
	{EX_MACH_CHECK,SIGSEGV},/* Machine Check */
	{EX_DSI,SIGSEGV},		/* Adress Error(store) DSI */
	{EX_ISI,SIGBUS},		/* Instruction Bus Error ISI */
	{EX_INT,SIGINT},		/* Interrupt */
	{EX_ALIGN,SIGBUS},		/* Alignment */
	{EX_PRG,SIGTRAP},		/* Breakpoint Trap */
	{EX_FP,SIGFPE},			/* FPU unavail */
	{EX_DEC,SIGALRM},		/* Decrementer */
	{EX_SYS_CALL,SIGSYS},	/* System Call */			
	{EX_TRACE,SIGTRAP},		/* Singel-Step/Watch */
	{0xB,SIGILL},			/* Reserved */
	{EX_IBAR,SIGTRAP},		/* Instruction Address Breakpoint (GEKKO) */
	{0xD,SIGFPE},			/* FP assist */			
	{0,0}					/* MUST be the last */
};

static struct bp_entry {
	u8 *mem;
	u8 val[8];
	struct bp_entry *next;
} bp_entries[GEKKO_MAX_BP];

static struct bp_entry *p_bpentries = NULL;

void __breakinst();

extern void __lwp_getthreadlist(lwp_obj** thrs);
extern frame_context* __lwp_getthrcontext();
extern u32 __lwp_getcurrentid();
extern u32 __lwp_is_threadactive(s32 thr_id);
extern void __exception_load(u32,void *,u32,void*);
extern void __excpetion_close(u32);

extern u8 __text_start[],__data_start[],__bss_start[];
extern u8 __text_fstart[],__data_fstart[],__bss_fstart[];
extern u8 debug_handler_start[],debug_handler_end[],debug_handler_patch[];

void DEBUG_Init(u16 port)
{
	uipdev_s hbba;
	struct uip_ip_addr local_ip,netmask,gw;
	struct uip_netif *pnet ;
	struct sockaddr_in name;
	socklen_t namelen = sizeof(struct sockaddr);

	memr_init();
	uip_ipinit();
	uip_pbuf_init();
	uip_netif_init();

	tcpip_init();

	local_ip.addr = uip_ipaddr(dbg_local_ip);
	netmask.addr = uip_ipaddr(dbg_netmask);
	gw.addr = uip_ipaddr(dbg_gw);

	hbba = uip_bba_create(&g_netif);
	pnet = uip_netif_add(&g_netif,&local_ip,&netmask,&gw,hbba,uip_bba_init,uip_ipinput);
	if(pnet) {
		uip_netif_setdefault(pnet);

		dbg_listensock = tcpip_socket();
		if(dbg_listensock<0) return ;

		name.sin_addr.s_addr = INADDR_ANY;
		name.sin_port = htons(port);
		name.sin_family = AF_INET;

		if(tcpip_bind(dbg_listensock,(struct sockaddr*)&name,&namelen)<0){
			tcpip_close(dbg_listensock);
			dbg_listensock = -1;
			return;
		}
		if(tcpip_listen(dbg_listensock,1)<0) {
			tcpip_close(dbg_listensock);
			dbg_listensock = -1;
			return;
		}

		__exception_load(EX_DSI,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
		__exception_load(EX_PRG,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
		__exception_load(EX_TRACE,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);

		dbg_initialized = 1;
	}
}

static s32 hex(u8 ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

static u8* mem2hex(u8 *mem, u8 *buf, u32 count)
{
	u8 ch;
	u16 tmp_s;
	u32 tmp_l;
	
	if ((count==2) && (((s32)mem&1)==0)) {
		tmp_s = *(u16*)mem;
		mem += 2;
		*buf++ = hexchars[(tmp_s>>12)& 0xf];
		*buf++ = hexchars[(tmp_s>>8)&0xf];
		*buf++ = hexchars[(tmp_s>>4)&0xf];
		*buf++ = hexchars[tmp_s&0xf];

	} else if((count==4) && (((s32)mem&3)==0)) {
		tmp_l = *(u32*)mem;
		mem += 4;
		*buf++ = hexchars[(tmp_l>>28)&0xf];
		*buf++ = hexchars[(tmp_l>>24)&0xf];
		*buf++ = hexchars[(tmp_l>>20)&0xf];
		*buf++ = hexchars[(tmp_l>>16)&0xf];
		*buf++ = hexchars[(tmp_l>>12)&0xf];
		*buf++ = hexchars[(tmp_l>>8)&0xf];
		*buf++ = hexchars[(tmp_l>>4)&0xf];
		*buf++ = hexchars[tmp_l & 0xf];

	} else {
		while (count-- > 0) {
			ch = *mem++;
			*buf++ = hexchars[ch >> 4];
			*buf++ = hexchars[ch & 0xf];
		}
	}
	*buf = 0;
	return buf;
}

static s8* hex2mem(u8 *buf, u8 *mem, u32 count)
{
	s32 i;
	u8 ch;

	for (i=0; i<count; i++) {
		ch = hex(*buf++) << 4;
		ch |= hex(*buf++);
		*mem++ = ch;
	}
	DCFlushRangeNoSync(mem, count);
	ICInvalidateRange(mem,count);

	return mem;
}

static s32 hexToInt(u8 **ptr, u32 *intValue)
{
	s32 numChars = 0;
	s32 hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
				break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}

	return (numChars);
}

static s32 computeSignal(s32 excpt)
{
	struct hard_trap_info *ht;
	for(ht = hard_trap_info;ht->tt && ht->signo;ht++) {
		if(ht->tt==excpt) return ht->signo;
	}
	return SIGHUP;
}

u32 insert_bp(u8 *mem)
{
	u32 i;
	struct bp_entry *p;

	for(p=p_bpentries;p;p=p->next) {
		if(p->mem == mem) return EINVAL;
	}
	for(i=0;i<GEKKO_MAX_BP;i++) {
		if(bp_entries[i].mem == NULL) break;
	}
	if(i==GEKKO_MAX_BP) return EINVAL;
	
	if(!mem2hex(mem,bp_entries[i].val,4)) return EINVAL;
	if(!hex2mem(STR_BPCODE,mem,4)) return EINVAL;

	bp_entries[i].mem = mem;
	bp_entries[i].next = p_bpentries;
	p_bpentries = &bp_entries[i];

	ICFlashInvalidate();
	DCFlushRangeNoSync((void*)bp_entries[i].mem,4);
	ICInvalidateRange((void*)bp_entries[i].mem,4);

	return 0;
}

u32 remove_bp(u8 *mem)
{
	struct bp_entry *e = p_bpentries;
	struct bp_entry *f = NULL;

	if(!e) return EINVAL;
	if(e->mem==mem) {
		p_bpentries = e->next;
		f = e;
	} else {
		for(;e->next;e=e->next) {
			if(e->next->mem==mem) {
				f = e->next;
				e->next = f->next;
				break;
			}
		}
	}
	if(!f) return EINVAL;

	hex2mem(f->val,f->mem,4);
	ICFlashInvalidate();
	DCFlushRangeNoSync((void*)f->mem,4);
	ICInvalidateRange((void*)f->mem,4);
	
	return 0;
}

static u8 getdbgchar()
{
	u8 ch = 0xff;
	s32 len = 0;

	if(dbg_datasock>=0)
		len = tcpip_read(dbg_datasock,&ch,1);

	return (len>0)?ch:0xff;
}

static void putdbgchar(u8 ch)
{
	if(dbg_datasock>=0)
		tcpip_write(dbg_datasock,&ch,1);
}

static void putdbgstr(const u8 *str)
{
	if(dbg_datasock>=0)
		tcpip_write(dbg_datasock,str,strlen(str));
}

static void getpacket(u8 *buffer)
{
	u8 ch;
	u8 chksum,xmitsum;
	s32 i,cnt;

	do {
		while((ch=(getdbgchar()&0x7f))!='$');

		cnt = 0;
		chksum = 0;
		xmitsum = -1;
		
		while(cnt<BUFMAX) {
			ch = getdbgchar()&0x7f;
			if(ch=='#') break;

			chksum += ch;
			buffer[cnt] = ch;
			cnt++;
		}
		if(cnt>=BUFMAX) continue;
		
		buffer[cnt] = 0;
		if(ch=='#') {
			xmitsum = hex(getdbgchar()&0x7f)<<4;
			xmitsum |= hex(getdbgchar()&0x7f);
			
			if(chksum!=xmitsum) putdbgchar('-');
			else {
				putdbgchar('+');
				
				if(buffer[2]==':') {
					putdbgchar(buffer[0]);
					putdbgchar(buffer[1]);

					cnt = strlen(buffer);
					for(i=3;i<=cnt;i++) buffer[i-3] = buffer[i];
				}
			}
		}
	} while(chksum!=xmitsum);
}

static void putpacket(const u8 *buffer)
{
	u8 recv;
	u8 chksum,ch;
	u8 *ptr,out[1024];
	const u8 *inp;

	do {
		inp = buffer;
		ptr = out;
		
		*ptr++ = '$';
		
		chksum = 0;
		while((ch=*inp++)!='\0') {
			*ptr++ = ch;
			chksum += ch;
		}
		
		*ptr++ = '#';
		*ptr++ = hexchars[chksum>>4];
		*ptr++ = hexchars[chksum&0x0f];
		*ptr = '\0';
	
		putdbgstr(out);

		while((recv= getdbgchar())==0xff);
	} while((recv&0x7f)!='+');
}

static void handle_query()
{
	u32 i,id;
	u8 *ptr;
	lwp_obj *threads = NULL;

//	printf("%s\n",remcomInBuffer);

	__lwp_getthreadlist(&threads);
	if(remcomInBuffer[1]=='C') 
		sprintf(remcomOutBuffer,"QC%04x",threads[dbg_currthr].thethread.id+1);
	else if(strncmp(&remcomInBuffer[2],"ThreadInfo",10)==0) {
		ptr = remcomOutBuffer;
		if(remcomInBuffer[1]=='f') {
			*ptr = 'm';
			for(i=0;i<1024;i++) {
				if(threads && threads[i].lwp_id!=-1) {
					ptr++;
					id = threads[i].thethread.id+1;
					ptr = mem2hex((u8*)&id,ptr,4);
					*ptr =  ',';
				}
			}
		} else if(remcomInBuffer[1]=='s') *ptr++ = 'l';
		*ptr = 0;
	} else if(strncmp(&remcomInBuffer[1],"Symbol::",8)==0) 
		strcpy(remcomOutBuffer,"OK");
	else if(strncmp(&remcomInBuffer[1],"Offsets",7)==0)
		sprintf(remcomOutBuffer,"Text=%08x;Data=%08x;Bss=%08x",((u32)__text_fstart-(u32)__text_fstart),((u32)__data_fstart-(u32)__text_fstart),((u32)__bss_fstart-(u32)__text_fstart));
}

void c_debug_handler(frame_context *ctx)
{
	u8* ptr;
	s32 thrid;
	u32 addr,len,msr;
	u32 sigval,res;
	u32 except_thr;
	frame_context *pctx = NULL;

	msr = mfmsr();
	mtmsr(msr&~MSR_EE);

	if(ctx->SRR0==(u32)__breakinst) ctx->SRR0 += 4;
	
	if(dbg_listensock>=0 && (dbg_datasock<0 || dbg_active==0))
		dbg_datasock = tcpip_accept(dbg_listensock);
	if(dbg_datasock<0) return;
	else tcpip_starttimer(dbg_datasock);
	
	sigval = computeSignal(ctx->EXCPT_Number);
	
	dbg_active = 1;

	ptr = remcomOutBuffer;
	*ptr++ = 'T';
	*ptr++ = hexchars[sigval>>4];
	*ptr++ = hexchars[sigval&0x0f];
	*ptr++ = hexchars[PC_REGNUM>>4];
	*ptr++ = hexchars[PC_REGNUM&0x0f];
	*ptr++ = ':';
	ptr = mem2hex((u8*)&ctx->SRR0,ptr,4);
	*ptr++ = ';';
	*ptr++ = hexchars[SP_REGNUM>>4];
	*ptr++ = hexchars[SP_REGNUM&0x0f];
	*ptr++ = ':';
	ptr = mem2hex((u8*)&ctx->GPR[1],ptr,4);
	*ptr++ = ';';
	*ptr++ = 0;
	
	putpacket(remcomOutBuffer);

	pctx = ctx;
	except_thr = __lwp_getcurrentid();

	while(1) {
		remcomOutBuffer[0] = 0;
		getpacket(remcomInBuffer);
		switch(remcomInBuffer[0]) {
			case '?':
				remcomOutBuffer[0] = 'S';
				remcomOutBuffer[1] = hexchars[sigval>>4];
				remcomOutBuffer[2] = hexchars[sigval&0x0f];
				remcomOutBuffer[3] = 0;
				break;
			case 'H':
				if(remcomInBuffer[2]=='-') dbg_currthr = except_thr;
				else dbg_currthr = strtoul(remcomInBuffer+2,0,16)-1;
				if(dbg_currthr!=except_thr) pctx = __lwp_getthrcontext(dbg_currthr);
				else pctx = ctx;
				strcpy(remcomOutBuffer,"OK");
				break;
			case 'g':
				ptr = remcomOutBuffer;
				ptr = mem2hex((u8*)pctx->GPR,ptr,32*4);
				ptr = mem2hex((u8*)pctx->FPR,ptr,32*8);
				if(pctx==ctx) 
					ptr = mem2hex((u8*)&pctx->SRR0,ptr,4);
				else 
					ptr = mem2hex((u8*)&pctx->LR,ptr,4);
				ptr = mem2hex((u8*)&pctx->MSR,ptr,4);
				ptr = mem2hex((u8*)&pctx->CR,ptr,4);
				ptr = mem2hex((u8*)&pctx->LR,ptr,4);
				ptr = mem2hex((u8*)&pctx->CTR,ptr,4);
				ptr = mem2hex((u8*)&pctx->XER,ptr,4);
				break;
			case 'm':
				ptr = &remcomInBuffer[1];
				if(hexToInt(&ptr,&addr) && (addr&0xC0000000) 
					&& *ptr++==',' && hexToInt(&ptr,&len)) {
					if(mem2hex((u8*)addr,remcomOutBuffer,len)) break;
					strcpy(remcomOutBuffer,"E03");
				} else
					strcpy(remcomOutBuffer,"E01");
				break;
			case 'q':
				handle_query();
				break;
			case 'k':
			case 'D':
				ctx->SRR1 &= ~MSR_SE;
				dbg_instep = 0;
				dbg_active = 0;
				mtmsr(msr);
				tcpip_close(dbg_datasock);
				dbg_datasock = -1;
				return;
			case 'c':
				ctx->SRR1 &= ~MSR_SE;
				dbg_instep = 0;
				dbg_active = 1;
				mtmsr(msr);
				tcpip_stoptimer(dbg_datasock);
				return;
			case 's':
				ctx->SRR1 |= MSR_SE; 
				dbg_instep = 1;
				dbg_active = 1;
				mtmsr(msr);
				tcpip_stoptimer(dbg_datasock);
				return;
			case 'T':
				ptr = &remcomInBuffer[1];
				hexToInt(&ptr,&thrid);
				if(__lwp_is_threadactive(thrid-1)) strcpy(remcomOutBuffer,"OK");
				else strcpy(remcomOutBuffer,"E01");
				break;
			case 'z':
			case 'Z':
				if(remcomInBuffer[1]!='0') {
					strcpy(remcomOutBuffer,"E01");
					break;
				}

				ptr = remcomInBuffer+3;
				hexToInt(&ptr,&addr);

				res =  0;
				if(remcomInBuffer[0]=='Z') res = insert_bp((u8*)addr);
				else remove_bp((u8*)addr);

				strcpy(remcomOutBuffer,"OK");
				break;
		}
		putpacket(remcomOutBuffer);
	}
	
}

void _break(void)
{
	if(!dbg_initialized) return;
	__asm__ __volatile__ (".globl __breakinst\n\
						   __breakinst: .long 0x7d821008");
}

