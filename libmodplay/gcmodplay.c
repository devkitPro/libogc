#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "cache.h"
#include "lwp.h"
#include "lwp_messages.h"
#include "gcmodplay.h"

//#define _GCMOD_DEBUG
#define AUDIO_DMA_REQ	3
#define STACKSIZE		16384
#define SNDBUFFERSIZE	3523<<2

static BOOL thr_running = FALSE;
static BOOL sndPlaying = FALSE;
static MODSNDBUF sndBuffer;

static u32 shiftVal = 0;
static vu32 curaudio = 0;
static u32 dma_stack[(STACKSIZE/2)/sizeof(u32)];
static u8 audioBuf[2][SNDBUFFERSIZE] ATTRIBUTE_ALIGN(32);

static mq_cntrl playermq;
static lwp_t hplayer;
static u32 player_stack[STACKSIZE/sizeof(u32)];
static void* player(void *);

#ifdef _GCMOD_DEBUG
extern int printk(const char *fmt,...);
#endif

static void* player(void *arg)
{
	u32 datalen,tmp;
#ifdef _GCMOD_DEBUG
	time_t start,end;
#endif

	thr_running = TRUE;
	while(sndPlaying==TRUE) {
		datalen = 0;
		while(!datalen) {
#ifdef _GCMOD_DEBUG
			printf("player(get req)\n\n");
#endif
			__lwp_thread_dispatchdisable();
			__lwpmq_seize(&playermq,AUDIO_DMA_REQ,(void*)&datalen,&tmp,TRUE,LWP_THREADQ_NOTIMEOUT);
			__lwp_thread_dispatchenable();
#ifdef _GCMOD_DEBUG
			printf("player(got req,%d)\n",datalen);
#endif
		}
		if(datalen!=-1 && sndPlaying==TRUE) {
#ifdef _GCMOD_DEBUG
			printf("player(run callback)\n\n");
			start = gettime();
#endif
			curaudio ^= 1;
			sndBuffer.callback(sndBuffer.usr_data,(u8*)audioBuf[curaudio],datalen);
			DCFlushRange(audioBuf[curaudio],datalen);
#ifdef _GCMOD_DEBUG
			end = gettime();
			printf("player(end callback,%d - %d us)\n\n",curaudio,diff_usec(start,end));
#endif
		}
	}
	thr_running = FALSE;
	return 0;
}

static void dmaCallback()
{	
	u32 cnt;
	static u32 datalen = 0;
	MODPlay *mp = (MODPlay*)sndBuffer.usr_data;
	MOD *mod = &mp->mod;

	if(!datalen) datalen = sndBuffer.data_len;
#ifdef _GCMOD_DEBUG
	static time_t start = 0,end = 0;

	end = gettime();
	if(start) printk("dmaCallback(%p,%d,%d - after %d ms)\n",(void*)audioBuf[curaudio],datalen,curaudio,diff_msec(start,end));
#endif
	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)audioBuf[curaudio],datalen);
	AUDIO_StartDMA();

	datalen = mod->samplespertick<<2;
	__lwpmq_broadcast(&playermq,(void*)&datalen,sizeof(u32),AUDIO_DMA_REQ,&cnt);
#ifdef _GCMOD_DEBUG
	start = gettime();
	printk("dmaCallback(%p,%d,%d,%d us) leave\n",(void*)audioBuf[curaudio],datalen,curaudio,diff_usec(end,start));
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

static u32 SndBufStart(MODSNDBUF *sndbuf)
{
	u32 datalen;
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

	sndBuffer.data_len = datalen = mod->samplespertick<<2;

	memset(audioBuf[0],0,SNDBUFFERSIZE);
	memset(audioBuf[1],0,SNDBUFFERSIZE);

	DCFlushRange(audioBuf[0],SNDBUFFERSIZE);
	DCFlushRange(audioBuf[1],SNDBUFFERSIZE);

	AUDIO_RegisterDMACallback(dmaCallback);

	while(thr_running);

	curaudio = 0;
	sndPlaying = TRUE;
	if(LWP_CreateThread(&hplayer,player,NULL,player_stack,STACKSIZE,80)!=-1) {
		AUDIO_InitDMA((u32)audioBuf[curaudio],datalen);
		AUDIO_StartDMA();
		return 1;
	}
	return -1;
}

static void SndBufStop()
{
	u32 close = -1;

	if(!sndPlaying) return;

	curaudio = 0;
	sndPlaying = FALSE;
	
	__lwpmq_send(&playermq,&close,sizeof(u32),AUDIO_DMA_REQ,FALSE,LWP_THREADQ_NOTIMEOUT);
	while(thr_running==TRUE);

	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
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
	mq_attr attr;

	memset(mod,0,sizeof(MODPlay));

	AUDIO_Init((u8*)&(dma_stack[(STACKSIZE/2)/sizeof(u32)]));
	MODPlay_SetFrequency(mod,48000);
	MODPlay_SetStereo(mod,TRUE);

	attr.mode = LWP_MQ_PRIORITY;
	__lwpmq_initialize(&playermq,&attr,8,sizeof(u32));
	
	sndPlaying = FALSE;
	thr_running = FALSE;

	mod->paused = FALSE;
	mod->bits = TRUE;
	mod->numSFXChans = 0;
	mod->manual_polling = FALSE;
}

u32 MODPlay_SetFrequency(MODPlay *mod,u32 freq)
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

u32 MODPlay_SetMOD(MODPlay *mod,u8 *mem)
{
	MODPlay_Unload(mod);

	if(MOD_SetMOD(&mod->mod,mem)==0) {
		MODPlay_AllocSFXChannels(mod,mod->numSFXChans);
		return 0;
	}
	return -1;
}

u32 MODPlay_Start(MODPlay *mod)
{
	if(mod->playing) return -1;
	if(mod->mod.modraw==NULL) return -1;
	
	updateWaveFormat(mod);
	MOD_Start(&mod->mod);
	if(SndBufStart(&mod->soundBuf)<0) return -1;
	mod->playing = TRUE;
	return 0;
}

u32 MODPlay_Stop(MODPlay *mod)
{
	if(!mod->playing) return -1;

	SndBufStop();
	mod->playing = FALSE;
	return 0;
}

u32 MODPlay_AllocSFXChannels(MODPlay *mod,u32 sfxchans)
{
	if(mod->mod.modraw==NULL) return -1;
	
	if(MOD_AllocSFXChannels(&mod->mod,sfxchans)==0) {
		mod->numSFXChans = sfxchans;
		return 0;
	}
	return -1;
}

u32 MODPlay_Pause(MODPlay *mod,BOOL pause)
{
	if(!mod->playing) return -1;
	mod->paused = TRUE;
	return 0;
}

u32 MODPlay_TriggerNote(MODPlay *mod,u32 chan,u8 inst,u16 freq,u8 vol)
{
	if(mod->mod.modraw==0) return -1;
	return MOD_TriggerNote(&mod->mod,chan,inst,freq,vol);
}
