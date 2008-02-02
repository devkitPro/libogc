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
#include "sys_state.h"
#include "context.h"
#include "cache.h"
#include "video.h"
#include "ogcsys.h"
#include "debug.h"

#include "uIP/bba.h"
#include "uIP/memr.h"
#include "tcpip.h"
#include "uIP/uip_ip.h"
#include "uIP/uip_arp.h"
#include "uIP/uip_pbuf.h"
#include "uIP/uip_netif.h"

#include "lwp_config.h"

#include "debug_supp.h"

#define GEKKO_MAX_BP	64

#define SP_REGNUM		1			//register no. for stackpointer
#define PC_REGNUM		64			//register no. for programcounter (srr0)

#define BUFMAX			2048		//we take the same as in ppc-stub.c

#define BPCODE			0x7d821008

#define highhex(x)		hexchars [((x)>>4)&0xf]
#define lowhex(x)		hexchars [(x)&0xf]

#if UIP_LOGGING == 1
#include <stdio.h>
#define UIP_LOG(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

static s32 dbg_active = 0;
static s32 dbg_instep = 0;
static s32 dbg_initialized = 0;

static s32 dbg_datasock = -1;
static s32 dbg_listensock = -1;
static struct uip_netif g_netif;

static u8 remcomInBuffer[BUFMAX];
static u8 remcomOutBuffer[BUFMAX];

const u8 hexchars[]="0123456789abcdef";

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
	{EX_IABR,SIGTRAP},		/* Instruction Address Breakpoint (GEKKO) */
	{0xD,SIGFPE},			/* FP assist */			
	{0,0}					/* MUST be the last */
};

static struct bp_entry {
	u32 *address;
	u32 instr;
	struct bp_entry *next;
} bp_entries[GEKKO_MAX_BP];

static struct bp_entry *p_bpentries = NULL;

static frame_context current_thread_registers;

void __breakinst();
void c_debug_handler(frame_context *ctx);

extern void dbg_exceptionhandler();
extern void __exception_sethandler(u32 nExcept, void (*pHndl)(frame_context*));

extern void __clr_iabr();
extern void __enable_iabr();
extern void __disable_iabr();
extern void __set_iabr(void *);

extern u8 __text_start[],__data_start[],__bss_start[];
extern u8 __text_fstart[],__data_fstart[],__bss_fstart[];

static __inline__ void bp_init()
{
	s32 i;

	for(i=0;i<GEKKO_MAX_BP;i++) {
		bp_entries[i].address = NULL;
		bp_entries[i].instr = 0xffffffff;
		bp_entries[i].next = NULL;
	}
}

