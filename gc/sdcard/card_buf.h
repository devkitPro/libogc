#ifndef __CARD_BUF_H__
#define __CARD_BUF_H__

#include <gctypes.h>

#ifdef __cplusplus
	extern "C" {
#endif

void card_initBufferPool();
u8*	card_allocBuffer();
void card_freeBuffer(u8 *buf);

#ifdef __cplusplus
	}
#endif

#endif
