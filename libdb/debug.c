#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include "asm.h"
#include "processor.h"
#include "spinlock.h"
#include "lwp.h"
#include "context.h"
#include "cache.h"
#include "network.h"
#include "debug.h"

//#define _DBG_DEBUG

#define BUFMAX		2048		//we take the same as in ppc-stub.c

#define BPCODE		0x7d821008
#define STR_BPCODE	"7d821008"

#define STACKSIZE	32768

typedef struct _dbginfo {
	u32 sock;
} dbginfo;

static u8 remcomInBuffer[BUFMAX];
static u8 remcomOutBuffer[BUFMAX];

static u32 curr_thread = -1;
static lwp_t hdebugger;
static u32 debug_stack[STACKSIZE/sizeof(u32)];

static u32 fault_jmp_buf[100];
static dbginfo gdb_info;

static spinlock_t bp_lock = SPIN_LOCK_UNLOCKED;

static const char hexchars[]="0123456789abcdef";


u32 c_debug_handler(frame_context *);
static void* debugger(void*);

extern int printk(const char *fmt,...);
extern void _cpu_context_save_ex(void *);

extern u8 text_mem_start[],data_mem_start[],bss_mem_start[];
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
	if(gdb_setjmp(fault_jmp_buf)==0) {
		while (count-- > 0) {
			ch = *mem++;
			*buf++ = hexchars[ch >> 4];
			*buf++ = hexchars[ch & 0xf];
		}
		*buf = 0;
	}
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

static u32 getDebugChar()
{
	u8 ch;
	s32 ret;
	ret = net_recv(gdb_info.sock,&ch,1,0);
#ifdef _DBG_DEBUG
	printf("getDebugChar(%c,%d)\n",ch,ret);
#endif
	if(ret<=0) return -1;
	return ch;
}

static u32 putDebugStr(u8 *str)
{
	u32 len;
	if(!str) return -1;

	len = strlen(str);
	if(net_send(gdb_info.sock,str,len,0)<=0) return -1;
#ifdef _DBG_DEBUG
	printf("putDebugStr(%s)\n",str);
#endif
	return len;
}

static u32 putDebugChar(u8 ch)
{
	if(net_send(gdb_info.sock,&ch,1,0)<=0) return -1;
#ifdef _DBG_DEBUG
	printf("putDebugChar(%c)\n",ch);
#endif
	return ch;
}

static u32 putpacket(u8 *buffer)
{
	u8 checksum;
	u32 cnt,i,recv;
	u8 ch,out[1024];
#ifdef _DBG_DEBUG
	printf("putpacket(%s) enter\n",buffer);
#endif
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

#ifdef _DBG_DEBUG
		printf("putpacket(%s) out\n",out);
#endif
		if(putDebugStr(out)==-1) return -1;
		
		while((recv=getDebugChar())!=-1 && ((recv&0x7f)=='\n' || (recv&0x7f)=='\r'));
		if(recv==-1) return -1;
#ifdef _DBG_DEBUG
		printf("putpacket(%c) recv\n",recv&0x7f);
#endif
	} while((recv&0x7f)!='+');
#ifdef _DBG_DEBUG
	printf("putpacket(%p,%d) leave\n",buffer,cnt);
#endif
	return cnt;
}

static u32 getpacket(u8 *buffer)
{
	u8 checksum;
	u8 xmitcsum;
	u32 i,cnt;
	u32 ch,nib;
#ifdef _DBG_DEBUG
	printf("getpacket(%p) enter\n",buffer);
#endif
	do {
		do {
			ch = getDebugChar();
			if(ch==-1) return -1;
		} while((ch&0x7f)!='$');
		
		cnt = 0;
		checksum = 0;
		xmitcsum = -1;
		while(cnt<BUFMAX) {
			if((ch=getDebugChar())==-1) return -1;

			ch &= 0x7f;
			if(ch=='#') break;
			
			checksum = checksum+ch;
			buffer[cnt] = ch;
			cnt++;
		}
		if(cnt>=BUFMAX) continue;

		buffer[cnt] = 0;
		if(ch=='#') {
			if((nib=getDebugChar())==-1) return -1;
			xmitcsum = hex(nib&0x7f)<<4;
			if((nib=getDebugChar())==-1) return -1;
			xmitcsum |= hex(nib&0x7f);

			if(checksum!=xmitcsum) {
				if(putDebugChar('-')==-1) return -1;
			} else {
				if(putDebugChar('+')==-1) return -1;
				if(buffer[2]==':') {
					if(putDebugChar(buffer[0])==-1 ||
						putDebugChar(buffer[1])==-1) return -1;
					
					cnt = strlen(buffer);
					for(i=3;i<=cnt;i++)
						buffer[i-3] = buffer[i];
				}
			}
		}

	} while(checksum!=xmitcsum);
//#ifdef _DBG_DEBUG
	printf("getpacket(%s,%d) leave\n",buffer,cnt);
//#endif
	return cnt;
}

extern void __exception_load(u32,void *,u32,void*);

