#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <gccore.h>
#include <ogcsys.h>
#include <mad.h>

#define STACKSIZE			(8192*4)
#define INPUT_BUFFER_SIZE	(5*8192)
#define OUTPUT_BUFFER_SIZE	8192

static u32 curr_audio = 0;
static u32 first_frame = 1;
static u32 thr_running = FALSE;
static boolean mp3_playing = FALSE;
static u8 InputBuffer[INPUT_BUFFER_SIZE+MAD_BUFFER_GUARD];
static u8 OutputBuffer[2][OUTPUT_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);

static u8 streamplay_stack[STACKSIZE];
static lwp_t hstreamplay;
static lwpq_t streamplay_queue;
static void* streamplay(void*);

static struct mad_stream Stream;
static struct mad_frame Frame;
static struct mad_synth Synth;
static mad_timer_t Timer;

static s16 fixed_to_short(mad_fixed_t Fixed);
static void dma_callback();

struct _mp3source
{
	const void * mp3stream;
	u32 len;
} mp3source;

void MP3Player_Init()
{
	AUDIO_Init(NULL);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);

	LWP_InitQueue(&streamplay_queue);
}

s32 MP3Player_Play(const void *mp3stream, u32 len)
{
	if(thr_running==TRUE) return -1;
	
	mp3source.mp3stream = mp3stream;
	mp3source.len = len;
	
	mp3_playing = TRUE;
	AUDIO_RegisterDMACallback(dma_callback);
	if(LWP_CreateThread(&hstreamplay,streamplay,NULL,streamplay_stack,STACKSIZE,80)!=-1) return 0;

	return -1;
}

void MP3Player_Stop()
{
	if(!mp3_playing) return;

	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);

	mp3_playing = FALSE;
	LWP_WakeThread(streamplay_queue);
	LWP_JoinThread(hstreamplay,NULL);
}

static void* streamplay(void *arg)
{
	u32 len;
	u8 *pOutput;
	void *pGuard;
	const void *curr_streampos;

	thr_running = TRUE;
	curr_audio = 0;
	first_frame = 1;

restart:
	curr_streampos = mp3source.mp3stream;
	len = mp3source.len;

	mad_stream_init(&Stream);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);
	mad_timer_reset(&Timer);

	while(mp3_playing==TRUE) {
		if(Stream.buffer==NULL || Stream.error==MAD_ERROR_BUFLEN) {
			size_t ReadSize, Remaining;
			u32 Offset;
			u8 *ReadStart;

			if(Stream.next_frame!=NULL) {
				Remaining = Stream.bufend - Stream.next_frame;
				memmove(InputBuffer, Stream.next_frame, Remaining );
				ReadStart = InputBuffer + Remaining;
				ReadSize = INPUT_BUFFER_SIZE - Remaining;
			} else {
				ReadSize = INPUT_BUFFER_SIZE;
				ReadStart = InputBuffer;
				Remaining = 0;
			}


			Offset = (curr_streampos - mp3source.mp3stream);
			if((Offset+ReadSize)>len) ReadSize = len - Offset;
			if(ReadSize<=0) {
				mad_synth_finish(&Synth);
				mad_frame_finish(&Frame);
				mad_stream_finish(&Stream);
				goto restart;
			}

			memcpy(ReadStart,curr_streampos,ReadSize);
			curr_streampos += ReadSize;

			if((len-(Offset+ReadSize))==0) {
				pGuard = ReadStart + ReadSize;
				memset(pGuard,0,MAD_BUFFER_GUARD);
				ReadSize += MAD_BUFFER_GUARD;
			}

			mad_stream_buffer(&Stream,InputBuffer,ReadSize+Remaining);
			Stream.error = 0;
		}

		if(mad_frame_decode(&Frame,&Stream)) {
			if (MAD_RECOVERABLE(Stream.error)) continue;
			else {
				if(Stream.error==MAD_ERROR_BUFLEN) continue;
				else break;
			}
		}

		mad_timer_add(&Timer,Frame.header.duration);
		mad_synth_frame(&Synth,&Frame);

		u32 n;
		s16 Sample;
		pOutput = OutputBuffer[curr_audio];
		for ( n = 0; n < Synth.pcm.length; n++ )
		{
			Sample = fixed_to_short(Synth.pcm.samples[0][n]);
			*(pOutput++) = Sample>>8;
			*(pOutput++) = Sample&0xff;
			
			Sample = fixed_to_short(Synth.pcm.samples[1][n]);
			*(pOutput++) = Sample>>8;
			*(pOutput++) = Sample&0xff;
		}
		DCFlushRange(OutputBuffer[curr_audio],Synth.pcm.length<<2);
		if(first_frame) {
			first_frame = 0;
			AUDIO_InitDMA((u32)OutputBuffer[curr_audio],Synth.pcm.length<<2);
			AUDIO_StartDMA ();
			curr_audio ^= 1;
			continue;
		}
		LWP_SleepThread(streamplay_queue);
	}

	mad_synth_finish(&Synth);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);

	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	
	mp3_playing = FALSE;
	thr_running = FALSE;
	
	return 0;
}

static s16 fixed_to_short(mad_fixed_t Fixed)
{
	if(Fixed>=MAD_F_ONE) return (SHRT_MAX);
	if(Fixed<=-MAD_F_ONE)return (-SHRT_MAX);

	Fixed = Fixed>>(MAD_F_FRACBITS-15);
	return ((s16)Fixed);
}

static void dma_callback()
{
	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)OutputBuffer[curr_audio], Synth.pcm.length<<2);
	AUDIO_StartDMA ();

	curr_audio ^= 1;
	LWP_WakeThread(streamplay_queue);
}
