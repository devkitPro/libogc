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
static u8 InputBuffer[INPUT_BUFFER_SIZE+MAD_BUFFER_GUARD];
static u8 OutputBuffer[2][OUTPUT_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);

static void* streamplay(void*);
static u8 streamplay_stack[STACKSIZE];
static lwp_t hstreamplay;
static lwpq_t streamplay_queue;

static struct mad_stream Stream;
static struct mad_frame Frame;
static struct mad_synth Synth;
static mad_timer_t Timer;

static signed short FixedToShort(mad_fixed_t Fixed);
static void DataTransferCallback();

struct MP3Source
{
	const void * MP3Stream;
	u32 Length;
};

int MP3Player_Play(const void *MP3Stream, u32 Length)
{
	u32 Result = 0;
	struct MP3Source * Source = malloc(sizeof(struct MP3Source));

	Source->MP3Stream = MP3Stream;
	Source->Length = Length;
	
	if(LWP_CreateThread(&hstreamplay,streamplay,Source,streamplay_stack,STACKSIZE,80)<0) {
		Result = -1;
	}

	printf ( "MP3Player: static void* streamplay(void *) returned(%d)!\n",Result);

	return Result;
}

static void* streamplay(void *Music)
{
	struct MP3Source * Source = ( struct MP3Source * ) Music;
	const void * MP3Stream = Source->MP3Stream;
	u32 Length = Source->Length;

	unsigned char * pOutput;
	void * pGuard;
	const void * pStream = MP3Stream;

	free(Music);
	
	AUDIO_Init(NULL);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(DataTransferCallback);

	LWP_InitQueue(&streamplay_queue);

	mad_stream_init ( & Stream );
	mad_frame_init ( & Frame );
	mad_synth_init ( & Synth );
	mad_timer_reset ( & Timer );

	printf ( "MP3Player: MP3Stream: %p, Length: %d\n", MP3Stream, Length );

	first_frame = 1;
	curr_audio = 0;
	while ( true )
	{
		if ( Stream.buffer == NULL || Stream.error == MAD_ERROR_BUFLEN )
		{
			size_t ReadSize, Remaining;
			unsigned int Offset;
			unsigned char * ReadStart;

			if ( Stream.next_frame != NULL )
			{
				Remaining = Stream.bufend - Stream.next_frame;
				memmove ( InputBuffer, Stream.next_frame, Remaining );
				ReadStart = InputBuffer + Remaining;
				ReadSize = INPUT_BUFFER_SIZE - Remaining;
			}
			else
			{
				ReadSize = INPUT_BUFFER_SIZE;
				ReadStart = InputBuffer;
				Remaining = 0;
			}


			Offset = ( pStream - MP3Stream );

			if ( ( Offset + ReadSize ) > Length )
			{
				ReadSize = Length - Offset;
			}

			if ( ReadSize <= 0 )
			{
				break;
			}

			//printf ( "Copying memory... " ); fflush ( stdout );
			memcpy ( ReadStart, pStream, ReadSize );
			//printf ( "Done!\n" );

			pStream = pStream + ReadSize;

			if ( ( Length - ( Offset + ReadSize ) ) == 0 )
			{
				pGuard = ReadStart + ReadSize;
				memset ( pGuard, 0, MAD_BUFFER_GUARD );
				ReadSize += MAD_BUFFER_GUARD;
			}

			mad_stream_buffer ( & Stream, InputBuffer, ReadSize + Remaining );
			Stream.error = 0;
		}

		if ( mad_frame_decode ( & Frame, & Stream ) )
		{
			if ( MAD_RECOVERABLE ( Stream.error ) )
			{
				continue;
			}
			else
			{
				if ( Stream.error == MAD_ERROR_BUFLEN )
				{
					continue;
				}
				else
				{
					break;
				}
			}
		}
		mad_timer_add ( & Timer, Frame.header.duration );

		mad_synth_frame ( & Synth, & Frame );
		//printf("sample size: %d\n",Synth.pcm.length);

		unsigned int n;
		signed short Sample;
		pOutput = OutputBuffer[curr_audio];
		for ( n = 0; n < Synth.pcm.length; n++ )
		{
			Sample = FixedToShort ( Synth.pcm.samples[0][n] );
			* ( pOutput++ ) = Sample >> 8;
			* ( pOutput++ ) = Sample & 0xff;
			
			Sample = FixedToShort ( Synth.pcm.samples[1][n] );
			* ( pOutput++ ) = Sample >> 8;
			* ( pOutput++ ) = Sample & 0xff;
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

	mad_synth_finish ( & Synth );
	mad_frame_finish ( & Frame );
	mad_stream_finish ( & Stream );

	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	LWP_CloseQueue(streamplay_queue);
	
	return 0;
}

static signed short FixedToShort ( mad_fixed_t Fixed )
{
	if ( Fixed >= MAD_F_ONE )
	{
		return ( SHRT_MAX );
	}
	if ( Fixed <= - MAD_F_ONE )
	{
		return ( - SHRT_MAX );
	}

	Fixed = Fixed >> ( MAD_F_FRACBITS - 15 );
	return ( ( signed short ) Fixed );
}

static void DataTransferCallback ()
{
	//printf("DataTransferCallback()\n");
	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)OutputBuffer[curr_audio], Synth.pcm.length<<2);
	AUDIO_StartDMA ();

	curr_audio ^= 1;
	LWP_WakeThread(streamplay_queue);
}
