#include <stdlib.h>
#include <system.h>
#include <asm.h>
#include <processor.h>
#include "lwp_wkspace.h"

static wkspace_cntrl __wkspace_area;

extern void SYS_SetArenaLo(void *newLo);
extern void* SYS_GetArenaLo();

void __lwp_wkspace_init(u32 size)
{
	u32 rsize;
	u32 arLo,level;
	wkspace_block *block;

	if(size<WKSPACE_MIN_SIZE) return;

	__wkspace_area.pg_size = PPC_ALIGNMENT;
	rsize = size - WKSPACE_OVERHEAD;
	
	// Get current ArenaLo and adjust to 32 byte boundary
	_CPU_ISR_Disable(level);
	arLo = (u32)SYS_GetArenaLo();
	arLo += 31;
	arLo &= ~31;
	rsize -= arLo - (u32)SYS_GetArenaLo();
	SYS_SetArenaLo((void*)(arLo+rsize));
	_CPU_ISR_Restore(level);

	block = (wkspace_block*)arLo;
	block->back_flag = WKSPACE_DUMMY_FLAG;
	block->front_flag = rsize;
	block->next	= __lwp_wkspace_tail(&__wkspace_area);
	block->prev = __lwp_wkspace_head(&__wkspace_area);
	
	__wkspace_area.start = block;
	__wkspace_area.first = block;
	__wkspace_area.perm_null = NULL;
	__wkspace_area.last = block;
	
	block = __lwp_wkspace_nextblock(block);
	block->back_flag = rsize;
	block->front_flag = WKSPACE_DUMMY_FLAG;
	__wkspace_area.final = block;
}

void* __lwp_wkspace_allocate(u32 size)
{
	u32 excess;
	u32 rsize;
	wkspace_block *block;
	wkspace_block *next_block;
	wkspace_block *tmp_block;
	void *ptr;
	u32 offset,level;

	_CPU_ISR_Disable(level);

	if(size>=(-1-WKSPACE_BLOCK_USED_OH)) return NULL;

	excess = size%PPC_ALIGNMENT;
	rsize = size+PPC_ALIGNMENT+WKSPACE_BLOCK_USED_OH;
	
	if(excess)
		rsize += PPC_ALIGNMENT-excess;

	if(rsize<sizeof(wkspace_block)) rsize = sizeof(wkspace_block);
	
	for(block=__wkspace_area.first;;block=block->next) {
		if(block==__lwp_wkspace_tail(&__wkspace_area)) {
			_CPU_ISR_Restore(level);
			return NULL;
		}
		if(block->front_flag>=rsize) break;
	}
	
	if((block->front_flag-rsize)>(__wkspace_area.pg_size+WKSPACE_BLOCK_USED_OH)) {
		block->front_flag -= rsize;
		next_block = __lwp_wkspace_nextblock(block);
		next_block->back_flag = block->front_flag;
		
		tmp_block = __lwp_wkspace_blockat(block,rsize);
		tmp_block->back_flag = next_block->front_flag = __lwp_wkspace_buildflag(rsize,WKSPACE_BLOCK_USED);

		ptr = __lwp_wkspace_startuser(next_block);
	} else {
		next_block = __lwp_wkspace_nextblock(block);
		next_block->back_flag = __lwp_wkspace_buildflag(rsize,WKSPACE_BLOCK_USED);
		
		block->front_flag = next_block->back_flag;
		block->next->prev = block->prev;
		block->prev->next = block->next;
		
		ptr = __lwp_wkspace_startuser(block);
	}

	offset = PPC_ALIGNMENT - (((u32)ptr)&(PPC_ALIGNMENT-1));
	ptr += offset;
	*(((u32*)ptr)-1) = offset;

	_CPU_ISR_Restore(level);
	return ptr;
}

void __lwp_wkspace_free(void *ptr)
{
	wkspace_block *block;
	wkspace_block *next_block;
	wkspace_block *new_next;
	wkspace_block *prev_block;
	wkspace_block *tmp_block;
	u32 rsize,level;

	_CPU_ISR_Disable(level);

	block = __lwp_wkspace_usrblockat(ptr);
	if(!__lwp_wkspace_blockin(&__wkspace_area,block) || __lwp_wkspace_blockfree(block)) {
		_CPU_ISR_Restore(level);
		return;
	}

	rsize = __lwp_wkspace_blocksize(block);
	next_block = __lwp_wkspace_blockat(block,rsize);
	
	if(!__lwp_wkspace_blockin(&__wkspace_area,next_block) || (block->front_flag!=next_block->back_flag)) {
		_CPU_ISR_Restore(level);
		return;
	}
	
	if(__lwp_wkspace_prev_blockfree(block)) {
		prev_block = __lwp_wkspace_prevblock(block);
		if(!__lwp_wkspace_blockin(&__wkspace_area,prev_block)) {
			_CPU_ISR_Restore(level);
			return;
		}
		
		if(__lwp_wkspace_blockfree(next_block)) {
			prev_block->front_flag += next_block->front_flag+rsize;
			tmp_block = __lwp_wkspace_nextblock(prev_block);
			tmp_block->back_flag = prev_block->front_flag;
			next_block->next->prev = next_block->prev;
			next_block->prev->next = next_block->next;
		} else {
			prev_block->front_flag = next_block->back_flag = prev_block->front_flag+rsize;
		}
	} else if(__lwp_wkspace_blockfree(next_block)) {
		block->front_flag = rsize+next_block->front_flag;
		new_next = __lwp_wkspace_nextblock(block);
		new_next->back_flag = block->front_flag;
		block->next = next_block->next;
		block->prev = next_block->prev;
		next_block->prev->next = block;
		next_block->next->prev = block;
		
		if(__wkspace_area.first==next_block) __wkspace_area.first = block;
	} else {
		next_block->back_flag = block->front_flag = rsize;
		block->prev = __lwp_wkspace_head(&__wkspace_area);
		block->next = __wkspace_area.first;
		__wkspace_area.first = block;
		block->next->prev = block;
	}
	_CPU_ISR_Restore(level);
}
