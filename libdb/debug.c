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
#include "network.h"
#include "debug.h"
#include "video.h"
#include "bba_dbg.h"
#include "uIP/uip.h"
#include "uIP/uip_arp.h"

//#define _DBG_DEBUG

#define GEKKO_MAX_BP	1024

#define PC_REGNUM		64
#define SP_REGNUM		1

#define BUFMAX			2048		//we take the same as in ppc-stub.c
#define TCPBUFMAX		32768		//we take the same as in ppc-stub.c

#define BPCODE		0x7d821008
#define STR_BPCODE	"7d821008"

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#define STACKSIZE	32768

typedef struct _dbginfo {
	u32 sock;
} dbginfo;

static u8 remcomInBuffer[BUFMAX];
static u8 remcomOutBuffer[BUFMAX];

static lwp_t hdebugger;
static u8 debug_stack[STACKSIZE];

static u32 fault_jmp_buf[100];

static const char hexchars[]="0123456789abcdef";

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
	u32 val;
	struct bp_entry *next;
} bp_entries[GEKKO_MAX_BP];

static u32 dbg_initialized = 0;
static u32 dbg_instep = 0;
static u32 dbg_active = 0;
static u32 dbg_started = 0;
static s32 dbg_arptime = 0;
static u32 dbg_arptimercalled = 0;
static u32 dbg_port = -1;
static s32 dbg_currthr = -1;
static u32 rx_len,tx_sentlen;
static u32 rx_pos,tx_pos;
static u8 rx_buffer[TCPBUFMAX];
static u8 tx_buffer[TCPBUFMAX];

static struct bp_entry *p_bpentries = NULL;

void __breakinst();
void c_debug_handler(frame_context *);
static void* debugger(void*);

extern long long gettime();
extern u32 diff_sec(long long start,long long end);
extern void __lwp_getthreadlist(lwp_obj** thrs);
extern frame_context* __lwp_getthrcontext();
extern u32 __lwp_getcurrentid();
extern u32 __lwp_is_threadactive(s32 thr_id);
extern void __set_ibar(u32 addr);
extern void __enable_ibar();
extern void __disable_ibar();
extern void __clr_ibar();
extern void __exception_load(u32,void *,u32,void*);
extern void __excpetion_close(u32);

extern u8 __text_start[],__data_start[],__bss_start[];
extern u8 __text_fstart[],__data_fstart[],__bss_fstart[];
extern u8 debug_handler_start[],debug_handler_end[],debug_handler_patch[];

u32 gdb_setjmp(u32 *buf)
{
	asm ("mflr 0; stw 0,0(%0);"
		 "stw 1,4(%0); stw 2,8(%0);"
		 "mfcr 0; stw 0,12(%0);"
		 "stmw 13,16(%0)"
		 : : "r" (buf));
	return 0;
}

void gdb_longjmp(u32 *buf,u32 val)
{
	if (val == 0)
		val = 1;
	asm ("lmw 13,16(%0);"
		 "lwz 0,12(%0); mtcrf 0x38,0;"
		 "lwz 0,0(%0); lwz 1,4(%0); lwz 2,8(%0);"
		 "mtlr 0; mr 3,%1"
		 : : "r" (buf), "r" (val));
}

