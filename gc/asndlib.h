/* ASNDLIB -> accelerated sound lib using the DSP

Copyright (c) 2008 Hermes <www.entuwii.net>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of
  conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list
  of conditions and the following disclaimer in the documentation and/or other
  materials provided with the distribution.
- The names of the contributors may not be used to endorse or promote products derived
  from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#ifndef __ASNDLIB_H__
#define __ASNDLIB_H__

#define __SNDLIB_H__

#define ASND_LIB 0x100
#define SND_LIB  (ASND_LIB+2)

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// SND return values

#define SND_OK               0
#define SND_INVALID         -1
#define SND_ISNOTASONGVOICE -2
#define SND_BUSY             1

// SND_IsActiveVoice additional return values

#define SND_UNUSED   0   // you can use this voice
#define SND_WORKING  1   // this voice is in progress
#define SND_WAITING  2   // this voice is in progress and waiting to one SND_AddVoice function (the voice handler is called continuously)

// SND_SetVoice format
#define VOICE_MONO_8BIT    0
#define VOICE_MONO_16BIT   1
#define VOICE_STEREO_8BIT  2
#define VOICE_STEREO_16BIT 3


// Voice volume Range

#define MIN_VOLUME 0
#define MID_VOLUME 127
#define MAX_VOLUME 255

// Pitch Range

#define MIN_PITCH      1      // 1 Hz

#define F44100HZ_PITCH 44100  // 44100Hz

#define MAX_PITCH      144000 // 144000Hz (more process time for pitch>48000)

#define INIT_RATE_48000

// note codification

enum
{
NOTE_DO=0,
NOTE_DOs,
NOTE_REb=NOTE_DOs,
NOTE_RE,
NOTE_REs,
NOTE_MIb=NOTE_REs,
NOTE_MI,
NOTE_FA,
NOTE_FAs,
NOTE_SOLb=NOTE_FAs,
NOTE_SOL,
NOTE_SOLs,
NOTE_LAb=NOTE_SOLs,
NOTE_LA,
NOTE_LAs,
NOTE_SIb=NOTE_LAs,
NOTE_SI
};

enum
{
NOTE_C=0,
NOTE_Cs,
NOTE_Db=NOTE_Cs,
NOTE_D,
NOTE_Ds,
NOTE_Eb=NOTE_Ds,
NOTE_E,
NOTE_F,
NOTE_Fs,
NOTE_Gb=NOTE_Fs,
NOTE_G,
NOTE_Gs,
NOTE_Ab=NOTE_Gs,
NOTE_A,
NOTE_As,
NOTE_Bb=NOTE_As,
NOTE_B
};

#define NOTE(note,octave) (note+(octave<<3)+(octave<<2)) // final note codification. Used in Note2Freq()

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

// compat with SNDLIB

#define Note2Freq               ANote2Freq
#define SND_Init                ASND_Init
#define SND_End                 ASND_End
#define SND_Pause               ASND_Pause
#define SND_Is_Paused           ASND_Is_Paused
#define SND_GetTime             ASND_GetTime
#define SND_GetSampleCounter    ASND_GetSampleCounter
#define SND_GetSamplesPerTick   ASND_GetSamplesPerTick
#define SND_SetTime             ASND_SetTime
#define SND_SetCallback         ASND_SetCallback
#define SND_GetAudioRate        ASND_GetAudioRate
#define SND_SetVoice            ASND_SetVoice
#define SND_AddVoice            ASND_AddVoice
#define SND_StopVoice           ASND_StopVoice
#define SND_PauseVoice          ASND_PauseVoice
#define SND_StatusVoice         ASND_StatusVoice
#define SND_GetFirstUnusedVoice ASND_GetFirstUnusedVoice
#define SND_ChangePitchVoice    ASND_ChangePitchVoice
#define SND_ChangeVolumeVoice   ASND_ChangeVolumeVoice
#define SND_ChangeVolumeVoice   ASND_ChangeVolumeVoice
#define SND_GetTickCounterVoice ASND_GetTickCounterVoice
#define SND_GetTimerVoice       ASND_GetTimerVoice
#define SND_TestPointer         ASND_TestPointer

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

typedef void (*ASNDVoiceCallback)(s32 voice);

/* int ANote2Freq(int note, int freq_base,int note_base);

Initializes the SND lib and fix the hardware sample rate.

-- Params ---

note: Note codified to play. For example: NOTE(C,4) for note C and octave 4

freq_base: Frequency base of the sample. For example 8000Hz

note_base: Note codified of the sample. For example: NOTE(L,3) for note L and octave 3 (LA 3)

return: frequency (in Hz)

*/