void DEBUG_Init(u16 port)
{
	u32 level;
	uipdev_s hbba;
	struct uip_ip_addr local_ip,netmask,gw;
	struct uip_netif *pnet ;
	struct sockaddr_in name;
	socklen_t namelen = sizeof(struct sockaddr);

	UIP_LOG("DEBUG_Init()\n");

	__lwp_thread_dispatchdisable();

	bp_init();
	memr_init();
	uip_ipinit();
	uip_pbuf_init();
	uip_netif_init();

	tcpip_init();
	
	local_ip.addr = uip_ipaddr((const u8_t*)dbg_local_ip);
	netmask.addr = uip_ipaddr((const u8_t*)dbg_netmask);
	gw.addr = uip_ipaddr((const u8_t*)dbg_gw);

	hbba = uip_bba_create(&g_netif);
	pnet = uip_netif_add(&g_netif,&local_ip,&netmask,&gw,hbba,uip_bba_init,uip_ipinput);
	if(pnet) {
		uip_netif_setdefault(pnet);

		dbg_listensock = tcpip_socket();
		if(dbg_listensock<0) {
			__lwp_thread_dispatchenable();
			return;
		}

		name.sin_addr.s_addr = INADDR_ANY;
		name.sin_port = htons(port);
		name.sin_family = AF_INET;

		if(tcpip_bind(dbg_listensock,(struct sockaddr*)&name,&namelen)<0){
			tcpip_close(dbg_listensock);
			dbg_listensock = -1;
			__lwp_thread_dispatchenable();
			return;
		}
		if(tcpip_listen(dbg_listensock,1)<0) {
			tcpip_close(dbg_listensock);
			dbg_listensock = -1;
			__lwp_thread_dispatchenable();
			return;
		}
		
		_CPU_ISR_Disable(level);
		__exception_sethandler(EX_DSI,dbg_exceptionhandler);
		__exception_sethandler(EX_PRG,dbg_exceptionhandler);
		__exception_sethandler(EX_TRACE,dbg_exceptionhandler);
		__exception_sethandler(EX_IABR,dbg_exceptionhandler);
		_CPU_ISR_Restore(level);

		dbg_initialized = 1;
		
	}
	__lwp_thread_dispatchenable();
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

static s32 hexToInt(u8 **ptr, s32 *ival)
{
	s32 cnt;
	s32 val,nibble;

	val = 0;
	cnt = 0;
	while(**ptr) {
		nibble = hex(**ptr);
		if(nibble<0) break;

		val = (val<<4)|nibble;
		cnt++;

		(*ptr)++;
	}
	*ival = val;
	return cnt;
}

static s32 computeSignal(s32 excpt)
{
	struct hard_trap_info *ht;
	for(ht = hard_trap_info;ht->tt && ht->signo;ht++) {
		if(ht->tt==excpt) return ht->signo;
	}
	return SIGHUP;
}

u32 insert_bp(void *mem)
{
	u32 i;
	struct bp_entry *p;

	for(p=p_bpentries;p;p=p->next) {
		if(p->address==mem && p->instr==0xffffffff) goto setbreak;
	}
	for(i=0;i<GEKKO_MAX_BP;i++) {
		if(bp_entries[i].address == NULL) break;
	}
	if(i==GEKKO_MAX_BP) return 0;
	
	p = &bp_entries[i];
	p->next = p_bpentries;
	p->address = mem;
	p_bpentries = p;

setbreak:
	p->instr = *(p->address);
	*(p->address) = BPCODE;
	
	DCStoreRangeNoSync((void*)((u32)mem&~0x1f),32);
	ICInvalidateRange((void*)((u32)mem&~0x1f),32);
	_sync();

	return 1;
}

u32 remove_bp(void *mem)
{
	struct bp_entry *e = p_bpentries;
	struct bp_entry *f = NULL;

	if(!e) return 0;
	if(e->address==mem) {
		p_bpentries = e->next;
		f = e;
	} else {
		for(;e->next;e=e->next) {
			if(e->next->address==mem) {
				f = e->next;
				e->next = f->next;
				break;
			}
		}
	}
	if(!f) return 0;

	*(f->address) = f->instr;
	f->instr = 0xffffffff;

	DCStoreRangeNoSync((void*)((u32)mem&~0x1f),32);
	ICInvalidateRange((void*)((u32)mem&~0x1f),32);
	_sync();

	return 1;
}

static u8 getdbgchar()
{
	u8 ch = 0;
	s32 len = 0;

	if(dbg_datasock>=0)
		len = tcpip_read(dbg_datasock,&ch,1);

	return (len>0)?ch:0;
}

static void putdbgchar(u8 ch)
{
	if(dbg_datasock>=0)
		tcpip_write(dbg_datasock,&ch,1);
}

static void putdbgstr(const u8 *str)
{
	if(dbg_datasock>=0)
		tcpip_write(dbg_datasock,str,strlen((const char*)str));
}

static void putpacket(const u8 *buffer)
{
	u8 recv;
	u8 chksum,ch;
	u8 *ptr;
	const u8 *inp;
	static u8 outbuf[2048];

	outbuf[0] = '+';
	do {
		inp = buffer;
		ptr = &outbuf[1];
		
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
	
		putdbgstr(outbuf);

		recv = getdbgchar();
	} while((recv&0x7f)!='+');
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
				if(buffer[2]==':') {
					putdbgchar(buffer[0]);
					putdbgchar(buffer[1]);

					cnt = strlen((const char*)buffer);
					for(i=3;i<=cnt;i++) buffer[i-3] = buffer[i];
				}
			}
		}
	} while(chksum!=xmitsum);
}

