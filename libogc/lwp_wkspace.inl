#ifndef __LWP_WKSPACE_INL__
#define __LWP_WKSPACE_INL__

static __inline__ wkspace_block* __lwp_wkspace_head(wkspace_cntrl *heap)
{
	return (wkspace_block*)&heap->start;
}

static __inline__ wkspace_block* __lwp_wkspace_tail(wkspace_cntrl *heap)
{
	return (wkspace_block*)&heap->final;
}

static __inline__ wkspace_block* __lwp_wkspace_prevblock(wkspace_block *block)
{
	return (wkspace_block*)((char*)block - (block->back_flag&~WKSPACE_BLOCK_USED));
}

static __inline__ wkspace_block* __lwp_wkspace_nextblock(wkspace_block *block)
{
	return (wkspace_block*)((char*)block + (block->front_flag&~WKSPACE_BLOCK_USED));
}

static __inline__ wkspace_block* __lwp_wkspace_blockat(wkspace_block *block,u32 offset)
{
	return (wkspace_block*)((char*)block + offset);
}

static __inline__ void* __lwp_wkspace_startuser(wkspace_block *block)
{
	return (void*)&block->next;
}

static __inline__ wkspace_block* __lwp_wkspace_usrblockat(void *ptr)
{
	u32 offset;
	
	offset = *(((u32*)ptr)-1);
	return __lwp_wkspace_blockat(ptr,-offset+WKSPACE_BLOCK_USED_OH);
}

static __inline__ boolean __lwp_wkspace_blockin(wkspace_cntrl *heap,wkspace_block *block)
{
	return ((void*)block>=(void*)heap->start && (void*)block<=(void*)heap->final);
}

static __inline__ boolean __lwp_wkspace_blockfree(wkspace_block *block)
{
	return !(block->front_flag&WKSPACE_BLOCK_USED);
}

static __inline__ u32 __lwp_wkspace_blocksize(wkspace_block *block)
{
	return (block->front_flag&~WKSPACE_BLOCK_USED);
}

static __inline__ boolean __lwp_wkspace_prev_blockfree(wkspace_block *block)
{
	return !(block->back_flag&WKSPACE_BLOCK_USED);
}

static __inline__ boolean __lwp_wkspace_pgsize_valid(u32 pgsize)
{
	return (pgsize!=0 && ((pgsize%PPC_ALIGNMENT)==0));
}

static __inline__ u32 __lwp_wkspace_buildflag(u32 size,u32 flag)
{
	return (size|flag);
}

#endif
