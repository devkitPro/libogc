#ifndef __AESNDLIB_H__
#define __AESNDLIB_H__

#include <gctypes.h>

#define MAX_VOICES				32
#define SND_BUFFERSIZE			384				// output 2ms sound data at 48KHz
#define	DSP_STREAMBUFFER_SIZE	1152			// input 2ms sound data at max. 144KHz

#if defined(HW_DOL)
	#define DSP_DEFAULT_FREQ	(54000000.0f/1124.0f)
#elif defined(HW_RVL)
	#define DSP_DEFAULT_FREQ	48000
#endif

#define VOICE_STATE_STOPPED		0
#define VOICE_STATE_RUNNING		1
#define VOICE_STATE_STREAM		2

#define VOICE_MONO8             0x00000000
#define VOICE_STEREO8           0x00000001
#define VOICE_MONO16            0x00000002
#define VOICE_STEREO16          0x00000003
#define VOICE_MONO8_UNSIGNED    0x00000004
#define VOICE_STEREO8_UNSIGNED  0x00000005
#define VOICE_MONO16_UNSIGNED   0x00000006
#define VOICE_STEREO16_UNSIGNED	0x00000007

#define VOICE_FREQ32KHZ			32000
#define VOICE_FREQ48KHZ			48000

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct aesndpb_t AESNDPB;

typedef void (*AESNDVoiceCallbackArg)(AESNDPB *pb,u32 state,void *cbArg);
typedef void (*AESNDVoiceCallback)(AESNDPB *pb,u32 state);
typedef void (*AESNDAudioCallbackArg)(void *audio_buffer,u32 len,void *cbArg);
typedef void (*AESNDAudioCallback)(void *audio_buffer,u32 len);

void AESND_Init(void);
void AESND_Reset(void);
void AESND_Pause(bool pause);
u32 AESND_GetDSPProcessTime(void);
f32 AESND_GetDSPProcessUsage(void);
AESNDAudioCallbackArg AESND_RegisterAudioCallbackWithArg(AESNDAudioCallbackArg cb, void *cbArg);
static inline AESNDAudioCallback AESND_RegisterAudioCallback(AESNDAudioCallback cb)
{
	return (AESNDAudioCallback)AESND_RegisterAudioCallbackWithArg((AESNDAudioCallbackArg)cb,NULL);
}

AESNDPB* AESND_AllocateVoiceWithArg(AESNDVoiceCallbackArg cb, void *cbArg);
static inline AESNDPB* AESND_AllocateVoice(AESNDVoiceCallback cb)
{
	return AESND_AllocateVoiceWithArg((AESNDVoiceCallbackArg)cb,NULL);
}
void AESND_FreeVoice(AESNDPB *pb);
void AESND_SetVoiceStop(AESNDPB *pb,bool stop);
void AESND_SetVoiceMute(AESNDPB *pb,bool mute);
void AESND_SetVoiceLoop(AESNDPB *pb,bool loop);
void AESND_SetVoiceFormat(AESNDPB *pb,u32 format);
void AESND_SetVoiceStream(AESNDPB *pb,bool stream);
void AESND_SetVoiceFrequency(AESNDPB *pb,f32 freq);
void AESND_SetVoiceVolume(AESNDPB *pb,u16 volume_l,u16 volume_r);
void AESND_SetVoiceDelay(AESNDPB *pb,u32 delay);
void AESND_SetVoiceBuffer(AESNDPB *pb,const void *buffer,u32 len);
void AESND_PlayVoice(AESNDPB *pb,u32 format,const void *buffer,u32 len,f32 freq,u32 delay,bool looped);
AESNDVoiceCallbackArg AESND_RegisterVoiceCallbackWithArg(AESNDPB *pb,AESNDVoiceCallbackArg cb,void *cbArg);
static inline AESNDVoiceCallback AESND_RegisterVoiceCallback(AESNDPB *pb,AESNDVoiceCallback cb)
{
	return (AESNDVoiceCallback)AESND_RegisterVoiceCallbackWithArg(pb,(AESNDVoiceCallbackArg)cb,NULL);
}

#ifdef __cplusplus
	}
#endif

#endif
