#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gccore.h>
#include "gcmodplay.h"

//#define _GCMOD_DEBUG

#define STACKSIZE		8192
#define SNDBUFFERSIZE	3523<<2

static BOOL thr_running = FALSE;
static BOOL sndPlaying = FALSE;
static MODSNDBUF sndBuffer;

static u32 curr_datalen = 0;
static u32 shiftVal = 0;
static vu32 curaudio = 0;
static u8 *audioBuf[2][SNDBUFFERSIZE] ATTRIBUTE_ALIGN(32);

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
		LWP_SleepThread(player_queue);

		if(curr_datalen>0 && sndPlaying==TRUE) {
#ifdef _GCMOD_DEBUG
			printf("player(run callback)\n\n");
			start = gettime();
#endif
			sndBuffer.callback(sndBuffer.usr_data,(u8*)audioBuf[curaudio],curr_datalen);
			DCFlushRange(audioBuf[curaudio],curr_datalen);
#ifdef _GCMOD_DEBUG
			end = gettime();
			printf("player(end callback,%d - %d us)\n\n",curaudio,diff_usec(start,end));
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
	MODPlay *mp = (MODPlay*)sndBuffer.usr_data;
	MOD *mod = &mp->mod;

#ifdef _GCMOD_DEBUG
	static long long start = 0,end = 0;

	end = gettime();
	if(start) printf("dmaCallback(%p,%d,%d - after %d ms)\n",(void*)audioBuf[curaudio],curr_datalen,curaudio,diff_msec(start,end));
#endif
	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)audioBuf[curaudio],curr_datalen);
	AUDIO_StartDMA();

	curaudio ^= 1;
	curr_datalen = mod->samplespertick<<2;
	LWP_WakeThread(player_queue);
#ifdef _GCMOD_DEBUG
	start = gettime();
	printf("dmaCallback(%p,%d,%d,%d us) leave\n",(void*)audioBuf[curaudio],curr_datalen,curaudio,diff_usec(end,start));
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
	mod->mixingbuf = stream;
	mod->mixingbuflen = len;

	if(mp->paused) {
		for(i=0;i<(len>>1);i++)
			((u16*)stream)[i] = 0;
	} else
		MOD_Player(mod);
#ifdef _GCMOD_DEBUG
	printf("mixCallback(%p,%p,%d,%d) leave\n",stream,usrdata,len,mp->paused);
#endif
}

static s32 SndBufStart(MODSNDBUF *sndbuf)
{
	MODPlay *mp = (MODPlay*)sndbuf->usr_data;
	MOD *mod = &mp->mod;

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

	sndBuffer.data_len = curr_datalen = mod->samplespertick<<2;

	memset(audioBuf[0],0,SNDBUFFERSIZE);
	memset(audioBuf[1],0,SNDBUFFERSIZE);

	DCFlushRange(audioBuf[0],SNDBUFFERSIZE);
	DCFlushRange(audioBuf[1],SNDBUFFERSIZE);

	AUDIO_RegisterDMACallback(dmaCallback);

	while(thr_running);

	curaudio = 0;
	sndPlaying = TRUE;
	if(LWP_CreateThread(&hplayer,player,NULL,player_stack,STACKSIZE,80)!=-1) {
		AUDIO_InitDMA((u32)audioBuf[curaudio],curr_datalen);
		AUDIO_StartDMA();
		return 1;
	}

	AUDIO_RegisterDMACallback(NULL);
	sndPlaying = FALSE;
	return -1;
}

static void SndBufStop()
{
	if(!sndPlaying) return;

	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);

	curr_datalen = 0;
	curaudio = 0;
	sndPlaying = FALSE;
	LWP_WakeThread(player_queue);
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
	
	mod->soundBuf.data_len = 0;
	mod->soundBuf.fmt = 16;
	mod->soundBuf.usr_data = mod;
	mod->soundBuf.callback = mixCallback;
	mod->soundBuf.samples = (f32)mod->playfreq/50.0;
	
	if(p)
		SndBufStart(&mod->soundBuf);
	
	return 0;	
}

void MODPlay_Init(MODPlay *mod)
{
	memset(mod,0,sizeof(MODPlay));

	AUDIO_Init(NULL);
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
		if(freq==32000)
			AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
		else
			AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);

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
	mod->paused = TRUE;
	return 0;
}

s32 MODPlay_TriggerNote(MODPlay *mod,u32 chan,u8 inst,u16 freq,u8 vol)
{
	if(mod->mod.modraw==0) return -1;
	return MOD_TriggerNote(&mod->mod,chan,inst,freq,vol);
}
