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

#include <stdio.h>
#include <unistd.h>

#include <ogcsys.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#include "asndlib.h"
#include "asnd_dsp_mixer_bin.h"

#undef SND_BUFFERSIZE

#define MAX_SND_VOICES			16
#define SND_BUFFERSIZE			(4096) // don't modify this value

#define VOICE_UPDATEADD   (1<<12)
#define VOICE_UPDATE      (1<<11)
#define VOICE_VOLUPDATE   (1<<10)
#define VOICE_PAUSE       (1<<9)
#define VOICE_SETLOOP     (1<<8)

typedef struct
{
	void *out_buf;	// output buffer 4096 bytes aligned to 32

	u32 delay_samples; // samples per delay to start (48000 == 1sec)

	u32 flags;	// (step<<16) | (statuses<<8) | (type & 7) used in DSP side

	u32 start_addr; // internal addr counter
	u32 end_addr;   // end voice physical pointer(bytes without alignament, but remember it reads in blocks of 32 bytes (use padding to the end))

	u32 freq;	// freq operation

	s16 left, right; // internally used to store de last sample played

	u32 counter;	// internally used to convert freq to 48000Hz samples

	u16 volume_l,volume_r; // volume (from 0 to 256)

	u32 start_addr2;	// initial voice2 physical pointer (bytes aligned  32 bytes) (to do a ring)
	u32 end_addr2;		// end voice2 physical pointer(bytes without alignament, but remember it reads in blocks of 32 bytes (use padding to the end))

	u16 volume2_l,volume2_r; // volume (from 0 to 256) for voice 2

	u32 backup_addr; // initial voice physical pointer backup (bytes aligned to 32 bytes): It is used for test pointers purpose

	u32 tick_counter; // voice tick counter

	ASNDVoiceCallback cb;

	u32 _pad;
} t_sound_data;

static dsptask_t dsp_task;

static vu64 time_of_process;
static vu32 dsp_complete = 1;
static vu64 dsp_task_starttime = 0;
static vu32 curr_audio_buf = 0;
static vu32 dsp_done = 0;

static vs32 snd_chan = 0;
static vs32 global_pause = 1;
static vu32 global_counter = 0;

static vu32 DSP_DI_HANDLER = 1;
static void (*global_callback)(void) = NULL;

static u32 asnd_inited = 0;
static t_sound_data sound_data[MAX_SND_VOICES];

static t_sound_data sound_data_dma ATTRIBUTE_ALIGN(32);
static s16 mute_buf[SND_BUFFERSIZE] ATTRIBUTE_ALIGN(32);
static s16 audio_buf[2][SND_BUFFERSIZE] ATTRIBUTE_ALIGN(32);

extern u32 gettick();

static __inline__ char* snd_set0b( char *p, int n)
{
	while(n>0) {*p++=0;n--;}
	return p;
}

static __inline__ s32* snd_set0w( s32 *p, int n)
{
	while(n>0) {*p++=0;n--;}
	return p;
}

static void __dsp_initcallback(dsptask_t *task)
{
	DSP_SendMailTo(0x0123); // command to fix the data operation
	while(DSP_CheckMailTo());

	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL((&sound_data_dma))); //send the data operation mem
	while(DSP_CheckMailTo());

	dsp_complete=1;
	DSP_DI_HANDLER=0;
}

