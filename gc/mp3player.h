#ifndef __MP3PLAYER_H__
#define __MP3PLAYER_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s32 MP3Player_Play(const void *mp3stream, u32 len);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
