// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdlib.h>
#include <string.h>
#include <tuxedo/thread.h>
#include <tuxedo/sync.h>
#include <errno.h>
#include <sys/iosupport.h>

// Dummy symbol referenced by crt0 so that this object file is pulled in by the linker
const u32 __newlib_syscalls = 0xdeadbeef;

struct __pthread_t {
	KThread base;
};

void __SYSCALL(lock_acquire)(_LOCK_T* lock)
{
	KMutexLock((KMutex*)lock);
}

void __SYSCALL(lock_release)(_LOCK_T* lock)
{
	KMutexUnlock((KMutex*)lock);
}

void __SYSCALL(lock_acquire_recursive)(_LOCK_RECURSIVE_T* lock)
{
	KRMutexLock((KRMutex*)lock);
}

void __SYSCALL(lock_release_recursive)(_LOCK_RECURSIVE_T* lock)
{
	KRMutexUnlock((KRMutex*)lock);
}

int __SYSCALL(cond_signal)(_COND_T* cond)
{
	KCondVarSignal((KCondVar*)cond);
	return 0;
}

int __SYSCALL(cond_broadcast)(_COND_T* cond)
{
	KCondVarBroadcast((KCondVar*)cond);
	return 0;
}

int __SYSCALL(cond_wait)(_COND_T* cond, _LOCK_T* lock, uint64_t timeout_ns)
{
	int ret = 0;

	if (timeout_ns == UINT64_MAX) {
		KCondVarWait((KCondVar*)cond, (KMutex*)lock);
	} else {
		ret = KCondVarWaitNs((KCondVar*)cond, (KMutex*)lock, timeout_ns) ? 0 : ETIMEDOUT;
	}

	return ret;
}

int __SYSCALL(cond_wait_recursive)(_COND_T* cond, _LOCK_RECURSIVE_T* lock, uint64_t timeout_ns)
{
	KRMutex* r = (KRMutex*)lock;
	u32 counter_backup = r->counter;
	r->counter = 0;

	int ret = 0;
	if (timeout_ns == UINT64_MAX) {
		KCondVarWait((KCondVar*)cond, &r->mutex);
	} else {
		ret = KCondVarWaitNs((KCondVar*)cond, &r->mutex, timeout_ns) ? 0 : ETIMEDOUT;
	}

	r->counter = counter_backup;
	return ret;
}

int __SYSCALL(thread_create)(struct __pthread_t** thread, void* (*func)(void*), void* arg, void* stack_addr, size_t stack_size)
{
	if (((uptr)stack_addr & 7) || (stack_size & 7)) {
		return EINVAL;
	}

	if (!stack_size) {
		stack_size = 64*1024;
	}

	size_t struct_sz = (sizeof(struct __pthread_t) + 7) &~ 7;

	size_t needed_sz = struct_sz + 0; //KThreadGetLocalStorageSize();
	if (!stack_addr) {
		needed_sz += stack_size;
	}

	*thread = malloc(needed_sz); // malloc align is 2*sizeof(void*); which is already 8
	if (!*thread) {
		return ENOMEM;
	}

	void* stack_top;
	if (stack_addr) {
		stack_top = (u8*)stack_addr + stack_size;
	} else {
		stack_top = (u8*)*thread + needed_sz;
	}

	KThread* t = &(*thread)->base;
	KThreadPrepare(t, (KThreadFn)func, arg, stack_top, KTHR_MIN_PRIO);
	//KThreadAttachLocalStorage(t, (u8*)*thread + struct_sz);
	KThreadResume(t);

	return 0;
}

void* __SYSCALL(thread_join)(struct __pthread_t* thread)
{
	void* rc = (void*)KThreadJoin(&thread->base);
	free(thread);
	return rc;
}

void __SYSCALL(thread_exit)(void* value)
{
	KThreadExit((int)value);
}

struct __pthread_t* __SYSCALL(thread_self)(void)
{
	return (struct __pthread_t*)KThreadGetSelf();
}