static void __dsp_requestcallback(dsptask_t *task)
{
	s32 n;

	if(DSP_DI_HANDLER) return;

	DCInvalidateRange(&sound_data_dma, sizeof(t_sound_data));

	if(snd_chan>=MAX_SND_VOICES) {
		if(!dsp_complete) time_of_process = (gettime() - dsp_task_starttime);
		if(!global_pause) global_counter++;

		dsp_complete = 1;
		return;
	}

	sound_data_dma.freq=sound_data[snd_chan].freq;
	sound_data_dma.cb=sound_data[snd_chan].cb;
	if(sound_data[snd_chan].flags & VOICE_UPDATE) // new song
	{
		sound_data[snd_chan].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_PAUSE | VOICE_UPDATEADD);
		//sound_data[snd_chan].out_buf= (void *) MEM_VIRTUAL_TO_PHYSICAL((void *) audio_buf[curr_audio_buf]);
		sound_data_dma=sound_data[snd_chan];
	}
	else
	{

		if(sound_data[snd_chan].flags & VOICE_VOLUPDATE)
		{
			sound_data[snd_chan].flags &=~VOICE_VOLUPDATE;
			sound_data_dma.volume_l=sound_data_dma.volume2_l=sound_data[snd_chan].volume2_l;
			sound_data_dma.volume_r=sound_data_dma.volume2_r=sound_data[snd_chan].volume2_r;
		}


		//if(mail==0xbebe0003) sound_data_dma.flags|=VOICE_SETCALLBACK;

		if(sound_data_dma.start_addr>=sound_data_dma.end_addr || !sound_data_dma.start_addr)
		{
			sound_data_dma.backup_addr=sound_data_dma.start_addr=sound_data_dma.start_addr2;
			sound_data_dma.end_addr=sound_data_dma.end_addr2;
			if(!(sound_data[snd_chan].flags & VOICE_SETLOOP)) {sound_data_dma.start_addr2=0;sound_data_dma.end_addr2=0;}
			sound_data_dma.volume_l=sound_data_dma.volume2_l;
			sound_data_dma.volume_r=sound_data_dma.volume2_r;
		}

		if(sound_data[snd_chan].start_addr2 && (sound_data[snd_chan].flags & VOICE_UPDATEADD))
		{
			sound_data[snd_chan].flags &=~VOICE_UPDATEADD;
			if(!sound_data[snd_chan].start_addr || !sound_data_dma.start_addr)
			{
				sound_data_dma.backup_addr=sound_data_dma.start_addr=sound_data[snd_chan].start_addr2;
				sound_data_dma.end_addr=sound_data[snd_chan].end_addr2;
				sound_data_dma.start_addr2=sound_data[snd_chan].start_addr2;
				sound_data_dma.end_addr2=sound_data[snd_chan].end_addr2;
				if(!(sound_data[snd_chan].flags & VOICE_SETLOOP)) {sound_data_dma.start_addr2=0;sound_data_dma.end_addr2=0;}
				sound_data_dma.volume_l=sound_data[snd_chan].volume2_l;
				sound_data_dma.volume_r=sound_data[snd_chan].volume2_r;
			}
			else
			{
				sound_data_dma.start_addr2=sound_data[snd_chan].start_addr2;
				sound_data_dma.end_addr2=sound_data[snd_chan].end_addr2;
				sound_data_dma.volume2_l=sound_data[snd_chan].volume2_l;
				sound_data_dma.volume2_r=sound_data[snd_chan].volume2_r;
			}

		}

		if(!sound_data[snd_chan].cb && (!sound_data_dma.start_addr && !sound_data_dma.start_addr2)) sound_data[snd_chan].flags=0;
		sound_data_dma.flags=sound_data[snd_chan].flags & ~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_UPDATEADD);
		sound_data[snd_chan]=sound_data_dma;
	}

	if(sound_data[snd_chan].flags>>16)
	{
		if(!sound_data[snd_chan].delay_samples && !(sound_data[snd_chan].flags & VOICE_PAUSE) && (sound_data_dma.start_addr || sound_data_dma.start_addr2)) sound_data[snd_chan].tick_counter++;
	}

	snd_chan++;

	if(!sound_data[snd_chan].cb && (!sound_data[snd_chan].start_addr && !sound_data[snd_chan].start_addr2)) sound_data[snd_chan].flags=0;

	while(snd_chan<16 && !(sound_data[snd_chan].flags>>16)) snd_chan++;

	if(snd_chan>=MAX_SND_VOICES)
	{
		snd_chan++;
		DCFlushRange(&sound_data_dma, sizeof(t_sound_data));
		DSP_SendMailTo(0x666);
		while(DSP_CheckMailTo());
		return;
	}

	sound_data_dma=sound_data[snd_chan];

	DCFlushRange(&sound_data_dma, sizeof(t_sound_data));
	DSP_SendMailTo(0x222); // send the voice and mix the samples of the buffer
	while(DSP_CheckMailTo());

	// callback strategy for next channel
	n=snd_chan+1;

	while(n<16 && !(sound_data[n].flags>>16)) n++;

	if(n<16)
	{
		if(!sound_data[n].start_addr2 && (sound_data[n].flags>>16) && sound_data[n].cb) sound_data[n].cb(n);

		if(sound_data[snd_chan].flags & VOICE_VOLUPDATE)
		{
			sound_data[snd_chan].flags &=~VOICE_VOLUPDATE;
		}

		if(sound_data[n].flags & VOICE_UPDATE) // new song
		{
			sound_data[n].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_PAUSE | VOICE_UPDATEADD);
		}

		if(!sound_data[n].cb && (!sound_data[n].start_addr && !sound_data[n].start_addr2)) sound_data[n].flags=0;
	}
}

