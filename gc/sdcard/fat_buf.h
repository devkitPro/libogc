#ifndef __FAT_BUF_H__
#define __FAT_BUF_H__

#include <gctypes.h>

#ifdef __cplusplus
	extern "C" {
#endif

void fat_bufferpoolinit();
u8*	fat_allocbuffer();
void fat_freebuffer(u8 *buf);

#ifdef __cplusplus
	}
#endif

#endif
