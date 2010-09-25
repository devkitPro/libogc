// Modified by Francisco Mu�oz 'Hermes' MAY 2008
// Changed to use libAESND
// linke aginst libaesnd.a - -laesnd

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gccore.h>
#include <aesndlib.h>
#include "gcmodplay.h"

//#define _GCMOD_DEBUG

#define STACKSIZE		8192
#define SNDBUFFERSIZE	(5760)			//that's the maximum buffer size

static BOOL thr_running = FALSE;
static BOOL sndPlaying = FALSE;
static MODSNDBUF sndBuffer;

static u32 shiftVal = 0;
static vu32 curr_audio = 0;
static u8 audioBuf[2][SNDBUFFERSIZE] ATTRIBUTE_ALIGN(32);

static lwpq_t player_queue;
static lwp_t hplayer;
static u8 player_stack[STACKSIZE] ATTRIBUTE_ALIGN(8);
static void* player(void *);

#ifdef __AESNDLIB_H__
static s32 mod_freq = 48000;
static AESNDPB	*modvoice = NULL;
#endif

#ifdef _GCMOD_DEBUG
static u64 mixtime = 0;
static u64 reqcbtime = 0;
extern u32 diff_usec(unsigned long long start,unsigned long long end);
extern u32 diff_msec(unsigned long long start,unsigned long long end);
#endif

static void* player(void *arg)
{
#ifdef _GCMOD_DEBUG
	u64 start;
#endif

	thr_running = TRUE;
	while(sndPlaying==TRUE) {
		LWP_ThreadSleep(player_queue);
		if(sndPlaying==TRUE) {
#ifdef _GCMOD_DEBUG
			start = gettime();
#endif
			sndBuffer.callback(sndBuffer.usr_data,((u8*)audioBuf[curr_audio]),SNDBUFFERSIZE);
#ifdef _GCMOD_DEBUG
			mixtime = gettime();
#endif
		}
	}
	thr_running = FALSE;

	return 0;
}

#ifdef __AESNDLIB_H__
static void __aesndvoicecallback(AESNDPB *pb,u32 state)
{
#ifdef _GCMOD_DEBUG
	static u64 rqstart = 0,rqend = 0;

	rqend = gettime();
	if(rqstart) reqcbtime = rqend - rqstart;
#endif
	switch(state) {
		case VOICE_STATE_STOPPED:
		case VOICE_STATE_RUNNING:
			break;
		case VOICE_STATE_STREAM:
			AESND_SetVoiceBuffer(pb,(void*)audioBuf[curr_audio],SNDBUFFERSIZE);
			LWP_ThreadSignal(player_queue);
			curr_audio ^= 1;
			break;
	}
#ifdef _GCMOD_DEBUG
	rqstart = gettime();
#endif
}

#else

static void dmaCallback()
{	
#ifdef _GCMOD_DEBUG
	static u64 adstart = 0,adend = 0;

	adend = gettime();
	if(adstart) printf("dmaCallback(%d us)\n",diff_usec(adstart,adend));
#endif

	curr_audio ^= 1;
	AUDIO_InitDMA((u32)audioBuf[curr_audio],SNDBUFFERSIZE);
	LWP_ThreadSignal(player_queue);

#ifdef _GCMOD_DEBUG
	adstart = gettime();
#endif
}
#endif

static void mixCallback(void *usrdata,u8 *stream,u32 len)
{
	u32 i;
	MODPlay *mp = (MODPlay*)usrdata;
	MOD *mod = &mp->mod;

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

#ifndef __AESNDLIB_H__
	DCFlushRange(stream,len);
#endif
}

static s32 SndBufStart(MODSNDBUF *sndbuf)
{
	if(sndPlaying) return -1;

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
	if(LWP_CreateThread(&hplayer,player,NULL,player_stack,STACKSIZE,80)!=-1) {
#ifndef __AESNDLIB_H__
		AUDIO_RegisterDMACallback(dmaCallback);
		AUDIO_InitDMA((u32)audioBuf[curr_audio],SNDBUFFERSIZE);
		AUDIO_StartDMA();
#else
		AESND_SetVoiceStop(modvoice,false);
#endif
		return 1;
	}
	sndPlaying = FALSE;

	return -1;
}

static void SndBufStop()
{
	if(!sndPlaying) return;
#ifndef __AESNDLIB_H__
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
#else
	AESND_SetVoiceStop(modvoice,true);
#endif
	curr_audio = 0;
	sndPlaying = FALSE;
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

#ifndef __AESNDLIB_H__
    AUDIO_Init(NULL);
#else
	modvoice = AESND_AllocateVoice(__aesndvoicecallback);
	if(modvoice) {
		AESND_SetVoiceFormat(modvoice,VOICE_STEREO16);
		AESND_SetVoiceFrequency(modvoice,mod_freq);
		AESND_SetVoiceVolume(modvoice,255,255);
		AESND_SetVoiceStream(modvoice,true);
	}
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
#ifndef __AESNDLIB_H__
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

#ifdef _GCMOD_DEBUG
u32 MODPlay_MixingTime()
{
	return ticks_to_microsecs(mixtime);
}

u32 MODPlay_RequestTime()
{
	return ticks_to_microsecs(reqcbtime);
}
#endif