static void __dsp_donecallback(dsptask_t *task)
{
	dsp_done = 1;
}

static void audio_dma_callback(void)
{
	u32 n;

	curr_audio_buf ^= 1;

	if(DSP_DI_HANDLER || global_pause)
		AUDIO_InitDMA((u32)mute_buf,SND_BUFFERSIZE);
	else
		AUDIO_InitDMA((u32)audio_buf[curr_audio_buf],SND_BUFFERSIZE);

	if(DSP_DI_HANDLER || global_pause) return;
	if(dsp_complete==0) return;

	dsp_complete = 0;

	for(n=0;n<MAX_SND_VOICES;n++) sound_data[n].out_buf = (void *)MEM_VIRTUAL_TO_PHYSICAL((void *)audio_buf[curr_audio_buf]);

	if(global_callback) global_callback();

	snd_chan = 0;
	if(!sound_data[snd_chan].start_addr2 && (sound_data[snd_chan].flags>>16) && sound_data[snd_chan].cb) sound_data[snd_chan].cb(snd_chan);

	if(sound_data[snd_chan].flags & VOICE_VOLUPDATE)
	{
		sound_data[snd_chan].flags &=~VOICE_VOLUPDATE;
	}

	if(sound_data[snd_chan].flags & VOICE_UPDATE) // new song
	{
		sound_data[snd_chan].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_PAUSE | VOICE_UPDATEADD);
	}
	else
	{

		if(sound_data[snd_chan].start_addr>=sound_data[snd_chan].end_addr)
		{
			sound_data[snd_chan].backup_addr=sound_data[snd_chan].start_addr=sound_data[snd_chan].start_addr2;sound_data[snd_chan].start_addr2=0;
			sound_data[snd_chan].end_addr=sound_data[snd_chan].end_addr2;sound_data[snd_chan].end_addr2=0;
			sound_data[snd_chan].volume_l=sound_data[snd_chan].volume2_l;
			sound_data[snd_chan].volume_r=sound_data[snd_chan].volume2_r;
		}

		if(sound_data[snd_chan].start_addr2 && (sound_data[snd_chan].flags & VOICE_UPDATEADD))
		{
			sound_data[snd_chan].flags &=~VOICE_UPDATEADD;

			if(!sound_data[snd_chan].start_addr)
			{
				sound_data[snd_chan].backup_addr=sound_data[snd_chan].start_addr=sound_data[snd_chan].start_addr2;
				sound_data[snd_chan].end_addr=sound_data[snd_chan].end_addr2;
				if(!(sound_data[snd_chan].flags & VOICE_SETLOOP)) {sound_data[snd_chan].start_addr2=0;sound_data[snd_chan].end_addr2=0;}
				sound_data[snd_chan].volume_l=sound_data[snd_chan].volume2_l;
				sound_data[snd_chan].volume_r=sound_data[snd_chan].volume2_r;
			}

		}

	}
	if(!sound_data[snd_chan].cb && (!sound_data[snd_chan].start_addr && !sound_data[snd_chan].start_addr2)) sound_data[snd_chan].flags=0;

	sound_data_dma=sound_data[snd_chan];
	DCFlushRange(&sound_data_dma, sizeof(t_sound_data));

	dsp_task_starttime = gettime();
	DSP_SendMailTo(0x111); // send the first voice and clear the buffer
	while(DSP_CheckMailTo());

	// callback strategy for next channel
	n=snd_chan+1;

	while(n<16 && !(sound_data[n].flags>>16)) n++;

	if(n<16)
	{
		if(!sound_data[n].start_addr2 && (sound_data[n].flags>>16) && sound_data[n].cb) sound_data[n].cb(n);

		if(sound_data[n].flags & (VOICE_VOLUPDATE | VOICE_UPDATEADD))
		{
			sound_data[n].flags &=~(VOICE_VOLUPDATE | VOICE_UPDATEADD);
		}

		if(sound_data[n].flags & VOICE_UPDATE) // new song
		{
			sound_data[n].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE| VOICE_PAUSE | VOICE_UPDATEADD);
		}

		if(!sound_data[n].cb && (!sound_data[n].start_addr && !sound_data[n].start_addr2)) sound_data[n].flags=0;
	}
}