/* Convert ch from a hex digit to an int */
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

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 */
static u8* mem2hex(u8 *mem, u8 *buf, u32 count)
{
	u8 ch;
	u16 tmp_s;
	u32 tmp_l;
	
	if(gdb_setjmp(fault_jmp_buf)==0) {
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
	}
	*buf = 0;
	return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written.
 */
static s8* hex2mem(u8 *buf, u8 *mem, u32 count)
{
	s32 i;
	u8 ch;

	if(gdb_setjmp(fault_jmp_buf)==0) {
		for (i=0; i<count; i++) {
			ch = hex(*buf++) << 4;
			ch |= hex(*buf++);
			*mem++ = ch;
		}
		DCFlushRangeNoSync(mem, count);
		ICInvalidateRange(mem,count);
	}
	return mem;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static s32 hexToInt(u8 **ptr, u32 *intValue)
{
	s32 numChars = 0;
	s32 hexValue;

	*intValue = 0;

	if(gdb_setjmp(fault_jmp_buf)==0) {
		while (**ptr) {
			hexValue = hex(**ptr);
			if (hexValue < 0)
					break;

			*intValue = (*intValue << 4) | hexValue;
			numChars ++;

			(*ptr)++;
		}
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
	u8 buf[10];
	u32 i,old;
	struct bp_entry *p;

	if(!mem2hex(mem,buf,4)) return EINVAL;
	if(!hex2mem(STR_BPCODE,mem,4)) return EINVAL;

	old = strtoul(buf,0,16);
	for(p=p_bpentries;p;p=p->next) {
		if(p->mem == mem) return EINVAL;
	}
	for(i=0;i<GEKKO_MAX_BP;i++) {
		if(bp_entries[i].mem == NULL) break;
	}
	if(i==GEKKO_MAX_BP) return EINVAL;
	
	bp_entries[i].mem = mem;
	bp_entries[i].val = old;
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
	s8 buf[10];

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

	sprintf(buf,"%08x",f->val);
	hex2mem(buf,f->mem,4);
	ICFlashInvalidate();
	DCFlushRangeNoSync((void*)f->mem,4);
	ICInvalidateRange((void*)f->mem,4);
	
	return 0;
}

static void poll()
{
	u32 i;
	s64 now;

	if(!dbg_arptime) dbg_arptime = gettime();
	
	now = gettime();
	uip_len = bba_read();
	if(uip_len==0) {
		for(i=0;i<UIP_CONNS;i++) {
			uip_periodic(i);
			if(uip_len>0) {
				uip_arp_out();
				bba_send();
			}
		}
		if(diff_sec(dbg_arptime,now)>=10 && !dbg_arptimercalled) {
			uip_arp_timer();
			dbg_arptime = gettime();
			dbg_arptimercalled = 1;
		}
		return;
	} else {
		if(BUF->type==HTONS(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			if(uip_len>0) {
				uip_arp_out();
				bba_send();
			}
		} else if(BUF->type==HTONS(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if(uip_len>0) bba_send();
		}
	}
	dbg_arptimercalled = 0;
}

static void acked()
{
	if((tx_pos-tx_sentlen)>0) memmove(tx_buffer,tx_buffer+tx_sentlen,(tx_pos-tx_sentlen));
	tx_pos -= tx_sentlen;
	tx_sentlen = 0;
}

static void senddata()
{
	if(tx_sentlen>0 || !tx_pos) return;

	tx_sentlen = tx_pos;
	if(tx_sentlen>uip_mss()) tx_sentlen = uip_mss();
	uip_send(tx_buffer,tx_sentlen);
}

void DEBUG_Init(u32 port)
{
	if(dbg_port!=-1) return;
	dbg_port = port;

	bba_init();
	uip_init();
	uip_arp_init();
	uip_listen((u16_t)dbg_port);

	if(LWP_CreateThread(&hdebugger,debugger,NULL,debug_stack,STACKSIZE,20)!=-1) {
		__exception_load(EX_PRG,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
		__exception_load(EX_TRACE,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
		dbg_initialized = 1;
	}
}

static s32 getdbgchar()
{
	s32 ch;
	
	poll();

	ch = -1;
	if(rx_len>0) {
		ch = rx_buffer[0];
		memmove(rx_buffer,rx_buffer+1,rx_len-1);
		rx_len--;
	}
	return ch;
}

static void putdbgchar(s32 ch)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(tx_pos<TCPBUFMAX) {
		tx_buffer[tx_pos++] = (u8)ch;
		tx_buffer[tx_pos] = 0;
	}
	_CPU_ISR_Restore(level);
}

static void putdbgstr(const char *str)
{
	while(str && *str!='\0') putdbgchar(*str++);
}

static u32 get_packet(u8 *buffer)
{
	u8 chksum;
	u8 xmitcsum;
	s32 ch;
	u32 cnt,i,nib;

	ch = getdbgchar();
	if((ch&0x7f)=='$') {
		do {
			cnt = 0;
			chksum = 0;
			xmitcsum = -1;
			while(cnt<BUFMAX) {
				ch = getdbgchar();
				if(ch==-1) return 0;
				
				ch &= 0x7f;
				if(ch=='#') break;

				chksum += ch;
				buffer[cnt] = ch;
				cnt++;
			}
			if(cnt>=BUFMAX) continue;

			buffer[cnt] = 0;
			if(ch=='#') {
				if((nib=getdbgchar())==-1) return 0;
				xmitcsum = hex(nib&0x7f)<<4;
				if((nib=getdbgchar())==-1) return 0;
				xmitcsum |= hex(nib&0x7f);

				if(chksum!=xmitcsum) putdbgchar('-');
				else {
					putdbgchar('+');
					if(buffer[2]==':') {
						putdbgchar(buffer[0]);
						putdbgchar(buffer[1]);
						
						cnt = strlen(buffer);
						for(i=3;i<=cnt;i++)
							buffer[i-3] = buffer[i];
					}
				}
			}
		} while(chksum!=xmitcsum);
		return 1;
	}
	return 0;
}

static u32 put_packet(u8 *buffer)
{
	u8 checksum;
	u32 cnt,i,recv;
	u8 ch,out[1024];

	do {
		out[0] = '$';

		i = 1;
		checksum = 0;
		cnt = 0;
		while((ch=buffer[cnt])) {
			out[i] = ch;
			checksum += ch;
			cnt++; i++;
		}
		out[i++] = '#';
		out[i++] = hexchars[checksum>>4];
		out[i++] = hexchars[checksum&0xf];
		out[i] = '\0';
		putdbgstr(out);
		
		while((recv=getdbgchar())==-1);
	} while((recv&0x7f)!='+');
	return cnt;
}

static void handle_query()
{
	u32 i;
	u8 *ptr;
	lwp_obj *threads = NULL;

//	printf("%s\n",remcomInBuffer);

	__lwp_getthreadlist(&threads);
	if(remcomInBuffer[1]=='C') 
		sprintf(remcomOutBuffer,"QC%04x",((lwp_cntrl*)threads[dbg_currthr].thethread)->id);
	else if(strncmp(&remcomInBuffer[2],"ThreadInfo",10)==0) {
		ptr = remcomOutBuffer;
		if(remcomInBuffer[1]=='f') {
			*ptr = 'm';
			for(i=0;i<1024;i++) {
				if(threads && threads[i].lwp_id!=-1 && threads[i].thethread) {
					ptr++;
					ptr = mem2hex((u8*)&((lwp_cntrl*)threads[i].thethread)->id,ptr,4);
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
	frame_context *pctx = ctx;

	if(dbg_active) return;

	dbg_active = 1;
	dbg_started = 1;

	msr = mfmsr();
	mtmsr(msr&~MSR_EE);

	if(ctx->SRR0==(u32)__breakinst) ctx->SRR0 += 4;
	
	sigval = computeSignal(ctx->EXCPT_Number);
	
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
	
	put_packet(remcomOutBuffer);
	
	while(1) {
		remcomOutBuffer[0] = 0;
		
		if(get_packet(remcomInBuffer)) {
			switch(remcomInBuffer[0]) {
				case '?':
					remcomOutBuffer[0] = 'S';
					remcomOutBuffer[1] = hexchars[sigval>>4];
					remcomOutBuffer[2] = hexchars[sigval&0x0f];
					remcomOutBuffer[3] = 0;
					break;
				case 'H':
					if(remcomInBuffer[2]=='-') dbg_currthr = __lwp_getcurrentid();
					else dbg_currthr = strtoul(remcomInBuffer+2,0,16);
					if(dbg_currthr==__lwp_getcurrentid()) pctx = ctx;
					else pctx = __lwp_getthrcontext(dbg_currthr);
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
				case 'c':
				case 'D':
					ctx->SRR1 &= ~MSR_SE;
					dbg_instep = 0;
					dbg_active = 0;
					mtmsr(msr);
					return;
				case 's':
					ctx->SRR1 |= MSR_SE; 
					dbg_instep = 1;
					dbg_active = 0;
					mtmsr(msr);
					return;
				case 'T':
					ptr = &remcomInBuffer[1];
					hexToInt(&ptr,&thrid);
					if(__lwp_is_threadactive(thrid)) strcpy(remcomOutBuffer,"OK");
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
			put_packet(remcomOutBuffer);
		}
	}
}

static void* debugger(void *arg)
{
	u32 addr,len,res;
	s32 thrid;
	u8 *ptr;
	frame_context *ctx = NULL;

	dbg_currthr = _thr_executing->id;
	while(1) {
		VIDEO_WaitVSync();

		remcomOutBuffer[0] = 0;
		if(get_packet(remcomInBuffer)) {
			switch(remcomInBuffer[0]) {
				case '?':
					remcomOutBuffer[0] = 'S';
					remcomOutBuffer[1] = hexchars[EX_PRG>>4];
					remcomOutBuffer[2] = hexchars[EX_PRG&0x0f];
					remcomOutBuffer[3] = 0;
					break;
				case 'H':
					if(remcomInBuffer[2]=='-') dbg_currthr = __lwp_getcurrentid();
					else dbg_currthr = strtoul(remcomInBuffer+2,0,16);
					ctx = __lwp_getthrcontext(dbg_currthr);
					strcpy(remcomOutBuffer,"OK");
					break;
				case 'c':
				case 'k':
					dbg_instep = 0;
					continue;
				case 'g':
					ptr = remcomOutBuffer;
					ptr = mem2hex((u8*)ctx->GPR,ptr,32*4);
					ptr = mem2hex((u8*)ctx->FPR,ptr,32*8);
					ptr = mem2hex((u8*)&ctx->LR,ptr,4);
					ptr = mem2hex((u8*)&ctx->MSR,ptr,4);
					ptr = mem2hex((u8*)&ctx->CR,ptr,4);
					ptr = mem2hex((u8*)&ctx->LR,ptr,4);
					ptr = mem2hex((u8*)&ctx->CTR,ptr,4);
					ptr = mem2hex((u8*)&ctx->XER,ptr,4);
					break;
				case 'm':
					ptr = &remcomInBuffer[1];
					if(hexToInt(&ptr,&addr) && *ptr++==',' && hexToInt(&ptr,&len)) {
						if(mem2hex((u8*)addr,remcomOutBuffer,len)) break;
						strcpy(remcomOutBuffer,"E03");
					} else
						strcpy(remcomOutBuffer,"E01");
					break;
				case 'q':
					handle_query();
					break;
				case 'T':
					ptr = &remcomInBuffer[1];
					hexToInt(&ptr,&thrid);
					if(__lwp_is_threadactive(thrid)) strcpy(remcomOutBuffer,"OK");
					else strcpy(remcomOutBuffer,"E01");
					break;
				case 'z':
				case 'Z':
					if(remcomInBuffer[1]!='0') {
						strcpy(remcomOutBuffer,"ERROR");
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
			put_packet(remcomOutBuffer);
		}
	}
	return 0;
}

void debugger_appcall()
{
	struct debugger_state *dbg_state;

	//printf("debugger_appcall()\n");

	if(uip_conn->lport==HTONS(dbg_port)) {
		//printf("debugger_appcall(%d)\n",uip_conn->lport);
		dbg_state = (struct debugger_state*)uip_conn->appstate;
		if(uip_connected()) {
			dbg_state->state = DBG_CONNECTED;
			dbg_state->alive = 0;
			rx_len = rx_pos = 0;
			tx_sentlen = tx_pos = 0;
			memset(rx_buffer,0,TCPBUFMAX);
			memset(tx_buffer,0,TCPBUFMAX);
			return;
		}
		if(uip_acked()) {
			acked();
		}
		if(uip_newdata() && dbg_state->state==DBG_CONNECTED) {
			if((rx_len+uip_datalen())<=TCPBUFMAX) {
				memcpy(rx_buffer+rx_len,(void*)uip_appdata,uip_datalen());
				rx_len += uip_datalen();
			}
		}

		if(uip_rexmit() || uip_newdata() || uip_acked()) {
			senddata();
		} else if(uip_poll()) {
			senddata();
		}
	}
}

void uip_log(char *msg)
{
	printf("uIP log msg: %s\n",msg);
}

void _break(void)
{
	if(!dbg_initialized) return;
	__asm__ __volatile__ (".globl __breakinst\n\
						   __breakinst: .long 0x7d821008");
}

