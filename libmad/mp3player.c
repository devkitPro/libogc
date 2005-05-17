/***************************************************************************
 *   Low Level MAD Library Implementation for the GameCube                 *
 *   By Daniel "nForce" Bloemendal                                         *
 *   nForce@Sympatico.ca                                                   *
 ***************************************************************************/

#include <mad.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lwp.h>
#include <pad.h>
#include <limits.h>
#include <system.h>
#include <ogcsys.h>
#include <malloc.h>

#define STACKSIZE				(4*8192)

#define INPUT_BUFFER_SIZE		(5*8192)
#define OUTPUT_BUFFER_SIZE		(8192)

static u8 InputBuffer[INPUT_BUFFER_SIZE+MAD_BUFFER_GUARD];
static u8 OutputBuffer[2][OUTPUT_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);

static u32 init_done = 0;
static u32 CurrentTXLen = 0;
static u32 FirstTransfer = 1;
static u32 CurrentBuffer = 0;
static f64 AISampleRate = 0.0f;
static BOOL thr_running = FALSE;
static BOOL MP3Playing = FALSE;

static void* StreamPlay(void *);
static u8 StreamPlay_Stack[STACKSIZE];
static lwp_t hStreamPlay;
static lwpq_t thQueue;

static s16 FixedToShort(mad_fixed_t Fixed);
static s32 Resample(struct mad_pcm *Pcm,u8 *pOutput,u32 stereo);
static void DataTransferCallback();
static u32 DSPSampleRate(u32 Input);

struct MP3Source
{
	const void * MP3Stream;
	u32 Length;
} mp3_source;

void MP3Player_Init()
{
	if(!init_done) {
		init_done = 1;
		AUDIO_Init(NULL);
		AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	}
}

s32 MP3Player_Play ( const void * MP3Stream, u32 Length )
{
	if(MP3Playing==TRUE) return -1;

	mp3_source.MP3Stream = MP3Stream;
	mp3_source.Length = Length;

	CurrentBuffer = 0;
	FirstTransfer = 1;
	memset(OutputBuffer[0],0,(OUTPUT_BUFFER_SIZE));
	memset(OutputBuffer[1],0,(OUTPUT_BUFFER_SIZE));
	DCFlushRange(OutputBuffer[0],(OUTPUT_BUFFER_SIZE));
	DCFlushRange(OutputBuffer[1],(OUTPUT_BUFFER_SIZE));

	while(thr_running);

	MP3Playing = TRUE;
	if(LWP_CreateThread(&hStreamPlay,StreamPlay,NULL,StreamPlay_Stack,STACKSIZE,80)<0) {
		MP3Playing = FALSE;
		return -1;
	}

	CurrentTXLen = OUTPUT_BUFFER_SIZE;
	AUDIO_InitDMA((u32)OutputBuffer[CurrentBuffer],CurrentTXLen);
	AUDIO_StartDMA();
	return 0;
}

void MP3Player_Stop()
{
	if(MP3Playing==FALSE) return;
	
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	
	CurrentTXLen = 0;
	CurrentBuffer = 0;
	MP3Playing = FALSE;
	LWP_WakeThread(thQueue);
	LWP_JoinThread(hStreamPlay,NULL);
}

BOOL MP3Player_IsPlaying()
{
	return MP3Playing;
}

