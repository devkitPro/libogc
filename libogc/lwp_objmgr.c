#include <processor.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lwp_wkspace.h>

#include "lwp_objmgr.h"

static u32 _lwp_objmgr_mem = 0;
static lwp_obj *null_local_table = NULL;

u32 __lwp_objmgr_memsize()
{
	return _lwp_objmgr_mem;
}

void __lwp_objmgr_initinfo(lwp_objinfo *info,u32 max_nodes,u32 node_size)
{
	u32 idx,i;
	lwp_obj *object;
	lwp_queue inactives;
	void **local_table;
	

	info->min_id = 0;
	info->max_id = 0;
	info->inactives_cnt = 0;
	info->node_size = node_size;
	info->max_nodes = max_nodes;
	info->obj_blocks = NULL;
	info->local_table = &null_local_table;

	__lwp_queue_init_empty(&info->inactives);

	_lwp_objmgr_mem += info->max_nodes*sizeof(lwp_obj*);
	local_table = (void**)__lwp_wkspace_allocate(info->max_nodes*sizeof(lwp_obj*));
	if(!local_table) return;

	info->local_table = (lwp_obj**)local_table;
	for(i=0;i<info->max_nodes;i++) {
		local_table[i] = NULL;
	}

	_lwp_objmgr_mem += info->max_nodes*info->node_size;
	info->obj_blocks = __lwp_wkspace_allocate(info->max_nodes*info->node_size);
	if(!info->obj_blocks) return;

	__lwp_queue_initialize(&inactives,info->obj_blocks,info->max_nodes,info->node_size);

	idx = info->min_id;
	while((object=(lwp_obj*)__lwp_queue_get(&inactives))!=NULL) {
		object->id = idx;
		__lwp_queue_append(&info->inactives,&object->node);
		idx++;
	}

	info->max_id += info->max_nodes;
	info->inactives_cnt += info->max_nodes;
}

lwp_obj* __lwp_objmgr_get(lwp_objinfo *info,u32 id)
{
	u32 level;
	lwp_obj *object = NULL;

	_CPU_ISR_Disable(level);
	if(info->max_id>=id) object = info->local_table[id];
	_CPU_ISR_Restore(level);

	return object;
}

lwp_obj* __lwp_objmgr_allocate(lwp_objinfo *info)
{
	lwp_obj* object;
	 object = (lwp_obj*)__lwp_queue_get(&info->inactives);
	if(object) info->inactives_cnt--;
	return object;
}

void __lwp_objmgr_free(lwp_objinfo *info,lwp_obj *object)
{
	__lwp_queue_append(&info->inactives,&object->node);
	info->inactives_cnt++;
}
