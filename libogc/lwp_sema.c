#include "asm.h"
#include "lwp_sema.h"

#define LWP_MAXSEMAS		1024

struct _lwp_semaobj {
	s32 sem_id;
	lwp_sema sem;
};

static struct _lwp_semaobj _sema_objects[LWP_MAXSEMAS];

lwp_sema* __lwp_sema_allocsema()
{
	s32 i;
	u32 level;
	lwp_sema *ret = NULL;
	
	_CPU_ISR_Disable(level);

	i = 0;
	while(i<LWP_MAXSEMAS && _sema_objects[i].sem_id!=-1) i++;
	if(i<LWP_MAXSEMAS) {
		_sema_objects[i].sem_id = i;
		ret = &_sema_objects[i].sem;
	}

	_CPU_ISR_Restore(level);
	return ret;
}

void __lwp_sema_freesema(lwp_sema *sem)
{
	s32 i;

	i = 0;
	while(i<LWP_MAXSEMAS && sem!=&_sema_objects[i].sem) i++;
	if(i<LWP_MAXSEMAS && _sema_objects[i].sem_id!=-1) {
		_sema_objects[i].sem_id = -1;
	}
}

void __lwp_sema_init()
{
	s32 i;
	for(i=0;i<LWP_MAXSEMAS;i++) _sema_objects[i].sem_id = -1;
}

void __lwp_sema_initialize(lwp_sema *sema,lwp_semattr *attrs,u32 init_count)
{
	sema->attrs = *attrs;
	sema->count = init_count;

	__lwp_threadqueue_init(&sema->wait_queue,__lwp_sema_ispriority(attrs)?LWP_THREADQ_MODEPRIORITY:LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_SEMAPHORE,LWP_SEMA_TIMEOUT);
}

u32 __lwp_sema_surrender(lwp_sema *sema,u32 id)
{
	u32 level,ret;
	lwp_cntrl *thethread;
	
	ret = LWP_SEMA_SUCCESSFUL;
	if((thethread=__lwp_threadqueue_dequeue(&sema->wait_queue))) return ret;
	else {
		_CPU_ISR_Disable(level);
		if(sema->count<=sema->attrs.max_cnt)
			++sema->count;
		else
			ret = LWP_SEMA_MAXCNT_EXCEEDED;
		_CPU_ISR_Restore(level);
	}
	return ret;
}

u32 __lwp_sema_seize(lwp_sema *sema,u32 id,u32 wait,u32 timeout)
{
	u32 level;
	lwp_cntrl *exec;
	
	exec = _thr_executing;
	exec->wait.ret_code = LWP_SEMA_SUCCESSFUL;

	_CPU_ISR_Disable(level);
	if(sema->count!=0) {
		--sema->count;
		_CPU_ISR_Restore(level);
		return LWP_SEMA_SUCCESSFUL;
	}

	if(!wait) {
		_CPU_ISR_Restore(level);
		exec->wait.ret_code = LWP_SEMA_UNSATISFIED_NOWAIT;
		return LWP_SEMA_UNSATISFIED_NOWAIT;
	}

	__lwp_threadqueue_csenter(&sema->wait_queue);
	exec->wait.queue = &sema->wait_queue;
	exec->wait.id = id;
	_CPU_ISR_Restore(level);
	
	__lwp_threadqueue_enqueue(&sema->wait_queue,timeout);
	return LWP_SEMA_SUCCESSFUL;
}

void __lwp_sema_flush(lwp_sema *sema,u32 status)
{
	__lwp_threadqueue_flush(&sema->wait_queue,status);
}
