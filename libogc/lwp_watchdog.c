#include <stdlib.h>
#include <limits.h>
#include "asm.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"
#include "libogcsys/timesupp.h"

//#define _LWPWD_DEBUG

vu32 _wd_sync_level;
vu32 _wd_sync_count;
u32 _wd_ticks_since_boot;

lwp_queue _wd_ticks_queue;
lwp_queue _wd_secs_queue;

extern long long gettime();

static void __lwp_wd_settimer(wd_cntrl *wd)
{
	s64 now;
	u64 diff;
	s64 int_max = INT_MAX;
	s64 delta_interval = (s64)wd->delta_interval;
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_settimer(%lld)\n",delta_interval);
#endif
	now = gettime();
	diff = diff_ticks(now,wd->start_time);
	delta_interval -= diff;
	if(0<=-(delta_interval)) {
#ifdef _LWPWD_DEBUG
		printf("0<(min_ticks-delta_interval): __lwp_wd_settimer(0)\n");
#endif
		wd->delta_interval = 0;
		mtdec(0);
	} else if(delta_interval<=int_max) {
#ifdef _LWPWD_DEBUG
		printf("0<(max_ticks-delta_interval): __lwp_wd_settimer(%d)\n",(u32)delta_interval);
#endif
		wd->delta_interval -= diff;
		mtdec((u32)delta_interval);
	} else {
#ifdef _LWPWD_DEBUG
		printf("__lwp_wd_settimer(0x7fffffff)\n");
#endif
		wd->delta_interval -= (int_max+1);
		mtdec(INT_MAX);
	}
}

void __lwp_watchdog_init()
{
	_wd_sync_level = 0;
	_wd_sync_count = 0;
	_wd_ticks_since_boot = 0;

	__lwp_queue_init_empty(&_wd_ticks_queue);
	__lwp_queue_init_empty(&_wd_secs_queue);
}

void __lwp_wd_insert(lwp_queue *header,wd_cntrl *wd)
{
	s64 now;
	u32 level;
	wd_cntrl *after;
	u32 isr_nest_level;
	u64 delta_interval,diff;
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_insert()\n");
#endif
	isr_nest_level = __lwp_isr_in_progress();
	wd->state = LWP_WD_INSERTED;
	
	_wd_sync_count++;
restart:
	delta_interval = wd->init_interval;
	
	_CPU_ISR_Disable(level);
	now = gettime();
	for(after=__lwp_wd_first(header);;after=__lwp_wd_next(after)) {
		if(delta_interval==0 || !__lwp_wd_next(after)) break;
	
		diff = diff_ticks(now,after->start_time);
		after->start_time = now;
		after->delta_interval -= diff;
		if(delta_interval<after->delta_interval) {
			after->delta_interval -= delta_interval;
			break;
		}
		
		delta_interval -= after->delta_interval;
		_CPU_ISR_Flash(level);

		if(wd->state!=LWP_WD_INSERTED) goto exit_insert;
		if(_wd_sync_level>isr_nest_level) {
			_wd_sync_level = isr_nest_level;
			_CPU_ISR_Restore(level);
			goto restart;
		}
	}
	__lwp_wd_activate(wd);
	wd->delta_interval = delta_interval;
	__lwp_queue_insertI(after->node.prev,&wd->node);
	wd->start_time = now;
	__lwp_wd_settimer(__lwp_wd_first(header));

exit_insert:
	_wd_sync_level = isr_nest_level;
	_wd_sync_count--;
	_CPU_ISR_Restore(level);
}

u32 __lwp_wd_remove(wd_cntrl *wd)
{
	u32 level,has_next;
	u32 prev_state;
	s64 now;
	u64 diff;
	wd_cntrl *next;
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_remove(%p)\n",wd);
#endif
	_CPU_ISR_Disable(level);
	has_next = 0;
	now = gettime();
	diff = diff_ticks(now,wd->start_time);
	prev_state = wd->state;
	switch(prev_state) {
		case LWP_WD_INACTIVE:
			break;
		case  LWP_WD_INSERTED:
			wd->state = LWP_WD_INACTIVE;
			break;
		case LWP_WD_ACTIVE:
		case LWP_WD_REMOVE:
			wd->state = LWP_WD_INACTIVE;
			next = __lwp_wd_next(wd);
			if(wd->delta_interval>0) wd->delta_interval -= diff;
			if(__lwp_wd_next(next)) {
				has_next = 1;
				next->delta_interval += wd->delta_interval;
			}
			if(_wd_sync_count) _wd_sync_level = __lwp_isr_in_progress();
			__lwp_queue_extractI(&wd->node);
			if(has_next && !__lwp_wd_prev(next)) __lwp_wd_settimer(next);
			break;
	}
	_CPU_ISR_Restore(level);
	return prev_state;
}

void __lwp_wd_tickle(lwp_queue *queue)
{
	wd_cntrl *wd;
	s64 now;
	u64 diff;
	u64 max_ticks = (u64)0x80000000;

	if(__lwp_queue_isempty(queue)) return;

	wd = __lwp_wd_first(queue);
	now = gettime();
	diff = diff_ticks(now,wd->start_time);
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_tickle(%llu)\n",diff);
#endif
	if(diff<wd->delta_interval && wd->delta_interval<max_ticks) return;
	if(wd->delta_interval>=max_ticks) {
		wd->start_time = gettime();
		__lwp_wd_settimer(wd);
		return;
	}

	wd->delta_interval = 0;
	do {
		switch(__lwp_wd_remove(wd)) {
			case LWP_WD_ACTIVE:	
				wd->routine(wd->usr_data);
				break;
			case LWP_WD_INACTIVE:
				break;
			case LWP_WD_INSERTED:
				break;
			case LWP_WD_REMOVE:
				break;
		}
		wd = __lwp_wd_first(queue);
	} while(!__lwp_queue_isempty(queue) && wd->delta_interval==0);
}

void __lwp_wd_adjust(lwp_queue *queue,u32 dir,u64 interval)
{
	if(!__lwp_queue_isempty(queue)) {
		switch(dir) {
			case LWP_WD_BACKWARD:
				__lwp_wd_first(queue)->delta_interval += interval;
				break;
			case LWP_WD_FORWARD:
				while(interval) {
					if(interval<__lwp_wd_first(queue)->delta_interval) {
						__lwp_wd_first(queue)->delta_interval -= interval;
						break;
					} else {
						interval -= __lwp_wd_first(queue)->delta_interval;
						__lwp_wd_first(queue)->delta_interval = 1;
						__lwp_wd_tickle(queue);
						if(__lwp_queue_isempty(queue)) break;
					}
				}
				break;
		}
	}
}