static void process_query(const char *inp,char *outp,s32 thread)
{
	char *optr;

	switch(inp[1]) {
		case 'C':
			optr = outp;
			*optr++ = 'Q';
			*optr++ = 'C';
			optr = thread2vhstr(optr,thread);
			*optr++ = 0;
			break;
		case 'P':
			{
				s32 ret,rthread,mask;
				struct gdbstub_threadinfo info;

				ret = parseqp(inp,&mask,&rthread);
				if(!ret || mask&~0x1f) {
					strcpy(outp,"E01");
					break;
				}

				ret = gdbstub_getthreadinfo(rthread,&info);
				if(!ret) {
					strcpy(outp,"E02");
					break;
				}
				packqq(outp,mask,rthread,&info);
			}
			break;
		case 'L':
			{
				s32 ret,athread,first,max_cnt,i,done,rthread;

				ret = parseql(inp,&first,&max_cnt,&athread);
				if(!ret) {
					strcpy(outp,"E02");
					break;
				}
				if(max_cnt==0) {
					strcpy(outp,"E02");
					break;
				}
				if(max_cnt>QM_MAXTHREADS) max_cnt = QM_MAXTHREADS;

				optr = reserve_qmheader(outp);
				if(first) rthread = 0;
				else rthread = athread;

				done = 0;
				for(i=0;i<max_cnt;i++) {
					rthread = gdbstub_getnextthread(rthread);
					if(rthread<=0) {
						done = 1;
						break;
					}
					optr = packqmthread(optr,rthread);
				}
				*optr = 0;
				packqmheader(outp,i,done,athread);
			}
			break;
		default:
			if(memcmp(&inp[1],"Offsets",8)==0) {
				char *t,*d,*b;

				optr = outp;
				if(!gdbstub_getoffsets(&t,&d,&b)) break;

				*optr++ = 'T';
				*optr++ = 'e';
				*optr++ = 'x';
				*optr++ = 't';
				*optr++ = '=';

				optr = int2vhstr(optr, (s32)t);

				*optr++ = ';';
				*optr++ = 'D';
				*optr++ = 'a';
				*optr++ = 't';
				*optr++ = 'a';
				*optr++ = '=';

				optr = int2vhstr(optr, (s32)d);

				*optr++ = ';';
				*optr++ = 'B';
				*optr++ = 's';
				*optr++ = 's';
				*optr++ = '=';

				optr = int2vhstr(optr, (s32)b);

				*optr++ = ';';
				*optr++ = 0;
			}
			break;
	}
}

static s32 gdbstub_setthreadregs(s32 thread,frame_context *frame)
{
	return 1;
}

static s32 gdbstub_getthreadregs(s32 thread,frame_context *frame)
{
	lwp_cntrl *th;

	th = gdbstub_indextoid(thread);
	if(th) {
		memcpy(frame->GPR,th->context.GPR,(32*4));
		memcpy(frame->FPR,th->context.FPR,(32*8));
		frame->SRR0 = th->context.LR;
		frame->SRR1 = th->context.MSR;
		frame->CR = th->context.CR;
		frame->LR = th->context.LR;
		frame->CTR = th->context.CTR;
		frame->XER = th->context.XER;
		frame->FPSCR = th->context.FPSCR;
		return 1;
	}
	return 0;
}

static void gdbstub_report_exception(frame_context *frame,s32 thread)
{
	u8 *ptr;
	s32 sigval;

	ptr = remcomOutBuffer;
	sigval = computeSignal(frame->EXCPT_Number);
	*ptr++ = 'T';
	*ptr++ = highhex(sigval);
	*ptr++ = lowhex(sigval);
	*ptr++ = highhex(SP_REGNUM);
	*ptr++ = lowhex(SP_REGNUM);
	*ptr++ = ':';
	ptr = mem2hstr(ptr,(u8*)&frame->GPR[1],4);
	*ptr++ = ';';
	*ptr++ = highhex(PC_REGNUM);
	*ptr++ = lowhex(PC_REGNUM);
	*ptr++ = ':';
	ptr = mem2hstr(ptr,(u8*)&frame->SRR0,4);
	*ptr++ = ';';

	*ptr++ = 't';
	*ptr++ = 'h';
	*ptr++ = 'r';
	*ptr++ = 'e';
	*ptr++ = 'a';
	*ptr++ = 'd';
	*ptr++ = ':';
	ptr = thread2vhstr(ptr,thread);
	*ptr++ = ';';

	*ptr++ = '\0';

}