static void *StreamPlay(void *arg)
{
	u32 len;
	void *pGuard;
	const void *pStream;
	struct mad_stream Stream;
	struct mad_frame Frame;
	struct mad_synth Synth;
	mad_timer_t Timer;

	thr_running = TRUE;

	AUDIO_RegisterDMACallback(DataTransferCallback);
	AISampleRate = DSPSampleRate(AUDIO_GetDSPSampleRate());

	LWP_InitQueue(&thQueue);

	mad_stream_init(&Stream);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);
	mad_timer_reset(&Timer);

	len = mp3_source.Length;
	pStream = mp3_source.MP3Stream;
	while(MP3Playing==TRUE) {
		LWP_SleepThread(thQueue);
		if(Stream.buffer==NULL || Stream.error==MAD_ERROR_BUFLEN) {
			size_t ReadSize, Remaining;
			u32 Offset;
			u8 *ReadStart;

			if(Stream.next_frame!=NULL) {
				Remaining = Stream.bufend - Stream.next_frame;
				memmove(InputBuffer,Stream.next_frame,Remaining);
				ReadStart = InputBuffer + Remaining;
				ReadSize = INPUT_BUFFER_SIZE - Remaining;
			} else {
				ReadSize = INPUT_BUFFER_SIZE;
				ReadStart = InputBuffer;
				Remaining = 0;
			}


			Offset = (pStream - mp3_source.MP3Stream);
			if((Offset + ReadSize)>mp3_source.Length) {
				ReadSize = mp3_source.Length - Offset;
			}
			if(ReadSize<=0) break;

			memcpy(ReadStart,pStream,ReadSize);
			pStream = pStream + ReadSize;

			if((mp3_source.Length - (Offset + ReadSize))==0) {
				pGuard = ReadStart + ReadSize;
				memset(pGuard,0,MAD_BUFFER_GUARD);
				ReadSize += MAD_BUFFER_GUARD;
			}

			mad_stream_buffer(&Stream,InputBuffer,(ReadSize + Remaining));
			Stream.error = 0;
		}

		if(mad_frame_decode(&Frame,&Stream)) {
			if(MAD_RECOVERABLE(Stream.error)) continue;
			else {
				if(Stream.error==MAD_ERROR_BUFLEN)continue;
				else break;
			}
		}

		mad_timer_add(&Timer,Frame.header.duration);
		mad_synth_frame(&Synth,&Frame);

		CurrentTXLen = Resample(&Synth.pcm,OutputBuffer[CurrentBuffer],(MAD_NCHANNELS(&Frame.header)==2));
	}

	mad_synth_finish(&Synth);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);

    AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	LWP_CloseQueue(thQueue);

	thr_running = FALSE;
	MP3Playing = FALSE;

	return 0;
}

static __inline__ s16 FixedToShort(mad_fixed_t Fixed)
{
	Fixed=Fixed>>(MAD_F_FRACBITS-14);
	return((signed short)Fixed);
}

#define FRAC		65536
#define FRACSHIFT	16

typedef union {
	struct {
		u16 hi;
		u16 lo;
	} aword;
	u32 adword;
} dword;

/* Simple Linear Interpolation */
static s32 Resample(struct mad_pcm *Pcm,u8 *pOutput,u32 stereo)
{
	u32 len; 
	dword pos;
	s16 Sample[2][2] = {{0,0},{0,0}};
	s16 fs[2] = {0,0};
	u8 *pOut = pOutput;

	len = 0;
	pos.adword = 0;
	while(pos.aword.hi<Pcm->length) {
		Sample[0][0] = Sample[1][0];
		Sample[0][1] = Sample[1][1];
		Sample[1][0] = FixedToShort(Pcm->samples[0][pos.aword.hi]);
		if(stereo) Sample[1][1] = FixedToShort(Pcm->samples[1][pos.aword.hi]);
		else Sample[1][1] = Sample[1][0];

		fs[0] = Sample[0][0] + (((Sample[1][0] - Sample[0][0]) * pos.aword.lo) >> FRACSHIFT);
		fs[1] = Sample[0][1] + (((Sample[1][1] - Sample[0][1]) * pos.aword.lo) >> FRACSHIFT);

		pOut[len++] = (fs[0]>>8);
		pOut[len++] = (fs[0]&0xff);
		pOut[len++] = (fs[1]>>8);
		pOut[len++] = (fs[1]&0xff);

		pos.adword += 60211;
	}

	DCFlushRange(pOutput,len);
	return len;
}

static void DataTransferCallback()
{
	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)OutputBuffer[CurrentBuffer],CurrentTXLen);
	AUDIO_StartDMA();

	CurrentBuffer ^= 1;
	LWP_WakeThread(thQueue);
}

u32 DSPSampleRate(u32 Input)
{
	if(!Input) 
		return 32000;
	else 
		return 48000;
}
