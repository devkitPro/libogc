#ifndef __MP3PLAYER_H__
#define __MP3PLAYER_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void MP3Player_Init();
s32 MP3Player_Play(const void *mp3stream, u32 len);
void MP3Player_Stop();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
