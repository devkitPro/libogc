#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include <tuxedo/ppc/exception.h>
#include <tuxedo/ppc/context.h>

#include "asm.h"
#include "processor.h"
#include "lwp.h"
#include "cache.h"
#include "video.h"
#include "ogcsys.h"

#include "tcpip.h"
#include "geckousb.h"
#include "debug_if.h"
#include "debug_supp.h"

#define GEKKO_MAX_BP	256

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

static struct dbginterface *current_device = NULL;

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

const char hexchars[]="0123456789abcdef";

const static struct hard_trap_info {
	u8 tt;
	u8 signo;
} hard_trap_info[] = {
	{PPC_EXCPT_MCHK,SIGSEGV},/* Machine Check */
	{PPC_EXCPT_DSI,SIGSEGV},		/* Adress Error(store) DSI */
	{PPC_EXCPT_ISI,SIGBUS},		/* Instruction Bus Error ISI */
	{PPC_EXCPT_IRQ,SIGINT},		/* Interrupt */
	{PPC_EXCPT_ALIGN,SIGBUS},		/* Alignment */
	{PPC_EXCPT_UNDEF,SIGTRAP},		/* Breakpoint Trap */
	{PPC_EXCPT_FPU,SIGFPE},			/* FPU unavail */
	{PPC_EXCPT_DECR,SIGALRM},		/* Decrementer */
	{PPC_EXCPT_SYSCALL,SIGSYS},	/* System Call */
	{PPC_EXCPT_TRACE,SIGTRAP},		/* Singel-Step/Watch */
	{PPC_EXCPT_BKPT,SIGTRAP},		/* Instruction Address Breakpoint (GEKKO) */
	{0,0}					/* MUST be the last */
};

static struct bp_entry {
	u32 *address;
	u32 instr;
	struct bp_entry *next;
} bp_entries[GEKKO_MAX_BP];

static struct bp_entry *p_bpentries = NULL;

void __breakinst(void);
static void c_debug_handler(unsigned exid, PPCContext *ctx);

extern const char *tcp_localip;
extern const char *tcp_netmask;
extern const char *tcp_gateway;

static __inline__ void bp_init(void)
{
	s32 i;

	for(i=0;i<GEKKO_MAX_BP;i++) {
		bp_entries[i].address = NULL;
		bp_entries[i].instr = 0xffffffff;
		bp_entries[i].next = NULL;
	}
}

static s32 hex(char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

static s32 hexToInt(char **ptr, s32 *ival)
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

static s32 computeSignal(unsigned excpt)
{
	const struct hard_trap_info *ht;
	for(ht = hard_trap_info;ht->tt && ht->signo;ht++) {
		if(ht->tt==excpt) return ht->signo;
	}
	return SIGHUP;
}

static u32 insert_bp(void *mem)
{
	u32 i;
	struct bp_entry *p;

	for(i=0;i<GEKKO_MAX_BP;i++) {
		if(bp_entries[i].address == NULL) break;
	}
	if(i==GEKKO_MAX_BP) return 0;
	
	p = &bp_entries[i];
	p->next = p_bpentries;
	p->address = mem;
	p_bpentries = p;

	p->instr = *(p->address);
	*(p->address) = BPCODE;
	
	DCStoreRangeNoSync((void*)((u32)mem&~0x1f),32);
	ICInvalidateRange((void*)((u32)mem&~0x1f),32);
	_sync();

	return 1;
}

static u32 remove_bp(void *mem)
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
	f->address = NULL;

	DCStoreRangeNoSync((void*)((u32)mem&~0x1f),32);
	ICInvalidateRange((void*)((u32)mem&~0x1f),32);
	_sync();

	return 1;
}

static char getdbgchar(void)
{
	char ch = 0;
	s32 len = 0;

	len = current_device->read(current_device,&ch,1);

	return (len>0)?ch:0;
}

static void putdbgchar(char ch)
{
	current_device->write(current_device,&ch,1);
}

static void putdbgstr(const char *str)
{
	current_device->write(current_device,str,strlen(str));
}

