// Modified by Francisco Mu�oz 'Hermes' MAY 2008

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gccore.h>
#include <asndlib.h>
#include "gcmodplay.h"

//#define _GCMOD_DEBUG

#define STACKSIZE		8192
#define SNDBUFFERSIZE	(3840<<2)			//that's the maximum buffer size for one VBlank we need.

static BOOL thr_running = FALSE;
static BOOL sndPlaying = FALSE;
static MODSNDBUF sndBuffer;

static s32 have_samples = 0;
static s32 mod_freq = 48000;

static u32 shiftVal = 0;
static vu32 curr_audio = 0;
static u32 curr_datalen[2] = {0,0};
static u8 audioBuf[2][SNDBUFFERSIZE] ATTRIBUTE_ALIGN(32);

static lwpq_t player_queue;
static lwp_t hplayer;
static u8 player_stack[STACKSIZE];
static void* player(void *);

#ifdef _GCMOD_DEBUG
extern long long gettime();
extern u32 diff_usec(unsigned long long start,unsigned long long end);
extern u32 diff_msec(unsigned long long start,unsigned long long end);
#endif

static void* player(void *arg)
{
#ifdef _GCMOD_DEBUG
	long long start,end;
#endif

	thr_running = TRUE;
	while(sndPlaying==TRUE) {
		LWP_ThreadSleep(player_queue);

		if(curr_datalen[curr_audio]>0 && sndPlaying==TRUE) {
#ifdef _GCMOD_DEBUG
			printf("player(run callback)\n\n");
			start = gettime();
#endif
			sndBuffer.callback(sndBuffer.usr_data,((u8*)audioBuf[curr_audio]),curr_datalen[curr_audio]);
			have_samples = 2;
#ifdef _GCMOD_DEBUG
			end = gettime();
			printf("player(end callback,%d - %d us)\n\n",curr_audio,diff_usec(start,end));
#endif
		}
	}
	thr_running = FALSE;
#ifdef _GCMOD_DEBUG
	printf("player stopped %d\n",thr_running);
#endif
	return 0;
}

static void dmaCallback()
{	
#ifndef __SNDLIB_H__
	MODPlay *mp = (MODPlay*)sndBuffer.usr_data;
	MOD *mod = &mp->mod;
#endif

#ifdef _GCMOD_DEBUG
	static long long start = 0,end = 0;

	end = gettime();
	if(start) printf("dmaCallback(%p,%d,%d - after %d ms)\n",(void*)audioBuf[curr_audio],curr_datalen,curr_audio,diff_msec(start,end));
#endif

#ifndef __SNDLIB_H__
	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)audioBuf[curr_audio],curr_datalen[curr_audio]);
	AUDIO_StartDMA();

	curr_audio ^= 1;
	curr_datalen[curr_audio] = (mod->samplespertick<<shiftVal);
	LWP_ThreadSignal(player_queue);
#else
	if(have_samples==0) {
		have_samples = 1;
		LWP_ThreadSignal(player_queue);
		return;
	}
	if(have_samples==1) return;
	if(have_samples==2) {
		if(SND_AddVoice(0,audioBuf[curr_audio], curr_datalen[curr_audio])!=0) return; // Sorry I am busy: try again

		curr_datalen[curr_audio]=0;
		have_samples=0;
		curr_audio ^= 1;
		curr_datalen[curr_audio]=SNDBUFFERSIZE;
	}
#endif

#ifdef _GCMOD_DEBUG
	start = gettime();
	printf("dmaCallback(%p,%d,%d,%d us) leave\n",(void*)audioBuf[curr_audio],curr_datalen,curr_audio,diff_usec(end,start));
#endif
}

static void mixCallback(void *usrdata,u8 *stream,u32 len)
{
	u32 i;
	MODPlay *mp = (MODPlay*)usrdata;
	MOD *mod = &mp->mod;
#ifdef _GCMOD_DEBUG
	printf("mixCallback(%p,%p,%d) enter\n",stream,usrdata,len);
#endif
	if(mp->manual_polling)
		mod->notify = &mp->paused;
	else
		mod->notify = NULL;

	mod->mixingbuf = stream;
	mod->mixingbuflen = len;

	if(mp->paused) {
		for(i=0;i<(len>>1);i++)
			((u16*)stream)[i] = 0;
	} else
		MOD_Player(mod);

	DCFlushRange(stream,len);
#ifdef _GCMOD_DEBUG
	printf("mixCallback(%p,%p,%d,%d) leave\n",stream,usrdata,len,mp->paused);
#endif
}

static s32 SndBufStart(MODSNDBUF *sndbuf)
{
	if(sndPlaying) return -1;
#ifdef _GCMOD_DEBUG
	printf("SndBufStart(%p) enter\n",sndbuf);
#endif
	memcpy(&sndBuffer,sndbuf,sizeof(MODSNDBUF));
	
	shiftVal = 0;
	if(sndBuffer.chans==2)
		shiftVal++;
	if(sndBuffer.fmt==16)
		shiftVal++;

	memset(audioBuf[0],0,SNDBUFFERSIZE);
	memset(audioBuf[1],0,SNDBUFFERSIZE);

	DCFlushRange(audioBuf[0],SNDBUFFERSIZE);
	DCFlushRange(audioBuf[1],SNDBUFFERSIZE);

	while(thr_running);

	curr_audio = 0;
	sndPlaying = TRUE;
	curr_datalen[0] = SNDBUFFERSIZE;
	curr_datalen[1] = SNDBUFFERSIZE;
	if(LWP_CreateThread(&hplayer,player,NULL,player_stack,STACKSIZE,80)!=-1) {
#ifndef __SNDLIB_H__
		AUDIO_RegisterDMACallback(dmaCallback);
		AUDIO_InitDMA((u32)audioBuf[curr_audio],curr_datalen[curr_audio]);
		AUDIO_StartDMA();
		curr_audio ^= 1;
#else
		SND_SetVoice(0, VOICE_STEREO_16BIT, mod_freq,0, audioBuf[curr_audio], curr_datalen[curr_audio], 255, 255, dmaCallback);
		have_samples=0;

		curr_audio ^= 1;
		SND_Pause(0);
#endif
		return 1;
	}
	sndPlaying = FALSE;

	return -1;
}

