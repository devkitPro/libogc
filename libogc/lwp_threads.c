#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "asm.h"
#include "processor.h"
#include "sys_state.h"
#include "lwp_stack.h"
#include "lwp_threads.h"
#include "lwp_threadq.h"
#include "lwp_watchdog.h"
#include "lwp_wkspace.h"

/* new one */
frame_context core_context;

lwp_cntrl *_thr_main = NULL;
lwp_cntrl *_thr_idle = NULL;
lwp_cntrl *_thr_executing = NULL;
lwp_cntrl *_thr_heir = NULL;
lwp_cntrl *_thr_allocated_fp = NULL;

lwp_queue _lwp_thr_ready[256];

volatile boolean _context_switch_want;
vu32 _thread_dispatch_disable_level;

void **__lwp_thr_libc_reent = NULL;

static u32 _lwp_threads[1024];
static lwp_obj _lwp_objects[1024];

extern void _cpu_context_save_fp(void *);
extern void _cpu_context_restore_fp(void *);
extern void _cpu_context_switch(void *,void *);
extern void _cpu_context_save(void *);
extern void _cpu_context_restore(void *);

extern int __libc_create_hook(lwp_cntrl *,lwp_cntrl *);
extern int __libc_start_hook(lwp_cntrl *,lwp_cntrl *);
extern int __libc_delete_hook(lwp_cntrl *, lwp_cntrl *);

extern int main();

extern u8 __stack_addr[],__stack_end[];

extern int printk(const char *fmt,...);

#ifdef _LWPTHREADS_DEBUG
static void __lwp_dumpcontext(frame_context *ctx)
{
	printf("GPR00 %08x GPR08 %08x GPR16 %08x GPR24 %08x\n",ctx->GPR[0], ctx->GPR[8], ctx->GPR[16], ctx->GPR[24]);
	printf("GPR01 %08x GPR09 %08x GPR17 %08x GPR25 %08x\n",ctx->GPR[1], ctx->GPR[9], ctx->GPR[17], ctx->GPR[25]);
	printf("GPR02 %08x GPR10 %08x GPR18 %08x GPR26 %08x\n",ctx->GPR[2], ctx->GPR[10], ctx->GPR[18], ctx->GPR[26]);
	printf("GPR03 %08x GPR11 %08x GPR19 %08x GPR27 %08x\n",ctx->GPR[3], ctx->GPR[11], ctx->GPR[19], ctx->GPR[27]);
	printf("GPR04 %08x GPR12 %08x GPR20 %08x GPR28 %08x\n",ctx->GPR[4], ctx->GPR[12], ctx->GPR[20], ctx->GPR[28]);
	printf("GPR05 %08x GPR13 %08x GPR21 %08x GPR29 %08x\n",ctx->GPR[5], ctx->GPR[13], ctx->GPR[21], ctx->GPR[29]);
	printf("GPR06 %08x GPR14 %08x GPR22 %08x GPR30 %08x\n",ctx->GPR[6], ctx->GPR[14], ctx->GPR[22], ctx->GPR[30]);
	printf("GPR07 %08x GPR15 %08x GPR23 %08x GPR31 %08x\n",ctx->GPR[7], ctx->GPR[15], ctx->GPR[23], ctx->GPR[31]);
	printf("LR %08x SRR0 %08x SRR1 %08x MSR %08x\n\n", ctx->LR, ctx->SRR0, ctx->SRR1,ctx->MSR);
}
#endif

u32 __lwp_isr_in_progress()
{
	register u32 isr_nest_level;
	isr_nest_level = mfspr(272);
	return isr_nest_level;
}

static inline void __lwp_msr_setlevel(u32 level)
{
	register u32 msr;
	_CPU_MSR_GET(msr);
	if(!(level&CPU_MODES_INTERRUPT_MASK))
		msr |= MSR_EE;
	else
		msr &= ~MSR_EE;
	_CPU_MSR_SET(msr);
}

static inline u32 __lwp_msr_getlevel()
{
	register u32 msr;
	_CPU_MSR_GET(msr);
	if(msr&MSR_EE) return 0;
	else return 1;
}