int ANote2Freq(int note, int freq_base,int note_base);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* General Sound Functions                                                                                                                              */
/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* void ASND_Init();

Initializes the ASND lib and fix the hardware sample rate to 48000.

-- Params ---


return: nothing

*/

void ASND_Init();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* void ASND_End();

De-initializes the ASND lib.

return: nothing

*/

void ASND_End();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* void ASND_Pause(s32 paused);

Used to pause (or not) the sound. When you call to the ASND_Init() function, the sound is paused.


-- Params ---

paused: use 0 to run or 1 to pause

return: nothing

*/

void ASND_Pause(s32 paused);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_Is_Paused();

Return if the sound is paused or not

-- Params ---

return: 0-> running 1-> paused

*/


s32 ASND_Is_Paused();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* u32 ASND_GetTime();

Get the global time (in milliseconds). This time is updated from the IRQ

-- Params ---


return: current time (in ms)

*/

u32 ASND_GetTime();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* u32 ASND_GetSampleCounter();

Get the global sample counter. This counter is updated from the IRQ in steps of ASND_GetSamplesPerTick()

NOTE: you can use this to implement one timer with high precision

-- Params ---


return: current sample

*/


u32 ASND_GetSampleCounter();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* u32 ASND_GetSamplesPerTick();

Get the samples sended from the IRQ in one tick

-- Params ---


return: samples per tick

*/


u32 ASND_GetSamplesPerTick();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* void ASND_SetTime(u32 time);

Set the global time (in milliseconds). This time is updated from the IRQ

-- Params ---

time: fix the current time (in ms)

return: nothing

*/

void ASND_SetTime(u32 time);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* void ASND_SetCallback(void (*callback)());

Set a global callback for general pourpose. This callback is called from the IRQ

-- Params ---

callback: function callback

return: nothing

*/

void ASND_SetCallback(void (*callback)());

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_GetAudioRate();

return: Audio rate (48000)

Note: for compatibility with SND_lib

*/

s32 ASND_GetAudioRate();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Voice Functions                                                                                                                                      */
/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_SetVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r, void (*callback) (s32 voice));

Set a PCM voice to play. This function stops one previously voice. Use the ASND_StatusVoice() to test the status condition

NOTE: The voices are played in stereo and 16 bits independently of the source format.

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

format: PCM format from VOICE_MONO_8BIT to VOICE_STEREO_16BIT

pitch: pitch frequency (in Hz)

delay: delay time in milliseconds (ms). Time to wait before to play the voice

snd: buffer containing the samples (aligned and padded to 32 bytes!!!)

size_snd: size (in bytes) of the buffer samples

volume_l: volume to the left channel from 0 to 255

volume_r: volume to the right channel from 0 to 255

callback: can be NULL or one callback function is used to implement a double buffer use. When the second buffer is empty, the callback is called sending
          the voice number as parameter. You can use "void callback(s32 voice)" function to call ASND_AddVoice() and add one voice to the second buffer.
		  NOTE: When callback is fixed the voice never stops and it turn in SND_WAITING status if success one timeout condition.

return: SND_OK or SND_INVALID

*/

s32 ASND_SetVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r, ASNDVoiceCallback callback);
s32 ASND_SetInfiniteVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_AddVoice(s32 voice, void *snd, s32 size_snd);

Add a PCM voice in the second buffer to play. This function requires one previously call to ASND_SetVoice() and one condition status different
        of 'SND_UNUSED'

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

snd: buffer containing the samples (aligned and padded to 32 bytes!) in the same format of the previously ASND_SetVoice() use

size_snd: size (in bytes) of the buffer samples

return: SND_OK, SND_INVALID or SND_BUSY if the second buffer is not empty and de voice cannot be add

