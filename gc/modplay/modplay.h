/*
Copyright (c) 2002,2003, Christian Nowak <chnowak@web.de>
All rights reserved.

Modified by Francisco Muñoz 'Hermes' MAY 2008

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

#ifndef __MODPLAY_H__
#define __MODPLAY_H__

#include <gctypes.h>

#define MAX_VOICES  32

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _modinstr
  {
    s8 name[23];    /* 000..021 */
    u32 length;       /* 022..023 */
    u8 finetune;      /* 024 */
    u8 volume;        /* 025 */
    u32 loop_start;   /* 026..027 */
    u32 loop_end;     /* 028..029 */
    u32 loop_length;
    BOOL looped;
    s8 * data;
  } MOD_INSTR;

typedef struct _mod
  {
    BOOL loaded;
    s8 name[21];
    MOD_INSTR instrument[31];
    s32 num_patterns;
    u8 song_length;
    u8 ciaa;
    u8 song[128];
    s8 id[4];
    u8 * patterndata;
    s32 num_instr;
    s32 num_voices;   /* Number of voices in the MOD */
    s32 num_channels; /* Number of channels to actually mix (num_channels-num_voices = number of sfx channels) */
    u8 * mixingbuf;
    s32 mixingbuflen;
    s32 shiftval;     /* Number of bits to lshift every mixed 16bit word by */
    /* Player variables */
    BOOL channel_active[MAX_VOICES];
    s32 patterndelay;
    s32 speed;
    s32 bpm;
    s32 songpos;      /* In the song */
    s32 patternline;  /* In the pattern */
    s32 patternline_jumpto;     /* For the E6 effect */
    s32 patternline_jumpcount;  /* For the E6 effect */
    s32 speedcounter;
    s32 freq;
    s32 bits;
    s32 channels;     /* 1 = mono, 2 = stereo */
    u32 playpos[MAX_VOICES];   /* Playing position for each channel */
    u8 instnum[MAX_VOICES];    /* Current instrument */
    u16 chanfreq[MAX_VOICES];  /* Current frequency */
    u16 channote[MAX_VOICES];  /* Last note triggered */
    u8 volume[MAX_VOICES];     /* Current volume */
    u8 effect[MAX_VOICES];     /* Current effect */
    u8 effectop[MAX_VOICES];   /* Current effect operand */
    u8 last_effect[MAX_VOICES];
    /* Effects handling */
    u16 portamento_to[MAX_VOICES];
    u8 porta_speed[MAX_VOICES];
    u8 retrigger_counter[MAX_VOICES];
    u8 arp_counter;
    u8 sintabpos[MAX_VOICES];
    u8 vib_freq[MAX_VOICES];
    u8 vib_depth[MAX_VOICES];
    u16 vib_basefreq[MAX_VOICES];
    u8 trem_basevol[MAX_VOICES];
    u8 trem_freq[MAX_VOICES];
    u8 trem_depth[MAX_VOICES];
    BOOL glissando[MAX_VOICES];
    u8 trem_wave[MAX_VOICES];
    u8 vib_wave[MAX_VOICES];

    u8 nextinstr[MAX_VOICES];
    u16 nextnote[MAX_VOICES];
    
    u32 samplespertick;
    u32 samplescounter;
    
    u8 * modraw;
	u32 *bpmtab;
	u32 *inctab;
	
    u32 notebeats;
    void (*callback)(void*);
    
    u8 musicvolume;
    u8 sfxvolume;
    
    BOOL set;
    BOOL *notify;

  } MOD;

s32 MOD_SetMOD ( MOD *, u8 * );
s32 MOD_Load ( MOD *, s8 * );
void MOD_Free ( MOD * );
void MOD_Start ( MOD * );
u32 MOD_Player ( MOD * );
s32 MOD_TriggerNote ( MOD *, s32, u8, u16, u8 );
s32 MOD_AllocSFXChannels ( MOD *, s32 );

u16 getNote ( MOD *, s32, s32 );
u8 getInstr ( MOD *, s32, s32 );
u8 getEffect ( MOD *, s32, s32 );
u8 getEffectOp ( MOD *, s32, s32 );

#ifdef __cplusplus
  }
#endif

#endif