static void putpacket(const char *buffer)
{
	u8 recv;
	u8 chksum,ch;
	char *ptr;
	const char *inp;
	static char outbuf[2048];

	do {
		inp = buffer;
		ptr = outbuf;
		
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

static void getpacket(char *buffer)
{
	char ch;
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
		case 'f':
			if(!strncmp(&inp[2],"ThreadInfo",10)) {
				optr = outp;
				*optr++ = 'm';
				s32 rthread = gdbstub_getnextthread(0);
				optr = thread2vhstr(optr,rthread);
				while ((rthread = gdbstub_getnextthread(rthread))) {
					*optr++=',';
					optr = thread2vhstr(optr,rthread);
				};
				*optr = 0;
			}
			break;
		case 's':
			if(!strncmp(&inp[2],"ThreadInfo",10)) {
				optr = outp;
				*optr++ = 'l';
				*optr = 0;
			}
			break;
		default:
			break;
	}
}

static void gdbstub_report_exception(unsigned exid, PPCContext *frame,s32 thread)
{
	s32 sigval;
	char *ptr;

	ptr = remcomOutBuffer;
	sigval = computeSignal(exid);
	*ptr++ = 'T';
	*ptr++ = highhex(sigval);
	*ptr++ = lowhex(sigval);
	*ptr++ = highhex(SP_REGNUM);
	*ptr++ = lowhex(SP_REGNUM);
	*ptr++ = ':';
	ptr = mem2hstr(ptr,(char*)&frame->gpr[1],4);
	*ptr++ = ';';
	*ptr++ = highhex(PC_REGNUM);
	*ptr++ = lowhex(PC_REGNUM);
	*ptr++ = ':';
	ptr = mem2hstr(ptr,(char*)&frame->pc,4);
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


void c_debug_handler(unsigned exid, PPCContext *frame)
{
	char *ptr;
	s32 addr,len;
	s32 thread,current_thread;
	s32 host_has_detached;
	PPCContext *regptr, *thrctx;

	thread = gdbstub_getcurrentthread();
	current_thread = thread;

	if(current_device->open(current_device)<0) return;

	if(dbg_active) {
		gdbstub_report_exception(exid,frame,thread);
		putpacket(remcomOutBuffer);
	}

	if(frame->pc==(u32)__breakinst) frame->pc += 4;

	host_has_detached = 0;
	while(!host_has_detached) {
		remcomOutBuffer[0]= 0;
		getpacket(remcomInBuffer);
		switch(remcomInBuffer[0]) {
			case '?':
				gdbstub_report_exception(exid,frame,thread);
				break;
			case 'D':
				dbg_instep = 0;
				dbg_active = 0;
				frame->msr &= ~MSR_SE;
				strcpy(remcomOutBuffer,"OK");
				host_has_detached = 1;
				break;
			case 'k':
				dbg_instep = 0;
				dbg_active = 0;
				frame->msr &= ~MSR_SE;
				frame->pc = 0x80001800;
				host_has_detached = 1;
				goto exit;
			case 'g':
				thrctx = &gdbstub_indextoid(current_thread)->ctx;
				regptr = frame;
				ptr = remcomOutBuffer;
				if(current_thread!=thread) regptr = thrctx;

				ptr = mem2hstr(ptr,(char*)regptr->gpr,32*4);
				ptr = mem2hstr(ptr,(char*)thrctx->fpr,32*8);
				ptr = mem2hstr(ptr,(char*)&regptr->pc,4);
				ptr = mem2hstr(ptr,(char*)&regptr->msr,4);
				ptr = mem2hstr(ptr,(char*)&regptr->cr,4);
				ptr = mem2hstr(ptr,(char*)&regptr->lr,4);
				ptr = mem2hstr(ptr,(char*)&regptr->ctr,4);
				ptr = mem2hstr(ptr,(char*)&regptr->xer,4);
				ptr = mem2hstr(ptr,(char*)&thrctx->fpscr,4);
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
				frame->msr &= ~MSR_SE;
				current_device->wait(current_device);
				goto exit;
			case 's':
				dbg_instep = 1;
				dbg_active = 1;
				frame->msr |= MSR_SE;
				current_device->wait(current_device);
				goto exit;
			case 'z':
				{
					s32 ret,type;
					u32 len;
					char *addr;

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
				if(remcomInBuffer[1]=='g')
				{
					s32 tmp;

					if(vhstr2thread(&remcomInBuffer[2],&tmp)==NULL) {
						strcpy(remcomOutBuffer,"E01");
						break;
					}
					if(!tmp) tmp = thread;

					current_thread= tmp;
				}
				strcpy(remcomOutBuffer,"OK");
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
					s32 ret,type;
					u32 len;
					char *addr;

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
	current_device->close(current_device);
exit:

	// Ensure FPU access is relinquished for non-exception contexts
	if (frame->msr & MSR_RI) {
		frame->msr &= ~MSR_FP;
	}

}

void _break(void)
{
	if(!dbg_initialized) return;
	__asm__ __volatile__ (".globl __breakinst\n\
						   __breakinst: .long 0x7d821008");
}

void DEBUG_Init(s32 device_type,s32 channel_port)
{
	struct uip_ip_addr localip,netmask,gateway;

	UIP_LOG("DEBUG_Init()\n");

	bp_init();

	if(device_type==GDBSTUB_DEVICE_USB) {
		current_device = usb_init(channel_port);
	} else {
		localip.addr = uip_ipaddr((const u8_t*)tcp_localip);
		netmask.addr = uip_ipaddr((const u8_t*)tcp_netmask);
		gateway.addr = uip_ipaddr((const u8_t*)tcp_gateway);

		current_device = tcpip_init(&localip,&netmask,&gateway,(u16)channel_port);
	}

	if(current_device!=NULL) {
		PPCExcptCurPanicFn = c_debug_handler;

		dbg_initialized = 1;

	}
}

