#ifndef __LWP_WKSPACE_INL__
#define __LWP_WKSPACE_INL__

#include "lwp_wkspace.h"
#include "lwp_heap.h"

static __inline__ void* __lwp_wkspace_allocate(u32 size)
{
	return __lwp_heap_allocate(&__wkspace_heap,size);
}

static __inline__ bool __lwp_wkspace_free(void *ptr)
{
	return __lwp_heap_free(&__wkspace_heap,ptr);
}

#endif