void c_debug_handler(frame_context *frame)
{
	u8 *ptr;
	s32 addr,len;
	s32 thread,current_thread;
	s32 host_has_detached;
	frame_context *regptr;

	thread = gdbstub_getcurrentthread();
	current_thread = thread;

	if(dbg_listensock>=0 && (dbg_datasock<0 || dbg_active==0))
		dbg_datasock = tcpip_accept(dbg_listensock);
	if(dbg_datasock<0) return;
	else tcpip_starttimer(dbg_datasock);

	if(dbg_active) {
		gdbstub_report_exception(frame,thread);
		putpacket(remcomOutBuffer);
	}

	if(frame->SRR0==(u32)__breakinst) frame->SRR0 += 4;

	host_has_detached = 0;
	while(!host_has_detached) {
		remcomOutBuffer[0]= 0;
		getpacket(remcomInBuffer);
		switch(remcomInBuffer[0]) {
			case '?':
				gdbstub_report_exception(frame,thread);
				break;
			case 'D':
				dbg_instep = 0;
				dbg_active = 0;
				frame->SRR1 &= ~MSR_SE;
				strcpy(remcomOutBuffer,"OK");
				host_has_detached = 1;
				break;
			case 'g':
				regptr = frame;
				ptr = remcomOutBuffer;
				if(current_thread!=thread) regptr = &current_thread_registers;

				ptr = mem2hstr(ptr,(u8*)regptr->GPR,32*4);
				ptr = mem2hstr(ptr,(u8*)regptr->FPR,32*8);
				ptr = mem2hstr(ptr,(u8*)&regptr->SRR0,4);
				ptr = mem2hstr(ptr,(u8*)&regptr->SRR1,4);
				ptr = mem2hstr(ptr,(u8*)&regptr->CR,4);
				ptr = mem2hstr(ptr,(u8*)&regptr->LR,4);
				ptr = mem2hstr(ptr,(u8*)&regptr->CTR,4);
				ptr = mem2hstr(ptr,(u8*)&regptr->XER,4);
				ptr = mem2hstr(ptr,(u8*)&regptr->FPSCR,4);
				break;
			case 'm':
				ptr = &remcomInBuffer[1];
				if(hexToInt(&ptr,&addr) && ((addr&0xC0000000)==0xC0000000 || (addr&0xC0000000)==0x80000000)
					&& *ptr++==','
					&& hexToInt(&ptr,&len) && len<=((BUFMAX - 4)/2))
					mem2hstr(remcomOutBuffer,(void*)addr,len);
				else
					strcpy(remcomOutBuffer,"E00");
				break;
			case 'q':
				process_query(remcomInBuffer,remcomOutBuffer,thread);
				break;
			case 'c':
				dbg_instep = 0;
				dbg_active = 1;
				frame->SRR1 &= ~MSR_SE;
				tcpip_stoptimer(dbg_datasock);
				goto exit;
			case 's':
				dbg_instep = 1;
				dbg_active = 1;
				frame->SRR1 |= MSR_SE; 
				tcpip_stoptimer(dbg_datasock);
				goto exit;
			case 'z':
				{
					s32 ret,type,len;
					u8 *addr;

					ret = parsezbreak(remcomInBuffer,&type,&addr,&len);
					if(!ret) {
						strcpy(remcomOutBuffer,"E01");
						break;
					}
					if(type!=0) break;

					if(len<4) {
						strcpy(remcomOutBuffer,"E02");
						break;
					}

					ret = remove_bp(addr);
					if(!ret) {
						strcpy(remcomOutBuffer,"E03");
						break;
					}
					strcpy(remcomOutBuffer,"OK");
				}
				break;
			case 'H':
				if(remcomInBuffer[1]!='g') break;
				{
					s32 tmp,ret;

					if(vhstr2thread(&remcomInBuffer[2],&tmp)==NULL) {
						strcpy(remcomOutBuffer,"E01");
						break;
					}
					if(!tmp) tmp = thread;
					if(tmp==current_thread) {
						strcpy(remcomOutBuffer,"OK");
						break;
					}

					if(current_thread!=thread) ret = gdbstub_setthreadregs(current_thread,&current_thread_registers);
					if(tmp!=thread) {
						ret = gdbstub_getthreadregs(tmp,&current_thread_registers);
						if(!ret) {
							strcpy(remcomOutBuffer,"E02");
							break;
						}
					}
					current_thread= tmp;
					strcpy(remcomOutBuffer,"OK");
				}
				break;
			case 'T':
				{
					s32 tmp;

					if(vhstr2thread(&remcomInBuffer[1],&tmp)==NULL) {
						strcpy(remcomOutBuffer,"E01");
						break;
					}
					if(gdbstub_indextoid(tmp)==NULL) strcpy(remcomOutBuffer,"E02");
					else strcpy(remcomOutBuffer,"OK");
				}
				break;
			case 'Z':
				{
					s32 ret,type,len;
					u8 *addr;

					ret = parsezbreak(remcomInBuffer,&type,&addr,&len);
					if(!ret) {
						strcpy(remcomOutBuffer,"E01");
						break;
					}
					if(type!=0) {
						strcpy(remcomOutBuffer,"E02");
						break;
					}
					if(len<4) {
						strcpy(remcomOutBuffer,"E03");
						break;
					}

					ret = insert_bp(addr);
					if(!ret) {
						strcpy(remcomOutBuffer,"E04");
						break;
					}
					strcpy(remcomOutBuffer,"OK");
				}
				break;
		}
		putpacket(remcomOutBuffer);
	}
	tcpip_close(dbg_datasock);
	dbg_datasock = -1;
exit:
	return;
}

void _break(void)
{
	if(!dbg_initialized) return;
	__asm__ __volatile__ (".globl __breakinst\n\
						   __breakinst: .long 0x7d821008");
}

