#include <stdio.h>
#include <string.h>

#include "fat_cmn.h"
#include "fat_buf.h"

#define BUF_POOL_CNT			3

typedef struct _buf_node {
	struct _buf_node *next;
	u8 data[SECTOR_SIZE+2];
	
} BufNode;

static BufNode s_buf[BUF_POOL_CNT];
static BufNode *s_freepool;

void fat_bufferpoolinit()
{
	u32 i;

	for(i=0;i<BUF_POOL_CNT-1;++i) {
		s_buf[i].next = s_buf+i+1;
	}
	s_buf[i].next = NULL;
	s_freepool = s_buf;
}

u8*	fat_allocbuffer()
{
	u8 *buf = NULL;
	
	if(s_freepool) {
		buf = s_freepool->data;
		s_freepool = s_freepool->next;
	}

	return buf;
}

void fat_freebuffer(u8 *buf)
{
	if(buf) {
		BufNode *node = (BufNode*)(buf-offsetof(BufNode,data));
		node->next = s_freepool;
		s_freepool = node;
	}
}
