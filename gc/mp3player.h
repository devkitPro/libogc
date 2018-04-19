#ifndef __MP3PLAYER_H__
#define __MP3PLAYER_H__

#include <mad.h>
#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void MP3Player_Init(void);
void MP3Player_Stop(void);
BOOL MP3Player_IsPlaying(void);
void MP3Player_Volume(u32 volume);
s32 MP3Player_PlayBuffer(const void *buffer,s32 len,void (*filterfunc)(struct mad_stream *,struct mad_frame *));
s32 MP3Player_PlayFile(void *cb_data,s32 (*reader)(void *,void *,s32),void (*filterfunc)(struct mad_stream *,struct mad_frame *));

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