void __thread_dispatch()
{
	u32 level;
	lwp_cntrl *exec,*heir;

	exec = _thr_executing;
	_CPU_ISR_Disable(level);
	while(_context_switch_want==TRUE) {
		heir = _thr_heir;
		_thread_dispatch_disable_level = 1;
		_context_switch_want = FALSE;
		_thr_executing = heir;
		_CPU_ISR_Restore(level);

		if(__lwp_thr_libc_reent) {
			exec->libc_reent = *__lwp_thr_libc_reent;
			*__lwp_thr_libc_reent = heir->libc_reent;
		}

		_cpu_context_save_fp((void*)&exec->fp);
		_cpu_context_switch((void*)&exec->context,(void*)&heir->context);

		if(!__lwp_thread_isallocatedfp(exec)) {
			if(_thr_allocated_fp!=NULL)
				_cpu_context_save_fp((void*)&_thr_allocated_fp->fp);
			_cpu_context_restore_fp((void*)&exec->fp);
			_thr_allocated_fp = exec;
		}

		exec = _thr_executing;
		_CPU_ISR_Disable(level);
	}
	_thread_dispatch_disable_level = 0;
	_CPU_ISR_Restore(level);
}

static void* idle_func(void *arg)
{
	while(1);
	return 0;
}

static void __lwp_thread_handler()
{
	u32 level;
	lwp_cntrl *exec;

	exec = _thr_executing;
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_handler(%p,%d)\n",exec,_thread_dispatch_disable_level);
#endif
	level = exec->isr_level;
	__lwp_msr_setlevel(level);

	if(!__lwp_thread_isallocatedfp(exec)) {
		if(_thr_allocated_fp!=NULL)
			_cpu_context_save_fp((void*)&_thr_allocated_fp->fp);
		_thr_allocated_fp = exec;
	}
	
	__lwp_thread_dispatchenable();
	exec->wait.ret_arg = exec->entry(exec->arg);

	__lwp_thread_exit(exec->wait.ret_arg);
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_handler(%p): thread returned(%p)\n",exec,exec->wait.ret_arg);
#endif
}

void __lwp_rotate_readyqueue(u32 prio)
{
	u32 level;
	lwp_cntrl *exec;
	lwp_queue *ready;
	lwp_node *node;

	ready = &_lwp_thr_ready[prio];
	exec = _thr_executing;
	
	if(ready==exec->ready) {
		__lwp_thread_yield();
		return;
	}

	_CPU_ISR_Disable(level);
	if(!__lwp_queue_isempty(ready) && !__lwp_queue_onenode(ready)) {
		node = __lwp_queue_firstnodeI(ready);
		__lwp_queue_appendI(ready,node);
	}
	_CPU_ISR_Flash(level);
	
	if(_thr_heir->ready==ready)
		_thr_heir = (lwp_cntrl*)ready->first;

	if(exec!=_thr_heir)
		_context_switch_want = TRUE;

#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_rotate_readyqueue(%d,%p,%p)\n",prio,exec,_thr_heir);
#endif
	_CPU_ISR_Restore(level);
}

void __lwp_thread_yield()
{
	u32 level;
	lwp_cntrl *exec;
	lwp_queue *ready;
	
	exec = _thr_executing;
	ready = exec->ready;
	
	_CPU_ISR_Disable(level);
	if(!__lwp_queue_onenode(ready)) {
		__lwp_queue_extractI(&exec->node);
		__lwp_queue_appendI(ready,&exec->node);
		_CPU_ISR_Flash(level);
		if(__lwp_thread_isheir(exec))
			_thr_heir = (lwp_cntrl*)ready->first;
		_context_switch_want = TRUE;
	} else if(!__lwp_thread_isheir(exec))
		_context_switch_want = TRUE;
	_CPU_ISR_Restore(level);
}

void __lwp_thread_setstate(lwp_cntrl *thethread,u32 state)
{
	u32 level;
	lwp_queue *ready;

	ready = thethread->ready;
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_setstate(%d,%p,%p,%08x)\n",_context_switch_want,_thr_heir,thethread,thethread->cur_state);
#endif
	_CPU_ISR_Disable(level);
	if(!__lwp_stateready(thethread->cur_state)) {
		thethread->cur_state = __lwp_clearstate(thethread->cur_state,state);
		_CPU_ISR_Restore(level);
		return;
	}

	thethread->cur_state = state;
	if(__lwp_queue_onenode(ready)) {
		__lwp_queue_init_empty(ready);
		__lwp_priomap_removefrom(&thethread->priomap);
	} else 
		__lwp_queue_extractI(&thethread->node);
	_CPU_ISR_Flash(level);

	if(__lwp_thread_isheir(thethread))
		__lwp_thread_calcheir();
	if(__lwp_thread_isexec(thethread))
		_context_switch_want = TRUE;
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_setstate(%d,%p,%p,%08x)\n",_context_switch_want,_thr_heir,thethread,thethread->cur_state);
#endif
	_CPU_ISR_Restore(level);
}

