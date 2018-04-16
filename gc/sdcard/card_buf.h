#ifndef __CARD_BUF_H__
#define __CARD_BUF_H__

#include <gctypes.h>

#ifdef __cplusplus
	extern "C" {
#endif

void sdgecko_initBufferPool(void);
u8*	sdgecko_allocBuffer(void);
void sdgecko_freeBuffer(u8 *buf);

#ifdef __cplusplus
	}
#endif

#endif