*/

s32 ASND_AddVoice(s32 voice, void *snd, s32 size_snd);


/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_StopVoice(s32 voice);

Stops the voice selected.

If the voice is used in song mode, you need to assign the samples with ASND_SetSongSampleVoice() again. Use ASND_PauseSongVoice() in this case to stops
        the voice without lose the samples.

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: SND_OK, SND_INVALID

*/

s32 ASND_StopVoice(s32 voice);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_PauseVoice(s32 voice, s32 pause);

Pause the voice selected.



-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: SND_OK, SND_INVALID

*/

s32 ASND_PauseVoice(s32 voice, s32 pause);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_StatusVoice(s32 voice);

Return the status of the voice selected

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: SND_INVALID
        SND_UNUSED   you can use this voice
        SND_WORKING  this voice is in progress
        SND_WAITING  this voice is in progress and waiting to one SND_AddVoice function (the voice handler is called continuously)

*/

s32 ASND_StatusVoice(s32 voice);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_GetFirstUnusedVoice();

Get the first unused voice. The voice 0 is tried especially and it is the last possible result. The idea is to reserve that voice for a Mod/Ogg/MP3
       Player or similar. So if the function return a value <1 you can be sure the rest of the voices are working.

-- Params ---

None

return: SND_INVALID or the first free voice (from 0 to (MAX_SND_VOICES-1))

*/

s32 ASND_GetFirstUnusedVoice();

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_ChangePitchVoice(s32 voice, s32 pitch);

Use this function to change the voice pitch in real-time. You can use this to create audio effects.

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: SND_OK, SND_INVALID

*/

s32 ASND_ChangePitchVoice(s32 voice, s32 pitch);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 SND_ChangeVolumeVoice(s32 voice, s32 volume_l, s32 volume_r);

Use this function to change the voice volume in real-time. You can use this to create audio effects.

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

volume_l: volume to the left channel from 0 to 255

volume_r: volume to the right channel from 0 to 255

return: SND_OK, SND_INVALID

*/

s32 ASND_ChangeVolumeVoice(s32 voice, s32 volume_l, s32 volume_r);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* u32 SND_GetTickCounterVoice(s32 voice);

Get the tick counter from the voice starts to play (without the delay time). This counter uses the same resolution of the internal sound buffer.
       For example if the lib is initilized with INIT_RATE_48000 a return value of 24000 are equal to 0.5 seconds played

USES: you can use this value to synchronize audio & video


-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: Number of ticks. No error condition for this function

*/

u32 ASND_GetTickCounterVoice(s32 voice);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* u32 ASND_GetTimerVoice(s32 voice);

Get the time (in milliseconds) from the voice starts to play (without the delay time).

USES: you can use this value to synchronize audio & video

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: time (in ms). No error condition for this function

*/

u32 ASND_GetTimerVoice(s32 voice);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 SND_TestPointer(s32 voice, void *pointer);

Test if the pointer is in use by the voice (as buffer).

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

pointer: address to test It must be the same pointer passed to the ASND_AddVoice() or ASND_SetVoice() functions

return: SND_INVALID, 0-> Unused, 1-> Used as buffer

*/


s32 ASND_TestPointer(s32 voice, void *pointer);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* s32 ASND_TestVoiceBufferReady(s32 voice);

Test if the Voice is Ready to receive a new buffer sample with ASND_AddVoice(). You can use this function to block a reader when you use double buffering
as similar form of the ASND_TestPointer() function without passing one address to test

-- Params ---

voice: use one from 0 to (MAX_SND_VOICES-1)

return: SND_INVALID, 0-> not ready to receive a new AddVoice(), 1->ready to receive a new AddVoice()

*/

s32 ASND_TestVoiceBufferReady(s32 voice);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* DSP FUNCTIONS			                                                                                                                            */
/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* u32 ASND_GetDSP_PercentUse();

Get the Percent use of the DSP

-- Params ---

None

return: percent use

*/

u32 ASND_GetDSP_PercentUse();


/*------------------------------------------------------------------------------------------------------------------------------------------------------*/


#ifdef __cplusplus
  }
#endif

#endif