void __lwp_thread_clearstate(lwp_cntrl *thethread,u32 state)
{
	u32 level,cur_state;
	
	_CPU_ISR_Disable(level);

	cur_state = thethread->cur_state;
	if(cur_state&state) {
		cur_state = thethread->cur_state = __lwp_clearstate(cur_state,state);
		if(__lwp_stateready(cur_state)) {
			__lwp_priomap_addto(&thethread->priomap);
			__lwp_queue_appendI(thethread->ready,&thethread->node);
			_CPU_ISR_Flash(level);
			
			if(thethread->cur_prio<_thr_heir->cur_prio) {
				_thr_heir = thethread;
				_context_switch_want = TRUE;
			}
		}
	}

	_CPU_ISR_Restore(level);
}

void __lwp_thread_changepriority(lwp_cntrl *thethread,u32 prio,u32 prependit)
{
	u32 level;
	
	__lwp_thread_settransient(thethread);
	
	if(thethread->cur_prio!=prio) 
		__lwp_thread_setpriority(thethread,prio);

	_CPU_ISR_Disable(level);

	thethread->cur_state = __lwp_clearstate(thethread->cur_state,LWP_STATES_TRANSIENT);
	if(!__lwp_stateready(thethread->cur_state)) {
		_CPU_ISR_Restore(level);
		return;
	}

	__lwp_priomap_addto(&thethread->priomap);
	if(prependit)
		__lwp_queue_prependI(thethread->ready,&thethread->node);
	else
		__lwp_queue_appendI(thethread->ready,&thethread->node);

	_CPU_ISR_Flash(level);

	__lwp_thread_calcheir();
	
	if(!(_thr_executing==_thr_heir))
		_context_switch_want = TRUE;

	_CPU_ISR_Restore(level);
}

void __lwp_thread_setpriority(lwp_cntrl *thethread,u32 prio)
{
	thethread->cur_prio = prio;
	thethread->ready = &_lwp_thr_ready[prio];
	__lwp_priomap_init(&thethread->priomap,prio);
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_setpriority(%p,%d,%p)\n",thethread,prio,thethread->ready);
#endif
}

void __lwp_thread_suspend(lwp_cntrl *thethread)
{
	u32 level;
	lwp_queue *ready;

	ready = thethread->ready;
	
	_CPU_ISR_Disable(level);
	thethread->suspendcnt++;
	if(!__lwp_stateready(thethread->cur_state)) {
		thethread->cur_state = __lwp_setstate(thethread->cur_state,LWP_STATES_SUSPENDED);
		_CPU_ISR_Restore(level);
		return;
	}
	
	thethread->cur_state = LWP_STATES_SUSPENDED;
	if(__lwp_queue_onenode(ready)) {
		__lwp_queue_init_empty(ready);
		__lwp_priomap_removefrom(&thethread->priomap);
	} else {
		__lwp_queue_extractI(&thethread->node);
	}
	_CPU_ISR_Flash(level);
	
	if(__lwp_thread_isheir(thethread))
		__lwp_thread_calcheir();
	
	if(__lwp_thread_isexec(thethread))
		_context_switch_want = TRUE;

	_CPU_ISR_Restore(level);
}

void __lwp_thread_settransient(lwp_cntrl *thethread)
{
	u32 level,oldstates;
	lwp_queue *ready;

	ready = thethread->ready;

	_CPU_ISR_Disable(level);
	
	oldstates = thethread->cur_state;
	thethread->cur_state = __lwp_setstate(oldstates,LWP_STATES_TRANSIENT);

	if(__lwp_stateready(oldstates)) {
		if(__lwp_queue_onenode(ready)) {
			__lwp_queue_init_empty(ready);
			__lwp_priomap_removefrom(&thethread->priomap);
		} else {
			__lwp_queue_extractI(&thethread->node);
		}
	}

	_CPU_ISR_Restore(level);
}

void __lwp_thread_resume(lwp_cntrl *thethread,u32 force)
{
	u32 level,state;
	
	_CPU_ISR_Disable(level);

	if(force==TRUE)
		thethread->suspendcnt = 0;
	else
		thethread->suspendcnt--;
	
	if(thethread->suspendcnt>0) {
		_CPU_ISR_Restore(level);
		return;
	}

	state = thethread->cur_state;
	if(state&LWP_STATES_SUSPENDED) {
		state = thethread->cur_state = __lwp_clearstate(thethread->cur_state,LWP_STATES_SUSPENDED);
		if(__lwp_stateready(state)) {
			__lwp_priomap_addto(&thethread->priomap);
			__lwp_queue_appendI(thethread->ready,&thethread->node);
			_CPU_ISR_Flash(level);
			if(thethread->cur_prio<_thr_heir->cur_prio) {
				_thr_heir = thethread;
				_context_switch_want = TRUE;
			}
		}
	}
	_CPU_ISR_Restore(level);
}

