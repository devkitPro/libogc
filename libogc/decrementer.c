#include <gctypes.h>
#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"
#include "context.h"

//#define _DECEX_DEBUG

extern u8 dechandler_start[],dechandler_end[];

extern void __exception_load(u32,void *,u32,void *);
#ifdef _DECEX_DEBUG
extern int printk(const char *fmt,...);
#endif
void __decrementer_init()
{
#ifdef _DECEX_DEBUG
	printf("__decrementer_init()\n\n");
#endif
	__exception_load(EX_DEC,dechandler_start,(dechandler_end-dechandler_start),NULL);
}

void c_decrementer_handler()
{
#ifdef _DECEX_DEBUG
	printk("c_decrementer_handler(%d)\n",_wd_ticks_since_boot);
#endif
	__lwp_wd_tickle_ticks();
}