static void SndBufStop()
{
	if(!sndPlaying) return;
#ifndef __SNDLIB_H__
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
#else
	SND_StopVoice(0);
#endif
	curr_audio = 0;
	sndPlaying = FALSE;
	curr_datalen[0] = 0;
	curr_datalen[1] = 0;
	LWP_ThreadSignal(player_queue);
	LWP_JoinThread(hplayer,NULL);
}

static s32 updateWaveFormat(MODPlay *mod)
{
	BOOL p = mod->playing;
	
	if(p)
		SndBufStop();
	
	if(mod->stereo) {
		mod->soundBuf.chans = 2;
		mod->mod.channels = 2;
	} else {
		mod->soundBuf.chans = 1;
		mod->mod.channels = 1;
	}

	mod->soundBuf.freq = mod->playfreq;
	mod->mod.freq = mod->playfreq;
	mod->mod.bits = 16;
	
	if(p) {
		mod->mod.samplescounter = 0;
		mod->mod.samplespertick = mod->mod.bpmtab[mod->mod.bpm-32];
	}
	
	mod->soundBuf.fmt = 16;
	mod->soundBuf.usr_data = mod;
	mod->soundBuf.callback = mixCallback;
	mod->soundBuf.samples = (f32)mod->playfreq/50.0F;
	
	if(p)
		SndBufStart(&mod->soundBuf);
	
	return 0;	
}

void MODPlay_Init(MODPlay *mod)
{
	memset(mod,0,sizeof(MODPlay));

#ifndef __SNDLIB_H__
    AUDIO_Init(NULL);
#else
    SND_Pause(0);
    SND_StopVoice(0);
#endif
	MODPlay_SetFrequency(mod,48000);
	MODPlay_SetStereo(mod,TRUE);

	LWP_InitQueue(&player_queue);

	sndPlaying = FALSE;
	thr_running = FALSE;

	mod->paused = FALSE;
	mod->bits = TRUE;
	mod->numSFXChans = 0;
	mod->manual_polling = FALSE;
}

s32 MODPlay_SetFrequency(MODPlay *mod,u32 freq)
{
	if(freq==mod->playfreq) return 0;
	if(freq==32000 || freq==48000) {
#ifndef __SNDLIB_H__
		if(freq==32000)
			AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
		else
			AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
#else
		mod_freq = 48000;
#endif
		mod->playfreq = freq;
		updateWaveFormat(mod);
		return 0;
	}
	return -1;
}

void MODPlay_SetStereo(MODPlay *mod,BOOL stereo)
{
	if(stereo==mod->stereo) return;

	mod->stereo = stereo;
	updateWaveFormat(mod);
}

void MODPlay_Unload(MODPlay *mod)
{
	MODPlay_Stop(mod);
	MOD_Free(&mod->mod);
}

s32 MODPlay_SetMOD(MODPlay *mod,const void *mem)
{
	MODPlay_Unload(mod);

	if(MOD_SetMOD(&mod->mod,(u8*)mem)==0) {
		MODPlay_AllocSFXChannels(mod,mod->numSFXChans);
		return 0;
	}
	return -1;
}

s32 MODPlay_Start(MODPlay *mod)
{
	if(mod->playing) return -1;
	if(mod->mod.modraw==NULL) return -1;
	
	updateWaveFormat(mod);
	MOD_Start(&mod->mod);
	if(SndBufStart(&mod->soundBuf)<0) return -1;
	mod->playing = TRUE;
	return 0;
}

s32 MODPlay_Stop(MODPlay *mod)
{
	if(!mod->playing) return -1;

	SndBufStop();
	mod->playing = FALSE;
	return 0;
}

s32 MODPlay_AllocSFXChannels(MODPlay *mod,u32 sfxchans)
{
	if(mod->mod.modraw==NULL) return -1;
	
	if(MOD_AllocSFXChannels(&mod->mod,sfxchans)==0) {
		mod->numSFXChans = sfxchans;
		return 0;
	}
	return -1;
}

s32 MODPlay_Pause(MODPlay *mod,BOOL pause)
{
	if(!mod->playing) return -1;
	mod->paused = pause;
	return 0;
}

s32 MODPlay_TriggerNote(MODPlay *mod,u32 chan,u8 inst,u16 freq,u8 vol)
{
	if(mod->mod.modraw==0) return -1;
	return MOD_TriggerNote(&mod->mod,chan,inst,freq,vol);
}

// add by Hermes

/* void MODPlay_SetVolume(MODPlay *mod, s32 musicvolume, s32 sfxvolume)

Set the volume levels for the MOD music (call it after MODPlay_SetMOD())

mod: the MODPlay pointer

musicvolume: in range 0 to 64
sfxvolume: in range 0 to 64

*/

void MODPlay_SetVolume(MODPlay *mod, s32 musicvolume, s32 sfxvolume)
{
	if(musicvolume<0) musicvolume=0;
	if(musicvolume>64) musicvolume=64;

	if(sfxvolume<0) sfxvolume=0;
	if(sfxvolume>64) sfxvolume=64;

	mod->mod.musicvolume= musicvolume;
	mod->mod.sfxvolume = sfxvolume;
}

