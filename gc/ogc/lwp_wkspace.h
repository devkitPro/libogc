#ifndef __LWP_WKSPACE_H__
#define __LWP_WKSPACE_H__

#include <gctypes.h>
#include <asm.h>

#define WKSPACE_BLOCK_USED		1
#define WKSPACE_BLOCK_FREE		0

#define WKSPACE_DUMMY_FLAG		(0+WKSPACE_BLOCK_USED);

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _wkspace_block_st wkspace_block;
struct _wkspace_block_st {
	u32 back_flag;
	u32 front_flag;
	wkspace_block *next;
	wkspace_block *prev;
};

#define WKSPACE_OVERHEAD		(sizeof(u32)*2)
#define WKSPACE_BLOCK_USED_OH	(sizeof(void*)*2)
#define WKSPACE_MIN_SIZE		(WKSPACE_OVERHEAD+sizeof(wkspace_block))

typedef struct _wkspace_cntrl {
	wkspace_block *start;
	wkspace_block *final;
	
	wkspace_block *first;
	wkspace_block *perm_null;
	wkspace_block *last;
	u32 pg_size;
	u32 reserved;
} wkspace_cntrl;

void __lwp_wkspace_init(u32 size);
void* __lwp_wkspace_allocate(u32 size);
void __lwp_wkspace_free(void *ptr);

#include <libogc/lwp_wkspace.inl>

#ifdef __cplusplus
	}
#endif

#endif
