#ifndef __LWP_WATCHDOG_INL__
#define __LWP_WATCHDOG_INL__

static __inline__ void __lwp_wd_initialize(wd_cntrl *wd,wd_service_routine routine,void *usr_data)
{
	wd->state = LWP_WD_INACTIVE;
	wd->routine = routine;
	wd->usr_data = usr_data;
}

static __inline__ wd_cntrl* __lwp_wd_first(lwp_queue *queue)
{
	return (wd_cntrl*)queue->first;
}

static __inline__ wd_cntrl* __lwp_wd_last(lwp_queue *queue)
{
	return (wd_cntrl*)queue->last;
}

static __inline__ wd_cntrl* __lwp_wd_next(wd_cntrl *wd)
{
	return (wd_cntrl*)wd->node.next;
}

static __inline__ wd_cntrl* __lwp_wd_prev(wd_cntrl *wd)
{
	return (wd_cntrl*)wd->node.prev;
}

static __inline__ void __lwp_wd_activate(wd_cntrl *wd)
{
	wd->state = LWP_WD_ACTIVE;
}

static __inline__ void __lwp_wd_deactivate(wd_cntrl *wd)
{
	wd->state = LWP_WD_REMOVE;
}

static __inline__ u32 __lwp_wd_isactive(wd_cntrl *wd)
{
	return (wd->state==LWP_WD_ACTIVE);
}

static __inline__ void __lwp_wd_tickle_ticks()
{
	__lwp_wd_tickle(&_wd_ticks_queue);
}

static __inline__ void __lwp_wd_tickle_secs()
{
	__lwp_wd_tickle(&_wd_secs_queue);
}

static __inline__ void __lwp_wd_insert_ticks(wd_cntrl *wd,u64 interval)
{
	wd->init_interval = interval;
	__lwp_wd_insert(&_wd_ticks_queue,wd);
}

static __inline__ void __lwp_wd_insert_secs(wd_cntrl *wd,u64 interval)
{
	wd->init_interval = interval;
	__lwp_wd_insert(&_wd_secs_queue,wd);
}

static __inline__ void __lwp_wd_adjust_ticks(u32 dir,u64 interval)
{
	__lwp_wd_adjust(&_wd_ticks_queue,dir,interval);
}

static __inline__ void __lwp_wd_adjust_secs(u32 dir,u64 interval)
{
	__lwp_wd_adjust(&_wd_secs_queue,dir,interval);
}

static __inline__ void __lwp_wd_reset(wd_cntrl *wd)
{
	__lwp_wd_remove(wd);
	__lwp_wd_insert(&_wd_ticks_queue,wd);
}
#endif