void __lwp_thread_loadenv(lwp_cntrl *thethread)
{
	u32 stackbase,sp,size;
	u32 r2,r13,msr_value;
	
	thethread->fp.FPSCR = 0x000000f8;

	stackbase = (u32)thethread->stack;
	size = thethread->stack_size;

	// tag both bottom & head of stack
	*((u32*)stackbase) = 0xDEADBABE;
	sp = stackbase+size-CPU_MINIMUM_STACK_FRAME_SIZE;
	sp &= ~(CPU_STACK_ALIGNMENT-1);
	*((u32*)sp) = 0;
	
	thethread->context.GPR[1] = sp;
	
	_CPU_MSR_GET(msr_value);
	if(!(thethread->isr_level&CPU_MODES_INTERRUPT_MASK))
		msr_value |= MSR_EE;
	else
		msr_value &= ~MSR_EE;
	
	thethread->context.MSR = msr_value;
	thethread->context.MSR |= MSR_FP;

	thethread->context.LR = (u32)__lwp_thread_handler;

	__asm__ __volatile__ ("mr %0,2; mr %1,13" : "=r" ((r2)), "=r" ((r13)));
	thethread->context.GPR[2] = r2;
	thethread->context.GPR[13] = r13;

#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_loadenv(%p,%p,%d,%p)\n",thethread,(void*)stackbase,size,(void*)sp);
#endif

}

void __lwp_thread_ready(lwp_cntrl *thethread)
{
	u32 level;
	lwp_cntrl *heir;

	_CPU_ISR_Disable(level);
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_ready(%p)\n",thethread);
#endif
	thethread->cur_state = LWP_STATES_READY;
	__lwp_priomap_addto(&thethread->priomap);
	__lwp_queue_appendI(thethread->ready,&thethread->node);
	_CPU_ISR_Flash(level);

	__lwp_thread_calcheir();
	heir = _thr_heir;
	if(!(__lwp_thread_isexec(heir)))
		_context_switch_want = TRUE;
	
	_CPU_ISR_Restore(level);
}

u32 __lwp_thread_init(lwp_cntrl *thethread,void *stack_area,u32 stack_size,u32 prio,u32 isr_level)
{
	u32 i,act_stack_size = 0;

#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_init(%p,%p,%d,%d,%d)\n",thethread,stack_area,stack_size,prio,isr_level);
#endif
	if(!stack_area) {
		if(!__lwp_stack_isenough(stack_size))
			act_stack_size = CPU_MINIMUM_STACK_SIZE;
		else
			act_stack_size = stack_size;

		act_stack_size = __lwp_stack_allocate(thethread,act_stack_size);
		if(!act_stack_size) return 0;

		thethread->stack_allocated = TRUE;
	} else {
		thethread->stack = stack_area;
		act_stack_size = stack_size;
		thethread->stack_allocated = FALSE;
	}
	thethread->stack_size = act_stack_size;

	i=0;
	while(i<1024 && _lwp_threads[i]!=-1) i++;
	if(i<1024) {
		thethread->own = &_lwp_objects[i];
		_lwp_threads[i] = (u32)thethread;
		_lwp_objects[i].lwp_id = thethread->id = i+1;
		_lwp_objects[i].thread = thethread;
		__lwp_threadqueue_init(&_lwp_objects[i].join_list,LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_JOINATEXIT,0);
	} else{
		__lwp_stack_free(thethread);
		return 0;
	}

	memset(&thethread->context,0,sizeof(thethread->context));
	memset(&thethread->fp,0,sizeof(thethread->fp));
	memset(&thethread->wait,0,sizeof(thethread->wait));

	
	thethread->isr_level = isr_level;
	thethread->real_prio = prio;
	thethread->cur_state = LWP_STATES_DORMANT;
	thethread->suspendcnt = 0;
	thethread->res_cnt = 0;
	__lwp_thread_setpriority(thethread,prio);

	__libc_create_hook(_thr_executing,thethread);

	return 1;
}

