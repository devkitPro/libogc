#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <gctypes.h>

#define AI_STREAM_STOP			0x00000000
#define AI_STREAM_START			0x00000001   // Audio streaming sample clock

#define AI_SAMPLERATE_32KHZ		0x00000000
#define AI_SAMPLERATE_48KHZ		0x00000001   // SRC sample rates for DSP

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*AIDCallback)(void);
typedef void (*AISCallback)(u32);


AISCallback AUDIO_RegisterStreamCallback(AISCallback callback);
void AUDIO_Init(u8 *stack);
void AUDIO_SetPlayerCallback(void (*)(void **,s32 *));
void AUDIO_SetStreamVolLeft(u8 vol);
u8 AUDIO_GetStreamVolLeft();
void AUDIO_SetStreamVolRight(u8 vol);
u8 AUDIO_GetStreamVolRight();
void AUDIO_SetStreamSampleRate(u32 rate);
u32 AUDIO_GetStreamSampleRate();
AIDCallback AUDIO_RegisterDMACallback(AIDCallback callback);
void AUDIO_InitDMA(u32 startaddr,u32 len);
u16 AUDIO_GetDMAEnableFlag();
void AUDIO_StartDMA();
void AUDIO_StopDMA();
u32 AUDIO_GetDMABytesLeft();
u32 AUDIO_GetDMALength();
u32 AUDIO_GetDMAStartAddr();
void AUDIO_SetStreamTrigger(u32 cnt);
void AUDIO_ResetStreamSampleCnt();
void AUDIO_SetDSPSampleRate(u8 rate);
u32 AUDIO_GetDSPSampleRate();
void AUDIO_SetStreamPlayState(u32 state);
u32 AUDIO_GetStreamPlayState();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