void DEBUG_Init(u32 port)
{
	u32 s;
	struct sockaddr_in locAddr;

	if(net_init()==SOCKET_ERROR) return;

	s = net_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(s==INVALID_SOCKET) return;

	locAddr.sin_family = AF_INET;
	locAddr.sin_port = htons(port);
	locAddr.sin_addr.s_addr = INADDR_ANY;
	if(net_bind(s,(struct sockaddr*)&locAddr,sizeof(locAddr))==SOCKET_ERROR) return;
	if(net_listen(s,20)!=SOCKET_ERROR) {
		if(LWP_CreateThread(&hdebugger,debugger,&s,debug_stack,STACKSIZE,210)>0) {
			__exception_load(EX_PRG,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
			__exception_load(EX_TRACE,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
			__exception_load(EX_IBAR,debug_handler_start,(debug_handler_end-debug_handler_start),debug_handler_patch);
		}
	}

}

#define GEKKO_MAX_BP	1024
static struct bp_entry {
	u8 *mem;
	u32 val;
	struct bp_entry *next;
} bp_entries[GEKKO_MAX_BP];

static struct bp_entry *p_bpentries = NULL;

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

static s32 computeSignal(s32 excpt)
{
	struct hard_trap_info *ht;
	for(ht = hard_trap_info;ht->tt && ht->signo;ht++) {
		if(ht->tt==excpt) return ht->signo;
	}
	return SIGHUP;
}

#define PC_REGNUM		64
#define SP_REGNUM		1

u32 c_debug_handler(frame_context *pCtx)
{
	u32 i,addr;
	u32 signo,len;
	u8 *ptr;
	u32 exc_vec = pCtx->EXCPT_Number;

#ifdef _DBG_DEBUG
	printk("c_debug_handler(%d,%d)\n",signo,exc_vec);
#endif

	if(pCtx->SRR0 && *((u32*)pCtx->SRR0)==BPCODE) pCtx->SRR0 += 4;

	signo = computeSignal(exc_vec);
	
	while(1) {
		remcomOutBuffer[0] = 0;

		if(getpacket(remcomInBuffer)==-1) goto cleanup;
		switch(remcomInBuffer[0]) {
			case '?':
				remcomOutBuffer[0] = 'S';
				remcomOutBuffer[1] = hexchars[signo>>4];
				remcomOutBuffer[2] = hexchars[signo&0xf];
				remcomOutBuffer[3] = 0;
				break;
			case 'g':
				ptr = remcomOutBuffer;
				ptr = mem2hex((u8*)pCtx->GPR,ptr,32*4);
				for(i=0;i<(32*8*2);i++) *ptr++= '0';
				ptr = mem2hex((u8*)&pCtx->SRR0,ptr,4);
				ptr = mem2hex((u8*)&pCtx->MSR,ptr,4);
				ptr = mem2hex((u8*)&pCtx->CR,ptr,4);
				ptr = mem2hex((u8*)&pCtx->LR,ptr,4);
				ptr = mem2hex((u8*)&pCtx->CTR,ptr,4);
				ptr = mem2hex((u8*)&pCtx->XER,ptr,4);
				break;
			case 'H':
				if(remcomInBuffer[1]=='g') {
					if(remcomInBuffer[2]=='-') 
						curr_thread = -strtoul(remcomInBuffer+3,0,16);
					else
						curr_thread = strtoul(remcomInBuffer+2,0,16);
					strcpy(remcomOutBuffer,"OK");
				}
				break;
			case 'q':
				break;
			case 'm':
				ptr = &remcomInBuffer[1];
				if(hexToInt(&ptr,&addr) && *ptr++==',' && hexToInt(&ptr,&len)) {
					if(mem2hex((u8*)addr,remcomOutBuffer,len)) break;
					strcpy(remcomOutBuffer,"E03");
				} else
					strcpy(remcomOutBuffer,"E01");
				break;
			case 'k':
				ptr = &remcomInBuffer[1];
				/*
				if(hexToInt(&ptr,&addr))
					pCtx->SRR0 = addr;
				ICFlashInvalidate();
				*/
				return 1;
			case 's':
				ICFlashInvalidate();
				pCtx->MSR |= MSR_SE;
				return 1;
			case 'Z':
			case 'z':
				{
					u32 type = remcomInBuffer[1] - '0';
					u32 address = strtoul(remcomInBuffer+3,0,16);
					u32 res = 0;
					if(type!=0) {
						strcpy(remcomOutBuffer,"ERROR");
						break;
					}
					
					spin_lock(&bp_lock);
					if(remcomInBuffer[0]=='Z') 
						res = insert_bp((u8*)address);
					else {
						res = 0;
						remove_bp((u8*)address);
					}
					spin_unlock(&bp_lock);
					if(res)
						strcpy(remcomOutBuffer,"ERROR");
					else
						strcpy(remcomOutBuffer,"OK");
				}
				break;
		}
		if(putpacket(remcomOutBuffer)==-1) goto cleanup;
	}
cleanup:
	return -1;
}

static void* debugger(void *arg)
{
	u32 dsock,ret;
	frame_context cur;
	struct sockaddr_in aServerAddr;
	u32 sock = *(u32*)arg;
	socklen_t nAddrSize = sizeof(struct sockaddr_in);
#ifdef _DBG_DEBUG
	printf("debugger(%d) started\n",sock);
#endif
	while(1) {
		dsock = net_accept(sock,(struct sockaddr*)&aServerAddr,&nAddrSize);
#ifdef _DBG_DEBUG
		printf("debugger(%d) dsock\n",dsock);
#endif
		if(dsock!=INVALID_SOCKET) {
			gdb_info.sock = dsock;
			memset(&cur,0,sizeof(frame_context));
			_cpu_context_save_ex(&cur);
			ret = c_debug_handler(&cur);
			net_close(dsock);
#ifdef _DBG_DEBUG
			printf("handle_exception(%d) return\n",ret);
#endif
		}
	}
	return 0;
}