void __lwp_thread_close(lwp_cntrl *thethread)
{
	u32 idx,level;
	void **value_ptr;
	lwp_cntrl *p;

	__lwp_thread_setstate(thethread,LWP_STATES_TRANSIENT);
	

	if(!__lwp_threadqueue_extractproxy(thethread)) {
		if(__lwp_wd_isactive(&thethread->timer))
			__lwp_wd_remove(&thethread->timer);
	}
	
	_CPU_ISR_Disable(level);
	idx = thethread->id-1;
	_lwp_threads[idx] = -1;

	value_ptr = (void**)thethread->wait.ret_arg;
	if(thethread->own) {
		while((p=__lwp_threadqueue_dequeue(&thethread->own->join_list))) {
			*(void**)p->wait.ret_arg = value_ptr;
		}
		_lwp_objects[idx].lwp_id = -1;
		_lwp_objects[idx].thread = NULL;
	}
	_CPU_ISR_Restore(level);

	__libc_delete_hook(_thr_executing,thethread);

	if(__lwp_thread_isallocatedfp(thethread))
		__lwp_thread_deallocatefp();

	__lwp_stack_free(thethread);
	__lwp_wkspace_free(thethread);
}

void __lwp_thread_exit(void *value_ptr)
{
	__lwp_thread_dispatchdisable();
	_thr_executing->wait.ret_arg = (u32*)value_ptr;
	__lwp_thread_close(_thr_executing);
	__lwp_thread_dispatchenable();
}

lwp_obj* __lwp_thread_getobject(lwp_cntrl *thethread)
{
	u32 i;

	i=0;
	while(i<1024 && _lwp_threads[i]!=(u32)thethread && _lwp_threads[i]!=-1) i++;
	if(i>=1024) return NULL;
	return &_lwp_objects[i];
}

u32 __lwp_thread_start(lwp_cntrl *thethread,void* (*entry)(void*),void *arg)
{
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_start(%p,%p,%p,%d)\n",thethread,entry,arg,thethread->cur_state);
#endif
	if(__lwp_statedormant(thethread->cur_state)) {
		thethread->entry = entry;
		thethread->arg = arg;
		__lwp_thread_loadenv(thethread);
		__lwp_thread_ready(thethread);
		__libc_start_hook(_thr_executing,thethread);
		return 1;
	}
	return 0;
}

void __lwp_start_multitasking()
{
	u32 level;

	_CPU_ISR_Disable(level);

	__sys_state_set(SYS_STATE_BEGIN_MT);
	__sys_state_set(SYS_STATE_UP);

	_context_switch_want = FALSE;
	_thr_executing = _thr_heir;
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_start_multitasking(%p,%p)\n",_thr_executing,_thr_heir);
#endif
	_cpu_context_restore_fp((void*)&_thr_heir->fp);
	_cpu_context_switch((void*)&core_context,(void*)&_thr_heir->context);

	_CPU_ISR_Restore(level);
}

void __lwp_stop_multitasking()
{
	if(__sys_state_get()!=SYS_STATE_SHUTDOWN) {
		__sys_state_set(SYS_STATE_SHUTDOWN);
		_cpu_context_switch((void*)&_thr_executing->context,(void*)&core_context);
	}
}

u32 __lwp_sys_init()
{
	u32 index,level;

#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_sys_init()\n\n");
#endif
	_CPU_ISR_Disable(level);
	
	__lwp_thread_dispatchinitialize();
	
	_context_switch_want = FALSE;
	_thr_executing = NULL;
	_thr_heir = NULL;
	_thr_allocated_fp = NULL;

	memset(_lwp_threads,-1,1024*sizeof(u32));

	for(index=0;index<=LWP_PRIO_MAX;index++)
		__lwp_queue_init_empty(&_lwp_thr_ready[index]);
	
	__sys_state_set(SYS_STATE_BEFORE_MT);
	
	// create idle thread, is needed if all threads are locked on a queue
	_thr_idle = (lwp_cntrl*)__lwp_wkspace_allocate(sizeof(lwp_cntrl));
	__lwp_thread_init(_thr_idle,NULL,8192,255,0);
	_thr_executing = _thr_heir = _thr_idle;
	__lwp_thread_start(_thr_idle,idle_func,NULL);

	// create main thread, as this is our entry point
	// for every GC application.
	_thr_main = (lwp_cntrl*)__lwp_wkspace_allocate(sizeof(lwp_cntrl));
	__lwp_thread_init(_thr_main,NULL,65536,191,0);
	_thr_executing = _thr_heir = _thr_main;
	__lwp_thread_start(_thr_main,(void*)main,NULL);

	_CPU_ISR_Restore(level);
	return 1;
}

