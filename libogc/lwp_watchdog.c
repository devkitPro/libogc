#include <stdlib.h>
#include "asm.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"

vu32 _wd_sync_level;
vu32 _wd_sync_count;
u32 _wd_ticks_since_boot;

lwp_queue _wd_ticks_queue;
lwp_queue _wd_secs_queue;

extern long long gettime();

void __lwp_watchdog_settimer(wd_cntrl *wd)
{
	s64 curr_time;

	curr_time = gettime();
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
	u32 level;
	wd_cntrl *after;
	u32 isr_nest_level;
	u32 delta_interval;

	isr_nest_level = __lwp_isr_in_progress();
	wd->state = LWP_WD_INSERTED;
	
	_wd_sync_count++;
restart:
	delta_interval = wd->init_interval;
	
	_CPU_ISR_Disable(level);
	for(after=__lwp_wd_first(header);;after=__lwp_wd_next(after)) {
		if(delta_interval==0 || !__lwp_wd_next(after)) break;

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
	wd->start_time = _wd_ticks_since_boot;

exit_insert:
	_wd_sync_level = isr_nest_level;
	_wd_sync_count--;
	_CPU_ISR_Restore(level);
}

u32 __lwp_wd_remove(wd_cntrl *wd)
{
	u32 level;
	u32 prev_state;
	wd_cntrl *next;

	_CPU_ISR_Disable(level);
	
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
			if(__lwp_wd_next(next)) next->delta_interval += wd->delta_interval;
			if(_wd_sync_count) _wd_sync_level = __lwp_isr_in_progress();
			__lwp_queue_extractI(&wd->node);
			break;
	}
	wd->stop_time = _wd_ticks_since_boot;
	_CPU_ISR_Restore(level);
	return prev_state;
}

void __lwp_wd_tickle(lwp_queue *queue)
{
	wd_cntrl *wd;

	if(__lwp_queue_isempty(queue)) return;
	
	wd = __lwp_wd_first(queue);
	wd->delta_interval--;
	if(wd->delta_interval!=0) return;

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

void __lwp_wd_adjust(lwp_queue *queue,u32 dir,u32 interval)
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
