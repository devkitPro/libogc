#ifndef __DVD_H__
#define __DVD_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*DVDCallback)(u32);

void DVD_Init();
s32 DVD_Read(void *pDst,s32 nLen,u32 nOffset);
s32 DVD_ReadId(void *pDst);
void DVD_Reset();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
