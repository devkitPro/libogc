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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defines.h"
#include "semitonetab.h"
#include "freqtab.h"
#include "modplay.h"
#include "mixer.h"
#ifndef GEKKO
#include "bpmtab.h"
#include "inctab.h"
#endif

#ifdef GP32
#include "gpstdlib.h"
#include "gpstdio.h"
/*
volatile long* lcdcon1 = (long*)0x14a00000;
#define VLINE ((*lcdcon1 >> 18) & 0x1ff)
extern int deltavline;
*/
#endif

#ifdef GEKKO
#include "video.h"

static u32 *inc_tabs[2] = {NULL,NULL};
static u32 *bpm_tabs[2] = {NULL,NULL};
#endif

#ifdef GP32
#include "gpmem.h"
#define MEM_CPY(a,b,c) gm_memcpy(a,b,c)
#define MEM_SET(a,b,c) gm_memset(a,b,c)
/*#define MEM_CMP(a,b,c) gm_memcmp(a,b,c)*/
static s32 MEM_CMP ( void * a, void * b, s32 l )
  {
    s32 i;
    
    for (i=0;i<l;i++)
      if (((u8*)a)[i]!=((u8*)b)[i])
        return -1;
    return 0;
  }
#else
#define MEM_CPY(a,b,c) memcpy(a,b,c)
#define MEM_SET(a,b,c) memset(a,b,c)
#define MEM_CMP(a,b,c) memcmp(a,b,c)
#endif

#ifdef DREAMCAST
#include <kos.h>
#endif

#define MINI(a,b) ((a)<(b)?(a):(b))
#define MAXI(a,b) ((a)>(b)?(a):(b))

s32 shiftvals[33] =
  {
    0,8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
  };

