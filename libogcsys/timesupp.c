#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "lwp.h"
#include "lwp_threadq.h"
#include "timesupp.h"
#include "exi.h"

#define TB_BUS_CLOCK		162000000u
#define TB_CORE_CLOCK		486000000u
#define TB_TIMER_CLOCK		(TB_BUS_CLOCK/4000)			//4th of the bus frequency

#define TB_REQ				250
#define TB_SUCCESSFUL		0

#define ticks_to_cycles(ticks)		(((ticks)*((TB_CORE_CLOCK*2)/TB_TIMER_CLOCK))/2)
#define ticks_to_secs(ticks)		((ticks)/TB_TIMER_CLOCK)
#define ticks_to_millisecs(ticks)	((ticks)/(TB_TIMER_CLOCK/1000))
#define ticks_to_microsecs(ticks)	(((ticks)*8)/(TB_TIMER_CLOCK/125000))
#define ticks_to_nanosecs(ticks)	(((ticks)*8000)/(TB_TIMER_CLOCK/125000))

#define secs_to_ticks(sec)			((sec)*TB_TIMER_CLOCK)
#define millisecs_to_ticks(msec)	((msec)*(TB_TIMER_CLOCK/1000))
#define microsecs_to_ticks(usec)	(((usec)*(TB_TIMER_CLOCK/125000))/8)
#define nanosecs_to_ticks(nsec)		(((nsec)*(TB_TIMER_CLOCK/125000))/8000)

#define diff_ticks(tick0,tick1)		((signed long long)tick0 - (signed long long)tick1)

/* time variables */
static lwpq_t time_exi_wait;
static lwp_thrqueue timedwait_queue;

unsigned long _DEFUN(gettick,(),
						  _NOARGS)
{
	__asm__ __volatile__ (
		"1:	mftb	3\n\
		    blr"
			: : : "memory");
	return 0;
}

unsigned long long _DEFUN(gettime,(),
						  _NOARGS)
{
	__asm__ __volatile__ (
		"1:	mftbu	3\n\
		    mftb	4\n\
		    mftbu	5\n\
			cmpw	3,5\n\
			bne		1b\n\
			blr"
			: : : "memory");
	return 0;
}

void _DEFUN(settime,(t),
			unsigned long long t)
{
	__asm__ __volatile__ (
		"mtspr	284,4\n\
		 mtspr	285,3\n\
	     blr"
		 : : : "memory");
}

u32 diff_sec(unsigned long long start,unsigned long long end)
{
	u32 upper,lower;

	upper = ((u32)(end>>32))-((u32)(start>>32));
	if((u32)start > (u32)end)
		upper--;

	lower = ((u32)end)-((u32)start);
	return ((upper*((u32)0x80000000/(TB_TIMER_CLOCK*500)))+(lower/(TB_TIMER_CLOCK*1000)));
}

u32 diff_msec(unsigned long long start,unsigned long long end)
{
	u32 upper,lower;

	upper = ((u32)(end>>32))-((u32)(start>>32));
	if((u32)start > (u32)end)
		upper--;

	lower = ((u32)end)-((u32)start);
	return ((upper*((u32)0x80000000/(TB_TIMER_CLOCK/2)))+(lower/(TB_TIMER_CLOCK)));
}

u32 diff_usec(unsigned long long start,unsigned long long end)
{
	u32 upper,lower;

	upper = ((u32)(end>>32))-((u32)(start>>32));
	if((u32)start > (u32)end)
		upper--;

	lower = ((u32)end)-((u32)start);
	return ((upper*((u32)0x80000000/(TB_TIMER_CLOCK/2000)))+(lower/(TB_TIMER_CLOCK/1000)));
}

void __timesystem_init()
{
	__lwp_threadqueue_init(&timedwait_queue,LWP_THREADQ_MODEPRIORITY,LWP_STATES_LOCALLY_BLOCKED,6);
	LWP_InitQueue(&time_exi_wait);
}


unsigned int timespec_to_interval(const struct timespec *time)
{
	u32 ticks;

	ticks = (time->tv_sec*TB_USPERSEC)/TB_USPERTICK;
	ticks += (time->tv_nsec/TB_NSPERUS)/TB_USPERTICK;

	if(ticks) return ticks;

	return 1;
}

// this function spins till timeout is reached
void _DEFUN(udelay,(us),
			int us)
{
	unsigned long long start, end;
	start = gettime();
	while (1)
	{
		end = gettime();
		if (diff_usec(start,end) >= us)
			break;
	}
}

unsigned int _DEFUN(nanosleep,(tb),
           struct timespec *tb)
{
	u32 level,timeout;
	lwp_cntrl *exec;

	__lwp_thread_dispatchdisable();

	exec = _thr_executing;
	exec->wait.ret_code = TB_SUCCESSFUL;

	if(tb->tv_nsec<TB_USPERTICK) {
		udelay(tb->tv_nsec);
		return TB_SUCCESSFUL;
	}

	timeout = timespec_to_interval(tb);

	_CPU_ISR_Disable(level);
	__lwp_threadqueue_csenter(&timedwait_queue);
	exec->wait.queue = &timedwait_queue;
	exec->id = TB_REQ;
	exec->wait.ret_arg = NULL;
	exec->wait.ret_arg_1 = NULL;
	_CPU_ISR_Restore(level);
	__lwp_threadqueue_enqueue(&timedwait_queue,timeout);

	__lwp_thread_dispatchenable();
	return TB_SUCCESSFUL;
}

static u32 __time_exi_unlock()
{
	LWP_WakeThread(time_exi_wait);
	return 1;
}

static void __time_exi_wait()
{
	u32 ret,level;

	_CPU_ISR_Disable(level);
	do {
		if((ret=EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,__time_exi_unlock))==1) break;
		LWP_SleepThread(time_exi_wait);
	}while(ret==0);
	_CPU_ISR_Restore(level);
}

time_t _DEFUN(time,(timer),
			  time_t *timer)
{
	time_t gctime = 0;
	u32 command;
	u32 SRAM[64];


	__time_exi_wait();

	command = 0x20000000;
	EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ);
	EXI_Imm(EXI_CHANNEL_0,&command,4,EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_Imm(EXI_CHANNEL_0,&gctime,4,EXI_READ,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_Deselect(EXI_CHANNEL_0);

	command = 0x20000100;
	EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ);
	EXI_Imm(EXI_CHANNEL_0,&command,4,EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,SRAM,64,EXI_READ);
	EXI_Deselect(EXI_CHANNEL_0);
	EXI_Unlock(EXI_CHANNEL_0);

	gctime += 946684800;
	gctime += SRAM[3];

	if (timer) *timer = gctime;

	return gctime;
}
