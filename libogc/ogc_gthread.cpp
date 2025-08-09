#include <bits/gthr-default.h>
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>

#include "lwp.h"
#include "mutex.h"
#include "cond.h"

#define __OGC_GTHR_BASE_PRIO (64)

#define __OGC_ONCE_INIT (0)
#define __OGC_ONCE_STARTED (1)
#define __OGC_ONCE_DONE (2)

extern "C" {

typedef struct {
	lwpq_t queue;
	lwp_t thread;
} gthr_thread_t;

int __gthr_impl_active(void)
{
	return 1;
}

int __gthr_impl_create(__gthread_t *__threadid, void *(*__func) (void*), void *__args)
{
	gthr_thread_t *th = (gthr_thread_t*)malloc(sizeof(gthr_thread_t));

	if (!th) {
		return ENOMEM;
	}

	if (LWP_InitQueue(&th->queue) != LWP_SUCCESSFUL) {
		free(th);
		return EINVAL;
	}

	if (LWP_CreateThread(&th->thread, __func, __args, NULL, 0, __OGC_GTHR_BASE_PRIO) != LWP_SUCCESSFUL) {
		LWP_CloseQueue(th->queue);
		free(th);
		return EINVAL;
	}

	*__threadid = (__gthread_t)th;
	return 0;
}

int __gthr_impl_join(__gthread_t __threadid, void **__value_ptr)
{
	gthr_thread_t *th = (gthr_thread_t*)__threadid;

	int res = LWP_JoinThread(th->thread, __value_ptr);
	if (res != LWP_SUCCESSFUL) {
		return -1;
	}

	/* Clean up thread data */
	LWP_CloseQueue(th->queue);
	free(th);

	return 0;
}

int __gthr_impl_detach(__gthread_t __threadid)
{
	/* Not supported */
	return -1;
}

int __gthr_impl_equal(__gthread_t __t1, __gthread_t __t2)
{
	return (gthr_thread_t*)__t1 == (gthr_thread_t*)__t2;
}

__gthread_t __gthr_impl_self(void)
{
	/*
	HACK: __gthread_self() is only used for std::thread::get_id(), so returning
	LWP_GetSelf() works as a unique id even though it's technically not a thread
	 */
	return (__gthread_t)LWP_GetSelf();
}

int __gthr_impl_yield(void)
{
	LWP_YieldThread();
	return 0;
}

int __gthr_impl_once(__gthread_once_t *__once, void (*__func) (void))
{
	uint32_t expected = __OGC_ONCE_INIT;
	if (__atomic_compare_exchange_n((uint32_t *)__once, &expected, __OGC_ONCE_STARTED, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		__func();
		__atomic_store_n((uint32_t *)__once, __OGC_ONCE_DONE, __ATOMIC_RELEASE);
	} else if (expected != __OGC_ONCE_DONE) {
		do {
			__atomic_load((uint32_t *)__once, &expected, __ATOMIC_ACQUIRE);
		} while (expected != __OGC_ONCE_DONE);
	}

	return 0;
}

void __gthr_impl_mutex_init_function(__gthread_mutex_t *mutex)
{
	LWP_MutexInit(((mutex_t*)mutex), false);
}

int __gthr_impl_mutex_lock(__gthread_mutex_t *mutex)
{
	return LWP_MutexLock(*((mutex_t*)mutex));
}

int __gthr_impl_mutex_trylock(__gthread_mutex_t *mutex)
{
	return LWP_MutexTryLock(*((mutex_t*)mutex));
}

int __gthr_impl_mutex_unlock(__gthread_mutex_t *mutex)
{
	return LWP_MutexUnlock(*((mutex_t*)mutex));
}

int __gthr_impl_mutex_destroy(__gthread_mutex_t *mutex)
{
	return LWP_MutexDestroy(*((mutex_t*)mutex));
}

int __gthr_impl_recursive_mutex_init_function(__gthread_recursive_mutex_t *mutex)
{
	return LWP_MutexInit(((mutex_t *)mutex), true);
}

int __gthr_impl_recursive_mutex_lock(__gthread_recursive_mutex_t *mutex)
{
	return LWP_MutexLock(*((mutex_t*)mutex));
}

int __gthr_impl_recursive_mutex_trylock(__gthread_recursive_mutex_t *mutex)
{
	return LWP_MutexTryLock(*((mutex_t*)mutex));
}

int __gthr_impl_recursive_mutex_unlock(__gthread_recursive_mutex_t *mutex)
{
	return LWP_MutexUnlock(*((mutex_t*)mutex));
}

int __gthr_impl_recursive_mutex_destroy(__gthread_recursive_mutex_t *mutex)
{
	return LWP_MutexDestroy(*((mutex_t*)mutex));
}

void __gthr_impl_cond_init_function(__gthread_cond_t *__cond)
{
	LWP_CondInit((cond_t*)__cond);
}

int __gthr_impl_cond_broadcast(__gthread_cond_t *__cond)
{
	return LWP_CondBroadcast(*(cond_t*)__cond);
}

int __gthr_impl_cond_signal(__gthread_cond_t *__cond)
{
	return LWP_CondSignal(*(cond_t*)__cond);
}

int __gthr_impl_cond_wait(__gthread_cond_t *__cond, __gthread_mutex_t *__mutex)
{
	return LWP_CondWait(*(cond_t*)__cond, *(mutex_t*)__mutex);
}

int __gthr_impl_cond_timedwait(__gthread_cond_t *__cond, __gthread_mutex_t *__mutex, const __gthread_time_t *__abs_timeout)
{
	return LWP_CondTimedWait(*(cond_t*)__cond, *(mutex_t*)__mutex, __abs_timeout);
}

int __gthr_impl_cond_wait_recursive(__gthread_cond_t *__cond, __gthread_recursive_mutex_t *__mutex)
{
	return LWP_CondWait(*(cond_t*)__cond, *(mutex_t*)__mutex);
}

int __gthr_impl_cond_destroy(__gthread_cond_t* __cond)
{
	return LWP_CondDestroy(*(cond_t*)__cond);
}

/* Dummy function required so that the linker doesn't strip this module */
void __ogc_gthread_init() {}
}