s16 wavetab[4][64] =
  {
    { /* Sine */
      (s16)0,    (s16)24,  (s16)49,  (s16)74,  (s16)97,  (s16)120, (s16)141, (s16)161,
      (s16)180,  (s16)197, (s16)212, (s16)224, (s16)235, (s16)244, (s16)250, (s16)253,
      (s16)255,  (s16)253, (s16)250, (s16)244, (s16)235, (s16)224, (s16)212, (s16)197,
      (s16)180,  (s16)161, (s16)141, (s16)120, (s16)97,  (s16)74,  (s16)49,  (s16)24,
      (s16)0,    (s16)-24, (s16)-49, (s16)-74, (s16)-97, (s16)-120,(s16)-141,(s16)-161,
      (s16)-180,(s16)-197,(s16)-212,(s16)-224,(s16)-235,(s16)-244,(s16)-250,(s16)-253,
      (s16)-255,(s16)-253,(s16)-250,(s16)-244,(s16)-235,(s16)-224,(s16)-212,(s16)-197,
      (s16)-180,(s16)-161,(s16)-141,(s16)-120,(s16)-97, (s16)-74, (s16)-49, (s16)-24
    },
    { /* Ramp down */
      (s16)255, (s16)247, (s16)239, (s16)231, (s16)223, (s16)215, (s16)207, (s16)199, 
      (s16)191, (s16)183, (s16)175, (s16)167, (s16)159, (s16)151, (s16)143, (s16)135, 
      (s16)127, (s16)119, (s16)111, (s16)103, (s16)95,  (s16)87,  (s16)79,  (s16)71,
      (s16)63,  (s16)55,  (s16)47,  (s16)39,  (s16)31,  (s16)23,  (s16)15,  (s16)7,
      (s16)-1,  (s16)-9,  (s16)-17, (s16)-25, (s16)-33, (s16)-41, (s16)-49, (s16)-57,
      (s16)-65, (s16)-73, (s16)-81, (s16)-89, (s16)-97, (s16)-105,(s16)-113,(s16)-121,
      (s16)-129,(s16)-137,(s16)-145,(s16)-153,(s16)-161,(s16)-169,(s16)-177,(s16)-185,
      (s16)-193,(s16)-201,(s16)-209,(s16)-217,(s16)-225,(s16)-233,(s16)-241,(s16)-249
    },
    { /* Square wave */
      (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255,
      (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255,
      (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255,
      (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255, (s16)255,
      (s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,
      (s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,
      (s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,
      (s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255,(s16)-255
    },
    { /* Random */
      (s16)-26, (s16)-251,(s16)-198,(s16)-46, (s16)-96, (s16)198, (s16)168, (s16)228,
      (s16)-49, (s16)-153,(s16)-236,(s16)-174,(s16)-37, (s16)61,  (s16)187, (s16)120,
      (s16)56,  (s16)-197,(s16)248, (s16)-58, (s16)-204,(s16)172, (s16)58,  (s16)253,
      (s16)-155,(s16)57,  (s16)62,  (s16)-62, (s16)60,  (s16)-137,(s16)-101,(s16)-184,
      (s16)66,  (s16)-160,(s16)160, (s16)-29, (s16)-91, (s16)243, (s16)175, (s16)-175,
      (s16)149, (s16)97,  (s16)3,   (s16)-113,(s16)7,   (s16)249, (s16)-241,(s16)-247,
      (s16)110, (s16)-180,(s16)-139,(s16)-20, (s16)246, (s16)-86, (s16)-80, (s16)-134,
      (s16)219, (s16)117, (s16)-143,(s16)-226,(s16)-166,(s16)120, (s16)-47, (s16)29
    }
  };

#if defined(GEKKO)
static u32* modplay_getinctab(s32 freq)
{
	u32 tv;
	u32 i,curr_tab,*inc_tab = NULL;
	f32 rfreq,fval,fdivid;

	if(freq==32000) curr_tab = 0;
	else curr_tab = 1;

	if(inc_tabs[curr_tab]) return inc_tabs[curr_tab];

	tv = VIDEO_GetCurrentTvMode();
	if(tv==VI_PAL) fdivid = 7093789.2/2.0F;
	else fdivid = 7159090.5/2.0F;

	inc_tab = (u32*)malloc(sizeof(u32)*4096);
	if(inc_tab) {
		inc_tabs[curr_tab] = inc_tab;
		for(i=0;i<4096;i++) {
			rfreq = (fdivid/(f32)i);
			fval = (rfreq/(f32)freq)*262144.0F;
			inc_tab[i] = (u32)fval;
		}

	}

	return inc_tab;
}

static u32* modplay_getbpmtab(s32 freq)
{
	u32 i,curr_tab,*bpm_tab = NULL;
	f32 fval;

	if(freq==32000) curr_tab = 0;
	else curr_tab = 1;

	if(bpm_tabs[curr_tab]) return bpm_tabs[curr_tab];

	bpm_tab = (u32*)malloc(sizeof(u32)*224);
	if(bpm_tab) {
		bpm_tabs[curr_tab] = bpm_tab;
		for(i=0;i<224;i++) {
			fval = ((f32)freq/((f32)(i+32)*0.4F));
			bpm_tab[i] = (u32)fval;
		}

	}

	return bpm_tab;
}
#endif

#if defined(GP32)
static s32 fsize ( s8 * fname )
  {
    u32 s;
    
    if (GpFileGetSize(fname,&s)!=SM_OK)
      return -1;
    else
      return (s32)s;
  }
#elif defined(DREAMCAST)
static s32 fsize ( s8 * fname )
  {
    file_t f;
    s32 s;
    
    if ((f=fs_open(fname, O_RDONLY))==0)
      return -1;
    
    fs_seek ( f, 0, SEEK_END );
    s = fs_tell ( f );
    fs_close ( f );
    
    return s;
  }
#elif defined(GAMECUBE)
static s32 fsize(s8 *fname)
{
	return 0;
}
#else
static s32 fsize ( s8 * fname )
  {
    FILE * f;
    s32 s;

    f = fopen (fname,"rb");
    if (f==NULL)
      return -1;

    fseek(f, 0, SEEK_END);
    s=ftell(f);
    fclose(f);

    return s;
  }
#endif

s32 MOD_AllocSFXChannels ( MOD * mod, s32 sfxchans )
  {
    if (mod->modraw==NULL)
      return -1;

    if (mod->num_voices+sfxchans>32)
      mod->num_channels = mod->num_voices;
    else
      mod->num_channels = mod->num_voices+sfxchans;
    mod->shiftval = shiftvals[mod->num_channels];
    
    return (mod->num_voices+sfxchans>32)?-1:0;
  }

#ifdef GP32
void MOD_Free ( MOD * mod )
  {
    if (!mod->loaded)
      return;

    if (mod->modraw!=NULL)
      gm_free ( mod->modraw );

    MEM_SET ( mod, 0, sizeof(MOD) );
    
    mod->loaded = FALSE;
  }
#else
void MOD_Free ( MOD * mod )
  {
    mod->set = FALSE;

    if (!mod->loaded)
      return;
    
    if (mod->modraw!=NULL)
      free ( mod->modraw );
    
    MEM_SET ( mod, 0, sizeof(MOD) );
    
    mod->loaded = FALSE;
  }
#endif

s32 MOD_SetMOD ( MOD * mod, u8 * mem )
  {
    s32 i;
    s32 ofs = 0;

    MEM_SET ( mod, 0, sizeof(MOD) );
    mod->modraw = mem;
    mod->musicvolume = mod->sfxvolume = 0x40;

    /* ID */
    mod->num_instr = 31;
    MEM_CPY ( mod->id, &mem[1080], 4 );
    mod->num_instr = 31;
    if ( (MEM_CMP(mod->id, "M.K.", 4)==0) ||
         (MEM_CMP(mod->id, "FLT4", 4)==0) )
      mod->num_voices = 4;
    else
    if ( MEM_CMP(mod->id, "2CHN", 4)==0 )
      mod->num_voices = 2;
    else
    if ( MEM_CMP(mod->id, "6CHN", 4)==0 )
      mod->num_voices = 6;
    else
    if ( MEM_CMP(mod->id, "8CHN", 4)==0 )
      mod->num_voices = 8;
    else
    if ( MEM_CMP(mod->id, "10CH", 4)==0 )
      mod->num_voices = 10;
    else
    if ( MEM_CMP(mod->id, "12CH", 4)==0 )
      mod->num_voices = 12;
    else
    if ( MEM_CMP(mod->id, "14CH", 4)==0 )
      mod->num_voices = 14;
    else
    if ( MEM_CMP(mod->id, "16CH", 4)==0 )
      mod->num_voices = 16;
    else
    if ( MEM_CMP(mod->id, "18CH", 4)==0 )
      mod->num_voices = 18;
    else
    if ( MEM_CMP(mod->id, "20CH", 4)==0 )
      mod->num_voices = 20;
    else
    if ( MEM_CMP(mod->id, "22CH", 4)==0 )
      mod->num_voices = 22;
    else
    if ( MEM_CMP(mod->id, "24CH", 4)==0 )
      mod->num_voices = 24;
    else
    if ( MEM_CMP(mod->id, "26CH", 4)==0 )
      mod->num_voices = 26;
    else
    if ( MEM_CMP(mod->id, "28CH", 4)==0 )
      mod->num_voices = 28;
    else
    if ( MEM_CMP(mod->id, "30CH", 4)==0 )
      mod->num_voices = 30;
    else
    if ( MEM_CMP(mod->id, "32CH", 4)==0 )
      mod->num_voices = 32;
    else
      {
        mod->num_instr = 15;
        mod->num_voices = 4;
      }

    /* Read song name */
    MEM_CPY ( mod->name, &mem[ofs], 20 ); ofs+=20;
    mod->name[20] = '\0';
    /* Read instruments */
    for (i=0;i<mod->num_instr;i++)
      {
        u32 tmp;
        s32 j;
        /* Name */
        MEM_CPY ( &mod->instrument[i].name, &mem[ofs], 22 ); ofs+=22;
        mod->instrument[i].name[22] = '\0';
        j=21;
        while ( mod->instrument[i].name[j]==' ' && j>=0 )
          {
            mod->instrument[i].name[j] = '\0';
            --j;
          }
        /* Length */
        mod->instrument[i].length = (mem[ofs+1] | (mem[ofs]<<8))<<1; ofs+=2;
        /* Fine tune */
        mod->instrument[i].finetune = mem[ofs++];
        mod->instrument[i].finetune += 8;
        mod->instrument[i].finetune &= 15;
        /* Volume */
        mod->instrument[i].volume = mem[ofs++];
        if (mod->instrument[i].volume > 0x40)
          mod->instrument[i].volume = 0x40;
        /* Loop start */
        mod->instrument[i].loop_start = (mem[ofs+1] | (mem[ofs]<<8))<<1; ofs+=2;
        /* Loop end */
        tmp = (mem[ofs+1] | (mem[ofs]<<8))<<1; ofs+=2;
        mod->instrument[i].loop_end = tmp + mod->instrument[i].loop_start;

        /* Is the sample looped ? */
        mod->instrument[i].looped = TRUE;
        if (tmp<=2)
          { /* No ! */
            mod->instrument[i].looped = FALSE;
            mod->instrument[i].loop_start = mod->instrument[i].loop_end =
              mod->instrument[i].length;
          }
        mod->instrument[i].loop_length = mod->instrument[i].loop_end - mod->instrument[i].loop_start;
      }
    /* Song length */
    mod->song_length = mem[ofs++];
    /* CIAA */
    mod->ciaa = mem[ofs++];
    /* Arrangement */
    MEM_CPY ( mod->song, &mem[ofs], 128 ); ofs+=128;
    mod->num_patterns = 0;
    for (i=0;i<128;i++)
      {
        if ( (mod->song[i] &= 63) > mod->num_patterns )
          mod->num_patterns = mod->song[i];
      }
    mod->num_patterns++;

    if (mod->num_instr!=15)
      ofs+=4;

    MOD_AllocSFXChannels ( mod, 0 );

    /* Patterns */
    mod->patterndata = &mem[ofs];
    ofs += 4*64*mod->num_voices*mod->num_patterns;
    /* Sample data */
    for (i=0;i<mod->num_instr;i++)
      {
        mod->instrument[i].data = NULL;
        if (mod->instrument[i].length!=0)
          {
            mod->instrument[i].data = (s8*)&mem[ofs];
            ofs += mod->instrument[i].length;
          }
      }

    mod->set = TRUE;

    return 0;
  } 

s32 MOD_Load ( MOD * mod, s8 * fname )
  {
#if defined(GP32)
    F_HANDLE fh;
    u32 tmp=0;
#elif defined(DREAMCAST)
    file_t fh;
#elif defined(GAMECUBE)
	void *fh;
#else
    FILE *fh;
#endif
    s32 fs;
    u8 * mem = NULL;

    fs = fsize(fname);
    if (fs<1080+1024)
      return -1;

#if defined(GP32)
    if (GpFileOpen(fname, OPEN_R, &fh)!=SM_OK)
      return -1;
    
    if ((mem=gm_calloc(1,fs))==NULL)
      {
        GpFileClose(fh);
        return -1;
      }
    
    GpFileRead ( fh, mem, fs, &tmp );
    GpFileClose(fh);
#elif defined(DREAMCAST)
    if ((fh=fs_open(fname, O_RDONLY))==0)
      return -1;
    
    if ((mem=calloc(1,fs))==NULL)
      {
        fs_close ( fh );
        return -1;
      }
    
    fs_read ( fh, mem, fs );
    fs_close ( fh );
#elif defined(GAMECUBE)
	fh = NULL;
#else
    if ((fh=fopen(fname,"rb"))==NULL)
      return -1;

    if ((mem=calloc(1,fs))==NULL)
      {
        fclose ( fh );
        return -1;
      }
    fread ( mem, fs, 1, fh );
    fclose ( fh );
#endif

    if ( MOD_SetMOD ( mod, mem ) < 0 )
      {
        MOD_Free ( mod );
        return -1;
      }
    mod->loaded = TRUE;

    return 0;
  }

u8 * getCurPatternData ( MOD * mod, s32 patternline, s32 channel )
  {
    return (&mod->patterndata[(((s32)mod->song[mod->songpos])*mod->num_voices*4*64) + ((patternline&63)*4*mod->num_voices) + (channel*4)]);
  }

u16 getNote ( MOD * mod, s32 patternline, s32 channel )
  {
    u8 * data = getCurPatternData(mod, patternline, channel);
    return (((data[0]&0x0f)<<8) | data[1]);
  }

u8 getInstr ( MOD * mod, s32 patternline, s32 channel )
  {
    u8 * data = getCurPatternData(mod, patternline, channel);
    return ((data[0]&0xf0) | ((data[2]>>4)&0x0f));
  }

u8 getEffect ( MOD * mod, s32 patternline, s32 channel )
  {
    u8 * data = getCurPatternData(mod, patternline, channel);
    return (data[2]&0x0f);
  }

u8 getEffectOp ( MOD * mod, s32 patternline, s32 channel )
  {
    u8 * data = getCurPatternData(mod, patternline, channel);
    return (data[3]);
  }

BOOL triggerNote ( MOD * mod, s32 i, u8 instrument, u16 note, u8 effect )
  {
    BOOL ret = FALSE;
    if ((instrument!=0) && (note!=0) && (effect!=3) && (effect!=5))
      {
        mod->playpos[i] = 0;
        mod->instnum[i] = instrument-1;
        mod->sintabpos[i] = 0;
        ret = TRUE;
      }
    if (note!=0)
      {
        ret = TRUE;
        mod->channote[i] = note;
        if ((effect==3) || (effect==5))
          mod->portamento_to[i] = note;
        else
          {
            if ((effect==4) || (effect==6))
              mod->vib_basefreq[i] = note;
            mod->playpos[i] = 0;
            mod->chanfreq[i] = note;
          }
      }
    if (instrument != 0)
      {
        ret = TRUE;
        mod->volume[i] = mod->instrument[mod->instnum[i]].volume;
      }
    return ret;
  }

/* Handle effect which must be on handled every tick */
u32 effect_handler ( MOD * mod )
  {
    s32 i;
    u32 retval = 0;

    if (mod->speedcounter==0)
      {
        mod->arp_counter = 0;
      } else
      {
        mod->arp_counter++;
        if (mod->arp_counter>3)
          mod->arp_counter = 1;
      }

    for (i=0;i<mod->num_voices;i++)
      {
        if ( mod->effect[i] < 0x10 )  /* Any effect ? */
          {
            switch ( mod->effect[i] )
              {
                case 0x00:  /* Arpeggio */
                  if (mod->speedcounter!=0)
                    {
                      if (mod->effectop[i] != 0)
                        {
                          if (mod->arp_counter==1)
                            {
                              mod->chanfreq[i] = freqtab[mod->channote[i]]+((mod->effectop[i]>>4)&0x0f);
                            } else
                          if (mod->arp_counter==2)
                            {
                              mod->chanfreq[i] = freqtab[mod->channote[i]]+(mod->effectop[i]&0x0f);
                            } else
                          if (mod->arp_counter==3)
                            {
                              mod->chanfreq[i] = freqtab[mod->channote[i]];
                            }
                        }
                    }
                  break;
                case 0x01:  /* Slide up */
                  if (mod->speedcounter!=0)
                    {
                      mod->chanfreq[i] -= mod->effectop[i];
                      if (mod->chanfreq[i] & 0x8000)
                        mod->chanfreq[i] = 0;
                    }
                  break;
                case 0x02:  /* Slide down */
                  if (mod->speedcounter!=0)
                    {
                      mod->chanfreq[i] += mod->effectop[i];
                      if (mod->chanfreq[i] > 4095)
                        mod->chanfreq[i] = 4095;
                    }
                  break;
                case 0x03:  /* Slide to */
                  if (mod->speedcounter!=0)
                    {
                      if ( mod->chanfreq[i] < mod->portamento_to[i] )
                        {
                          if ( (mod->portamento_to[i] - mod->chanfreq[i]) > mod->porta_speed[i] )
                            mod->chanfreq[i] += mod->porta_speed[i];
                          else
                            mod->chanfreq[i] = mod->portamento_to[i];
                        } else
                      if ( mod->chanfreq[i] > mod->portamento_to[i] )
                        {
                          if ( (mod->chanfreq[i] - mod->portamento_to[i]) > mod->porta_speed[i] )
                            mod->chanfreq[i] -= mod->porta_speed[i];
                          else
                            mod->chanfreq[i] = mod->portamento_to[i];
                        } else
                        {
                          mod->channote[i] = mod->chanfreq[i];
                        }
                    }
                  break;
                case 0x04:  /* Vibrato */
                  if (mod->speedcounter!=0)
                    {
                      mod->chanfreq[i] = (((s32)((s32)wavetab[mod->vib_wave[i]&3][mod->sintabpos[i]]*(u32)mod->vib_depth[i]))>>7) + mod->vib_basefreq[i];
                      mod->sintabpos[i] += mod->vib_freq[i];
                      mod->sintabpos[i] &= 63;
                    }
                  break;
                case 0x05:  /* Slide to & Volume siding */
                  if (mod->speedcounter!=0)
                    {
                      /* Do portamento */
                      if ( mod->chanfreq[i] < mod->portamento_to[i] )
                        {
                          if ( (mod->portamento_to[i] - mod->chanfreq[i]) > mod->porta_speed[i] )
                            mod->chanfreq[i] += mod->porta_speed[i];
                          else
                            mod->chanfreq[i] = mod->portamento_to[i];
                        } else
                      if ( mod->chanfreq[i] > mod->portamento_to[i] )
                        {
                          if ( (mod->chanfreq[i] - mod->portamento_to[i]) > mod->porta_speed[i] )
                            mod->chanfreq[i] -= mod->porta_speed[i];
                          else
                            mod->chanfreq[i] = mod->portamento_to[i];
                        } else
                        {
                          mod->channote[i] = mod->chanfreq[i];
                        }
                      
                      /* Do volume sliding */
                      if (mod->effectop[i]&0xf0)  /* Increase volume */
                        {
                          mod->volume[i] += (mod->effectop[i]>>4)&0x0f;
                          if (mod->volume[i]>64)
                            mod->volume[i] = 64;
                        } else
                      if (mod->effectop[i]&0x0f)  /* Decrease volume */
                        {
                          mod->volume[i] -= mod->effectop[i]&0x0f;
                          if (mod->volume[i]&0x80)  /* <0 ? */
                            mod->volume[i] = 0;
                        }
                    }
                  break;
                case 0x06:  /* Vibrato & Volume slide */
                  if (mod->speedcounter!=0)
                    {
                      mod->chanfreq[i] = (((s32)((s32)wavetab[mod->vib_wave[i]&3][mod->sintabpos[i]]*(u32)mod->vib_depth[i]))>>7) + mod->vib_basefreq[i];
                      mod->sintabpos[i] += mod->vib_freq[i];
                      mod->sintabpos[i] &= 63;

                      if (mod->effectop[i]&0xf0)  /* Increase volume */
                        {
                          mod->volume[i] += (mod->effectop[i]>>4)&0x0f;
                          if (mod->volume[i]>64)
                            mod->volume[i] = 64;
                        } else
                      if (mod->effectop[i]&0x0f)  /* Decrease volume */
                        {
                          mod->volume[i] -= mod->effectop[i]&0x0f;
                          if (mod->volume[i]&0x80)  /* <0 ? */
                            mod->volume[i] = 0;
                        }
                    }
                  break;
                case 0x07:  /* Tremolo */
                  if (mod->speedcounter!=0)
                    {
                      s16 v = mod->trem_basevol[i];
                      v += ((s32)((s32)wavetab[mod->trem_wave[i]&3][mod->sintabpos[i]]*(s32)mod->trem_depth[i]))>>6;
                      if (v>64)
                        v=64;
                      if (v<0)
                        v=0;

                      mod->sintabpos[i] += mod->trem_freq[i];
                      mod->sintabpos[i] &= 63;

                      mod->volume[i] = v;
                    }
                  break;
                case 0x0a:  /* Volume slide */
                  if (mod->speedcounter!=0)
                    {
                      if (mod->effectop[i]&0xf0)  /* Increase volume */
                        {
                          mod->volume[i] += (mod->effectop[i]>>4)&0x0f;
                          if (mod->volume[i]>64)
                            mod->volume[i] = 64;
                        } else
                      if (mod->effectop[i]&0x0f) /* Decrease volume */
                        {
                          mod->volume[i] -= mod->effectop[i]&0x0f;
                          if (mod->volume[i]&0x80)  /* <0 ? */
                            mod->volume[i] = 0;
                        }
                    }
                  break;
                case 0x0e:  /* Sub commands */
                  switch ( (mod->effectop[i]>>4)&0x0f )
                    {
                      case 0x09:  /* Retrigger sample */
                        if (mod->speedcounter!=0)
                          {
                            if (++mod->retrigger_counter[i] >= (mod->effectop[i]&0x0f))
                              {
                                mod->retrigger_counter[i] = 0;
                                mod->playpos[i] = 0;
                                mod->channel_active[i] = TRUE;
                                retval |= 1<<i;
                              }
                          }
                        break;
                      case 0x0c:  /* Cut note */
                        if (mod->speedcounter!=0)
                          {
                            if ( (mod->effectop[i]&0x0f) == mod->speedcounter )
                              mod->volume[i] = 0;
                          }
                        break;
                      case 0x0d:  /* Delay note */
                        if (mod->speedcounter!=0)
                          {
                            if (mod->speedcounter == (mod->effectop[i]&0x0f))
                              {
                                triggerNote ( mod, i, mod->nextinstr[i], mod->nextnote[i], mod->effect[i] );
                                mod->channel_active[i] = TRUE;
                                retval |= 1<<i;
                              }
                          }
                    }
                  break;
              }
          }
      }
    return retval;
  }

u32 process ( MOD * mod )
  {
    s32 i;
    u32 retval = 0;
    BOOL doPatternBreak=FALSE;
    BOOL doPatternLoop=FALSE;
    u8 * patternData = getCurPatternData ( mod, mod->patternline, 0 );

    for (i=0;i<mod->num_voices;++i)
      {
        u16 note = ((patternData[0]<<8)&0x0f00) | patternData[1];
        u8 instrument = (patternData[0]&0xf0) | ((patternData[2]>>4)&0x0f);
        u8 effect = patternData[2]&0x0f;
        u8 effect_operand = patternData[3];
        
        patternData+=4;

        if ( ((mod->last_effect[i]==0x04)||(mod->last_effect[i]==0x06)) &&
             ((effect!=0x04) && (effect!=0x06)) )
          {
            mod->chanfreq[i] = mod->vib_basefreq[i];
          }
        if ( (mod->last_effect[i]==0x07) && (effect!=0x07) )
          {
            mod->volume[i] = mod->trem_basevol[i];
          }
          
        if ( mod->effect[i]==0x00 && mod->effectop[i]!=0)
          {
            if (effect!=0 || effect_operand==0)
              mod->chanfreq[i] = mod->channote[i];
          }

        mod->nextinstr[i] = instrument;
        mod->nextnote[i] = note;

        if ( !( (effect==0x0e) && ((effect_operand&0xf0)==0xd0)) )
          {
            if (triggerNote ( mod, i, mod->nextinstr[i], mod->nextnote[i], effect ))
              {
                retval |= 1<<i;
                mod->channel_active[i] = TRUE;
              }
          }

        mod->effect[i] = 0xff;

        switch (effect)
          {
            case 0x03:
              mod->effect[i] = effect;
              mod->effectop[i] = effect_operand;
              if ( mod->effectop[i] != 0 )
                mod->porta_speed[i] = mod->effectop[i];
              break;
            case 0x04:
              if (!((mod->last_effect[i]==0x04)||(mod->last_effect[i]==0x06)))
                mod->vib_basefreq[i] = mod->chanfreq[i];

              if (effect_operand&0xf0)
                mod->vib_freq[i] = (effect_operand>>4)&0x0f;
              if (effect_operand&0x0f)
                mod->vib_depth[i] = effect_operand&0x0f;
              mod->effect[i] = effect;
              mod->effectop[i] = effect_operand;
              break;
            case 0x05:
              mod->effect[i] = effect;
              mod->effectop[i] = effect_operand;
              if ( mod->effectop[i] != 0 )
                mod->porta_speed[i] = mod->effectop[i];
              break;
            case 0x06:
              if (!((mod->last_effect[i]==0x04)||(mod->last_effect[i]==0x06)))
                mod->vib_basefreq[i] = mod->chanfreq[i];
              
              mod->effect[i] = effect;
              mod->effectop[i] = effect_operand;
              break;
            case 0x07:
              if ( mod->last_effect[i]!=0x07 )
                mod->trem_basevol[i] = mod->volume[i];
              
              if (effect_operand&0xf0)
                mod->trem_freq[i] = (effect_operand>>4)&0x0f;
              if (effect_operand&0x0f)
                mod->trem_depth[i] = effect_operand&0x0f;
              mod->effect[i] = effect;
              mod->effectop[i] = effect_operand;
              break;
            case 0x09:
              {
                u32 ofs = effect_operand<<8;
                if (ofs>mod->instrument[mod->instnum[i]].length)
                  mod->playpos[i] = mod->instrument[mod->instnum[i]].length << 16;
                else
                  mod->playpos[i] = ofs << 16;
              }
              break;
            case 0x0b:
              if (effect_operand<128)
                {
				  if(mod->notify) *mod->notify = TRUE;
                  mod->songpos = effect_operand;
                  mod->patternline = 0;
                  return retval;
                }
              break;
            case 0x0c:
              if (effect_operand>64)
                effect_operand = 64;
              mod->volume[i] = effect_operand;
              break;
            case 0x0d:
              doPatternBreak=TRUE;
              break;
            case 0x0e:
              switch ( (effect_operand>>4)&0x0f )
                {
                  case 0x01:
                    mod->chanfreq[i] -= effect_operand&0x0f;
                    if (mod->chanfreq[i]&0x8000)
                      mod->chanfreq[i] = 0;
                    break;
                  case 0x02:
                    mod->chanfreq[i] += effect_operand&0x0f;
                    if (mod->chanfreq[i]>4095)
                      mod->chanfreq[i] = 4095;
                    break;
                  case 0x03:
                    if ( (effect_operand&0x0f) == 0x00 )
                      mod->glissando[i] = FALSE;
                    else
                    if ( (effect_operand&0x0f) == 0x01 )
                      mod->glissando[i] = TRUE;
                    break;
                  case 0x04:
                    if ( (effect_operand&0x0f) < 8 )
                      mod->vib_wave[i] = effect_operand & 0x07;
                    break;
                  case 0x05:
                    mod->instrument[mod->instnum[i]].finetune = (effect_operand+8)&15;
                    break;
                  case 0x06:
                    if ((effect_operand&0x0f)==0)
                      mod->patternline_jumpto = mod->patternline;
                    else
                      {
                        doPatternLoop = TRUE;
                        if (mod->patternline_jumpcount==0)
                          {
                            mod->patternline_jumpcount = effect_operand&0x0f;
                          } else
                          {
                            if (--mod->patternline_jumpcount==0)
                              doPatternLoop = FALSE;
                          }
                      }
                    break;
                  case 0x07:
                    if ( (effect_operand&0x0f) < 8 )
                      mod->trem_wave[i] = effect_operand & 0x07;
                    break;
                  case 0x09:
                    mod->retrigger_counter[i] = 0;
                    mod->effect[i] = effect;
                    mod->effectop[i] = effect_operand;
                    break;
                  case 0x0a:
                    mod->volume[i] += effect_operand&0x0f;
                    if (mod->volume[i]>64)
                      mod->volume[i] = 64;
                    break;
                  case 0x0b:
                    mod->volume[i] -= effect_operand&0x0f;
                    if (mod->volume[i]&0x80)
                      mod->volume[i] = 0;
                  case 0x0c:
                  case 0x0d:
                    mod->effect[i] = effect;
                    mod->effectop[i] = effect_operand;
                    break;
                  case 0x0e:
                    mod->patterndelay = mod->speed*(effect_operand&0x0f);
                    break;
                }
              break;
            case 0x0f:
              if ( effect_operand<32 )
                mod->speed = effect_operand;
              else
                {
                  mod->bpm=effect_operand;
                  mod->samplespertick = mod->bpmtab[effect_operand-32];
                }

              break;
            default:
              mod->effect[i] = effect;
              mod->effectop[i] = effect_operand;
              break;
          }
        mod->last_effect[i] = effect;
      }

    if (doPatternBreak)
      {
        mod->songpos++;
        mod->patternline = 0;
        if (mod->songpos>=mod->song_length)
          {
			if(mod->notify) *mod->notify = TRUE;
            mod->songpos = 0;
            mod->patternline = 0;
          }
        mod->patternline_jumpto = 0;
        mod->patternline_jumpcount = 0;
      } else
    if (doPatternLoop)
      {
        mod->patternline = mod->patternline_jumpto;
      } else
      {
        mod->patternline++;
        if (mod->patternline>63)
          {
            mod->patternline=0;
            mod->songpos++;
            mod->patternline_jumpto = 0;
            mod->patternline_jumpcount = 0;
          }
        if (mod->songpos>=mod->song_length)
          {
			if(mod->notify) *mod->notify = TRUE;
            mod->songpos = 0;
            mod->patternline = 0;
          }
      }

    return retval;
  }

void MOD_Start ( MOD * mod )
  {
    s32 i;

    for (i=MAX_VOICES-1;i>=0;--i)
      {
        mod->volume[i] = 0;
        mod->playpos[i] = 0;
        mod->chanfreq[i] = 0;
        mod->instnum[i] = 0;
        mod->channote[i] = 0;
        mod->sintabpos[i] = 0;
        mod->vib_freq[i] = 0;
        mod->vib_depth[i] = 0;
        mod->last_effect[i] = 0;
        mod->glissando[i] = FALSE;
        mod->trem_wave[i] = 0;
        mod->vib_wave[i] = 0;
        mod->channel_active[i] = FALSE;
      }

    mod->songpos = 0;
    mod->patternline = 0;
    mod->speed = 6;
    mod->bpm = 125;
    mod->speedcounter = 0;
    
    mod->patterndelay = 0;
    mod->patternline_jumpto = 0;
    mod->patternline_jumpcount = 0;
#ifndef GEKKO
	switch(mod->freq) {
		case 32000:
			mod->bpmtab = bpmtab32KHz;
			mod->inctab = inctab32KHz;
			break;
		case 48000:
			mod->bpmtab = bpmtab48KHz;
			mod->inctab = inctab48KHz;
			break;
		default:
			mod->bpmtab = bpmtab48KHz;
			mod->inctab = inctab48KHz;
			break;
	}
#else
	if(mod->freq==32000 || mod->freq==48000) {
		mod->inctab = modplay_getinctab(mod->freq);
		mod->bpmtab = modplay_getbpmtab(mod->freq);
	}
#endif

    mod->samplescounter = 0;
    mod->samplespertick = mod->bpmtab[125-32];
  }

u32 MOD_Player ( MOD * mod )
  {
    s16 * buf = (s16*)mod->mixingbuf;
    s32 len = mod->mixingbuflen;
    u32 retval = 0;

    if (mod->musicvolume>0x40)
      mod->musicvolume = 0x40;
    if (mod->sfxvolume>0x40)
      mod->sfxvolume = 0x40;

    if (mod->bits == 16)
      {
        len >>= 1;
        if (mod->channels==1)
          {
            s32 l = 0;
            s32 remain = len;
            do
              {
                s32 tick_remain = mod->samplespertick - mod->samplescounter;
                s32 res;

                res = mix_mono_16bit ( mod, &buf[l], tick_remain<=remain ? tick_remain : remain );
                l += res;
                remain -= res;

                mod->samplescounter += res;
                if ( mod->samplescounter >= mod->samplespertick )
                  {
                    mod->samplescounter -= mod->samplespertick;
                    mod->speedcounter++;
                    if (mod->speedcounter>=(mod->speed+mod->patterndelay))
                      {
                        mod->patterndelay=0;
                        retval |= process(mod);
                        mod->speedcounter = 0;
                      }
                    retval |= effect_handler(mod);
                  }
              } while ( remain>0 );
          } else
        if (mod->channels==2)
          {
            s32 l = 0;
            s32 remain;
            len >>= 1;
            remain = len;
            do
              {
                s32 tick_remain = mod->samplespertick - mod->samplescounter;
                s32 res;

                res = mix_stereo_16bit ( mod, &buf[l<<1], tick_remain<=remain ? tick_remain : remain );
                l += res;
                remain -= res;

                mod->samplescounter += res;
                if ( mod->samplescounter >= mod->samplespertick )
                  {
                    mod->samplescounter -= mod->samplespertick;
                    mod->speedcounter++;
                    if (mod->speedcounter>=(mod->speed+mod->patterndelay))
                      {
                        mod->patterndelay=0;
                        retval |= process(mod);
                        mod->speedcounter = 0;
                      }
                    retval |= effect_handler(mod);
                  }
              } while ( remain>0 );
          }
      }

    mod->notebeats = retval;
    if (mod->callback!=NULL)
      mod->callback ( mod );

    return retval;
  }

s32 MOD_TriggerNote ( MOD * mod, s32 channel, u8 instnum, u16 freq, u8 vol )
  {
    if (mod==NULL)
      return -1;
     
    channel += mod->num_voices;
    if (channel>=mod->num_channels)
      return -1;
    if (mod->instrument[instnum].data==NULL && instnum!=0xff)
      return -1;

    if (instnum!=0xff)
      {
        mod->channel_active[channel] = TRUE;
        mod->playpos[channel] = 0;
        mod->instnum[channel] = instnum;
      }
    if (freq!=0xffff)
      mod->chanfreq[channel] = freq;
    if (vol<=0x40)
      mod->volume[channel] = vol;

    return 0;
  }