void ASND_Init(void)
{
	u32 i,level;

	DSP_Init();
	AUDIO_Init(NULL);
	AUDIO_StopDMA(); // in case audio was previously inited and a DMA callback set
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);

	_CPU_ISR_Disable(level);
	if(!asnd_inited) {
		asnd_inited = 1;
		global_pause = 1;
		curr_audio_buf = 0;
		DSP_DI_HANDLER = 1;
		dsp_complete = 0;
		dsp_done = 0;

		snd_set0w((s32*)mute_buf, SND_BUFFERSIZE>>2);
		snd_set0w((s32*)audio_buf[0], SND_BUFFERSIZE>>2);
		snd_set0w((s32*)audio_buf[1], SND_BUFFERSIZE>>2);
		DCFlushRange(mute_buf,SND_BUFFERSIZE);
		DCFlushRange(audio_buf[0],SND_BUFFERSIZE);
		DCFlushRange(audio_buf[1],SND_BUFFERSIZE);

		for(i=0;i<MAX_SND_VOICES;i++)
			snd_set0w((s32*)&sound_data[i],sizeof(t_sound_data)/4);

		dsp_task.prio = 255;
		dsp_task.iram_maddr = (u16*)MEM_VIRTUAL_TO_PHYSICAL(asnd_dsp_mixer_bin);
		dsp_task.iram_len = asnd_dsp_mixer_bin_size;
		dsp_task.iram_addr = 0x0000;
		dsp_task.init_vec = 0x10;
		dsp_task.res_cb = NULL;
		dsp_task.req_cb = __dsp_requestcallback;
		dsp_task.init_cb =__dsp_initcallback;
		dsp_task.done_cb =__dsp_donecallback;
		DSP_AddTask(&dsp_task);
	}
	AUDIO_RegisterDMACallback(audio_dma_callback);
	AUDIO_InitDMA((u32)audio_buf[curr_audio_buf],SND_BUFFERSIZE);
	AUDIO_StartDMA();

	_CPU_ISR_Restore(level);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_End(void)
{
   if(asnd_inited) {
       AUDIO_StopDMA();
       DSP_DI_HANDLER=1;
       usleep(100);
       AUDIO_RegisterDMACallback(NULL);
       DSP_DI_HANDLER=1;
	   dsp_done = 0;
	   DSP_SendMailTo(0x999);
	   while(DSP_CheckMailTo());
	   while(!dsp_done);
	   asnd_inited=0;
   }
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_SetVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r, ASNDVoiceCallback callback)
{
	u32 level;
	u32 flag_h=0;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(size_snd<=0 || snd==NULL) return SND_INVALID; // invalid voice

	DCFlushRange(snd, size_snd);

	if(pitch<1) pitch=1;
	if(pitch>MAX_PITCH) pitch=MAX_PITCH;

	volume_l &=255;
	volume_r &=255;

	delay=(u32) (48000LL*((u64) delay)/1000LL);

	format&=7;

	switch(format&3)
	{
	case 0:
		flag_h=1<<16;break;
	case 1:
	case 2:
		flag_h=2<<16;break;
	case 3:
		flag_h=4<<16;break;
	}

	format|= flag_h | VOICE_UPDATE;

	_CPU_ISR_Disable(level);

	sound_data[voice].left=0;
	sound_data[voice].right=0;
	sound_data[voice].counter=0;

	sound_data[voice].freq=pitch;

	sound_data[voice].delay_samples=delay;

	sound_data[voice].volume_l= volume_l;
	sound_data[voice].volume_r= volume_r;

	sound_data[voice].backup_addr=sound_data[voice].start_addr=MEM_VIRTUAL_TO_PHYSICAL(snd);
	sound_data[voice].end_addr=MEM_VIRTUAL_TO_PHYSICAL(snd)+(size_snd);

	sound_data[voice].start_addr2=0;
	sound_data[voice].end_addr2=0;

	sound_data[voice].volume2_l= volume_l;
	sound_data[voice].volume2_r= volume_r;

	sound_data[voice].flags=format;
	sound_data[voice].tick_counter=0;

	sound_data[voice].cb = callback;
	_CPU_ISR_Restore(level);

	return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_SetInfiniteVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r)
{
	u32 level;
	u32 flag_h=0;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(size_snd<=0 || snd==NULL) return SND_INVALID; // invalid voice

	DCFlushRange(snd, size_snd);

	if(pitch<1) pitch=1;
	if(pitch>MAX_PITCH) pitch=MAX_PITCH;

	volume_l &=255;
	volume_r &=255;

	delay=(u32) (48000LL*((u64) delay)/1000LL);

	format&=7;

	switch(format&3)
	{
	case 0:
		flag_h=1<<16;break;
	case 1:
	case 2:
		flag_h=2<<16;break;
	case 3:
		flag_h=4<<16;break;
	}

	format|= flag_h | VOICE_UPDATE | VOICE_SETLOOP;

	_CPU_ISR_Disable(level);

	sound_data[voice].left=0;
	sound_data[voice].right=0;
	sound_data[voice].counter=0;

	sound_data[voice].freq=pitch;

	sound_data[voice].delay_samples=delay;

	sound_data[voice].volume_l= volume_l;
	sound_data[voice].volume_r= volume_r;

	sound_data[voice].backup_addr=sound_data[voice].start_addr=MEM_VIRTUAL_TO_PHYSICAL(snd);
	sound_data[voice].end_addr=MEM_VIRTUAL_TO_PHYSICAL(snd)+(size_snd);

	sound_data[voice].start_addr2=sound_data[voice].start_addr;
	sound_data[voice].end_addr2=sound_data[voice].end_addr;

	sound_data[voice].volume2_l= volume_l;
	sound_data[voice].volume2_r= volume_r;

	sound_data[voice].flags=format;
	sound_data[voice].tick_counter=0;

	sound_data[voice].cb=NULL;
	_CPU_ISR_Restore(level);

	return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_AddVoice(s32 voice, void *snd, s32 size_snd)
{
	u32 level;
	s32 ret=SND_OK;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(size_snd<=0 || snd==NULL) return SND_INVALID; // invalid voice

	if((sound_data[voice].flags & (VOICE_UPDATE | VOICE_UPDATEADD)) || !(sound_data[voice].flags>>16)) return SND_INVALID; // busy or unused voice

	DCFlushRange(snd, size_snd);
	_CPU_ISR_Disable(level);

	if(sound_data[voice].start_addr2==0)
	{

		sound_data[voice].start_addr2=MEM_VIRTUAL_TO_PHYSICAL(snd);
		sound_data[voice].end_addr2=MEM_VIRTUAL_TO_PHYSICAL(snd)+(size_snd);

		sound_data[voice].flags&=~VOICE_SETLOOP;
		sound_data[voice].flags|=VOICE_UPDATEADD;
	} else ret=SND_BUSY;

	_CPU_ISR_Restore(level);

	return ret;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_TestVoiceBufferReady(s32 voice)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return 0; // invalid voice: not ready (of course XD)
	if(sound_data[voice].start_addr && sound_data[voice].start_addr2) return 0; // not ready

	return 1; // ready
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_TestPointer(s32 voice, void *pointer)
{
	u32 level;
	u32 addr2=(u32) MEM_VIRTUAL_TO_PHYSICAL(pointer);
	int ret=SND_OK;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	_CPU_ISR_Disable(level);

	if(sound_data[voice].backup_addr==addr2 /*&& sound_data[voice].end_addr>(addr2)*/) ret=SND_BUSY;
	else
		if(sound_data[voice].start_addr2==addr2 /*&& sound_data[voice].end_addr2>(addr2)*/) ret=SND_BUSY;

	_CPU_ISR_Restore(level);

	return ret;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_PauseVoice(s32 voice, s32 pause)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice
	if(pause) sound_data[voice].flags|=VOICE_PAUSE; else sound_data[voice].flags&=~VOICE_PAUSE;

	return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_StopVoice(s32 voice)
{
	u32 level;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	_CPU_ISR_Disable(level);

	sound_data[voice].backup_addr=sound_data[voice].start_addr=sound_data[voice].start_addr2=0;
	sound_data[voice].end_addr=sound_data[voice].end_addr2=0;
	sound_data[voice].flags=0;

	_CPU_ISR_Restore(level);

	return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_StatusVoice(s32 voice)
{
	u32 level;
	s32 status=SND_WORKING;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	_CPU_ISR_Disable(level);
	if(!(sound_data[voice].flags>>16)) status=SND_UNUSED;
	if(sound_data[voice].flags & VOICE_PAUSE) status=SND_WAITING;
	_CPU_ISR_Restore(level);

	return status;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_ChangeVolumeVoice(s32 voice, s32 volume_l, s32 volume_r)
{
	u32 level;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	volume_l &=255;
	volume_r &=255;

	_CPU_ISR_Disable(level);
	sound_data[voice].flags |=VOICE_VOLUPDATE;
	sound_data[voice].volume_l= sound_data[voice].volume2_l= volume_l;
	sound_data[voice].volume_r= sound_data[voice].volume2_r= volume_r;
	_CPU_ISR_Restore(level);

	return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetTickCounterVoice(s32 voice)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return 0; // invalid voice

	return (sound_data[voice].tick_counter * SND_BUFFERSIZE/4);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetTimerVoice(s32 voice)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return 0; // invalid voice

	return (sound_data[voice].tick_counter * SND_BUFFERSIZE/4)/48;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_Pause(s32 pause)
{
	global_pause=pause;

}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_Is_Paused(void)
{
	return global_pause;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetTime(void)
{
	return (global_counter * SND_BUFFERSIZE/4)/48;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetSampleCounter(void)
{
	return (global_counter * SND_BUFFERSIZE/4);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetSamplesPerTick(void)
{
	return (SND_BUFFERSIZE/4);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_SetTime(u32 time)
{
	global_counter=48*time;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_SetCallback(void (*callback)(void))
{
	global_callback=callback;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_GetAudioRate(void)
{
	return 48000;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_GetFirstUnusedVoice(void)
{

	s32 n;

	for(n=1;n<MAX_SND_VOICES;n++)
		if(!(sound_data[n].flags>>16)) return n;

	if(!(sound_data[0].flags>>16)) return 0; // voice 0 is a special case

	return SND_INVALID;  // all voices used

}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_ChangePitchVoice(s32 voice, s32 pitch)
{
	u32 level;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(pitch<1) pitch=1;
	if(pitch>144000) pitch=144000;

	_CPU_ISR_Disable(level);
	sound_data[voice].freq= pitch;
	_CPU_ISR_Restore(level);

	return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetDSP_PercentUse(void)
{
	return (time_of_process)*100/21333; // time_of_process = nanoseconds , 1024 samples= 21333 nanoseconds
}

u32 ASND_GetDSP_ProcessTime(void)
{
	u32 level,ret;

	_CPU_ISR_Disable(level);
	ret = time_of_process;
	_CPU_ISR_Restore(level);

	return ticks_to_nanosecs(ret);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int ANote2Freq(int note, int freq_base,int note_base)
{
	int n;
	static int one=1;
	static u32 tab_piano_frac[12];

	if(one)
	{
		float note=1.0f;
		one=0;
		for(n=0;n<12;n++) // table note
		{
			tab_piano_frac[n]=(u32)(10000.0f*note);
			note*=1.0594386f;
		}
	}

	// obtiene octava 3 (notas 36 a 47)

	n=(note/12)-(note_base/12);
	if(n>=0) freq_base<<=n;
	else freq_base>>= -n;


	if(freq_base<=0x1ffff) // Math precision
		n=(s32) (((u32)freq_base)*tab_piano_frac[(note % 12)]/tab_piano_frac[(note_base % 12)]);
	else
		n=(s32) (((u64)freq_base)*((u64) tab_piano_frac[(note % 12)])/((u64) tab_piano_frac[(note_base % 12)]));


	return n;
}

// END

