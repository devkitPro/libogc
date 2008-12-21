/*
Copyright (c) 2002,2003, Christian Nowak <chnowak@web.de>
All rights reserved.

Modified by Francisco Mu�oz 'Hermes' MAY 2008

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

/* mixer.c */

#include "defines.h"
#include "modplay.h"

#if defined(DREAMCAST)
#define PREFILL_WORD 0
#elif defined(MINGW)
#define PREFILL_WORD 32768
#elif defined(ALLEGRO)
#define PREFILL_WORD 32768
#elif defined(GP32)
#define PREFILL_WORD 32768
#elif defined(GEKKO)
#define PREFILL_WORD 0
#else
#define PREFILL_WORD 32768
#endif


#define MIX_SAMPLES \
				accum = (s32)b[i] + (s32)((((s32)data[playpos.aword.high]*volume)>>6) << shiftval); \
				if(accum<-32768) accum = -32768; if(accum>32767) accum = 32767; \
                b[i] = accum; \
                playpos.adword+=incval; \
                if ( playpos.adword>=loop_end ) \
                  { \
                    if (mod->instrument[mod->instnum[voice]].looped) \
                      playpos.adword -= mod->instrument[mod->instnum[voice]].loop_length<<16; \
                    else \
                      { \
                        playpos.adword = (mod->instrument[mod->instnum[voice]].loop_end-1)<<16; \
                        mod->channel_active[voice] = FALSE; \
                        break; \
                      } \
                  }

s32 mix_mono_16bit ( MOD * mod, s16 * buf, s32 numSamples )
  {
    s32 accum;
    s32 voice, i, j;
    s16 * b = buf;
    s32 shiftval = mod->shiftval;
    s32 numIterations;
    s32 numIterationsRest;
    
    numIterations = numSamples>>4;
    numIterationsRest = numSamples&((1<<4)-1);


    for (i=j=0;j<numIterations;j++)
      {
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
        buf[i] = PREFILL_WORD;i++;
      }
    for (j=0;j<numIterationsRest;j++,i++)
      buf[i] = PREFILL_WORD;


    for (voice=0;voice<mod->num_channels;++voice)
      {
        if (mod->instrument[mod->instnum[voice]].data != NULL && mod->channel_active[voice])
          {
            u32 incval,noteidx;

			noteidx = (mod->chanfreq[voice] - mod->chanfreq[voice]*2*(mod->instrument[mod->instnum[voice]].finetune-8)/256);
			incval = mod->inctab[noteidx];

            s8 * data = mod->instrument[mod->instnum[voice]].data;
            union_dword playpos;
            u32 loop_end = mod->instrument[mod->instnum[voice]].loop_end<<16;
            s32 volume = mod->volume[voice];
            playpos.adword = mod->playpos[voice];

            if ( voice<mod->num_voices )
              volume = (volume*(s32)mod->musicvolume)>>6;
            else
              volume = (volume*(s32)mod->sfxvolume)>>6;

            if (mod->freq==32000 || mod->freq==48000)
              incval >>= 2;

            for (j=i=0;j<numIterations;j++)
              {
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
                MIX_SAMPLES;i++;
              }
            for (j=0;j<numIterationsRest;j++,i++)
              {
                MIX_SAMPLES;
              }
            mod->playpos[voice] = playpos.adword;
          }
      }
    return numSamples;
  }

s32 mix_stereo_16bit ( MOD * mod, s16 * buf, s32 numSamples )
  {
    s32 accum;
    s32 voice, i, j;
    s16 * b = buf;
    s32 shiftval = mod->shiftval+1;
    s32 numIterations, numIterationsRest;

    numIterations = (numSamples)>>2;
    numIterationsRest = (numSamples)&((1<<2)-1);

    for (i=j=0;j<numIterations;j++)
      {
        buf[i] = buf[i+1] = PREFILL_WORD;i+=2;
        buf[i] = buf[i+1] = PREFILL_WORD;i+=2;
        buf[i] = buf[i+1] = PREFILL_WORD;i+=2;
        buf[i] = buf[i+1] = PREFILL_WORD;i+=2;
      }
    for (j=0;j<numIterationsRest;j++,i+=2)
        buf[i] = buf[i+1] = PREFILL_WORD;

/*    for (j=0;j<numSamples*2;j+=2)
      buf[j] = buf[j+1] = PREFILL_WORD;*/

    for (voice=0;voice<mod->num_channels;++voice)
      {
        s32 lrofs = (((voice-1)>>1)&1)^1;
        if (mod->instrument[mod->instnum[voice]].data != NULL && mod->channel_active[voice])
          {
            u32 incval,noteidx;

			noteidx = (mod->chanfreq[voice] - mod->chanfreq[voice]*2*(mod->instrument[mod->instnum[voice]].finetune-8)/256);
			incval = mod->inctab[noteidx];

            s8 * data = mod->instrument[mod->instnum[voice]].data;
            union_dword playpos;
            u32 loop_end = mod->instrument[mod->instnum[voice]].loop_end<<16;
            s32 volume = mod->volume[voice];
            
            if ( voice<mod->num_voices )
              volume = (volume*(s32)mod->musicvolume)>>6;
            else
              volume = (volume*(s32)mod->sfxvolume)>>6;

            playpos.adword = mod->playpos[voice];            
            if (mod->freq==32000 || mod->freq==48000)
              incval >>= 2;
            
            i = lrofs;
            for (j=0;j<numIterations;j++)
              {
                MIX_SAMPLES;i+=2;
                MIX_SAMPLES;i+=2;
                MIX_SAMPLES;i+=2;
                MIX_SAMPLES;i+=2;
              }
            for (j=0;j<numIterationsRest;j++)
              {
                MIX_SAMPLES;i+=2;
              }
/*            for (i=0;i<numSamples*2;i+=2)
              {
                MIX_SAMPLES;
              }*/
            mod->playpos[voice] = playpos.adword;
          }
      }

    return numSamples;
  }
