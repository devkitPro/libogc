#include <gctypes.h>
#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"
#include "context.h"

//#define _DECEX_DEBUG

static u32 dec_value = 0;

extern u8 dechandler_start[],dechandler_end[];

extern void __exception_load(u32,void *,u32,void *);
#ifdef _DECEX_DEBUG
extern int printk(const char *fmt,...);
#endif
void __decrementer_init()
{
	u32 busfreq = *(u32*)0x800000f8;
#ifdef _DECEX_DEBUG
	printf("__decrementer_init()\n\n");
#endif
	dec_value = (busfreq/4000)*10;		//(busfreq/4000)*(ms_per_tslice/1000)
	__exception_load(EX_DEC,dechandler_start,(dechandler_end-dechandler_start),NULL);
	mtdec(dec_value);
}

void c_decrementer_handler()
{
#ifdef _DECEX_DEBUG
	printk("c_decrementer_handler(%d)\n",_wd_ticks_since_boot);
#endif
	mtdec(dec_value);
	_wd_ticks_since_boot++;
	__lwp_wd_tickle_ticks();
}
