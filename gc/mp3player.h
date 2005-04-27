#ifndef __MP3PLAYER_H__
#define __MP3PLAYER_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

int MP3Player_Play ( const void * MP3Stream, u32 Length );
bool MP3Player_IsPlaying ();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
