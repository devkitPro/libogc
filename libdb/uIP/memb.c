#include <stdlib.h>
#include <string.h>

#include "memb.h"

void memb_init(struct memb_blks *blk)
{
	memset(blk->mem,0,(blk->size+1)*blk->num);
}

void* memb_alloc(struct memb_blks *blk)
{
	s32 i;
	u8 *ptr;
	
	ptr = blk->mem;
	for(i=0;i<blk->num;i++) {
		if(*ptr==0) {
			++(*ptr);
			return (void*)(ptr+1);
		}
		ptr += (blk->size+1);
	}
	return NULL;
}

u8 memb_free(struct memb_blks *blk,void *ptr)
{
	s32 i;
	u8 *ptr2,*p = (u8*)ptr;

	ptr2 = blk->mem;
	for(i=0;i<blk->num;i++) {
		if(ptr2==(ptr - 1)) {
			return --(*ptr2);
		}
		ptr2 += (blk->size+1);
	}
	return -1;
}

u8 memb_ref(struct memb_blks *blk,void *ptr)
{
	return ++(*(((u8*)ptr) - 1));
}
