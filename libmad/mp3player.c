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
#include <samplerate.h>
#include <malloc.h>

#define STACKSIZE ( 3 * 8192 )

#define INPUT_BUFFER_SIZE ( 16 * 8192 )
#define OUTPUT_BUFFER_SIZE ( 8 * 8192 )
#define OUTPUT_BUFFER_PADDING ( OUTPUT_BUFFER_SIZE )
#define OUTPUT_BUFFER_PADDED ( OUTPUT_BUFFER_SIZE + OUTPUT_BUFFER_PADDING )

static unsigned char * InputBuffer;
static unsigned char OutputBuffer[2][( OUTPUT_BUFFER_PADDED )] __attribute__ ( ( aligned ( 32 ) ) );
static const unsigned char * OutputBufferEnd[] = { ( OutputBuffer[0] + OUTPUT_BUFFER_SIZE ), ( OutputBuffer[1] + OUTPUT_BUFFER_SIZE ) };
static float * UpsampleInput; static float * UpsampleOutput; SRC_DATA * USData; double AISampleRate;
static unsigned short CurrentBuffer; static unsigned char * pOutput;

static bool MP3Playing = false;

static void AllocateMemory ();
static void DeallocateMemory ();
static void * StreamPlay ( void * );
static u8 StreamPlay_Stack[STACKSIZE];
static lwp_t hStreamPlay;
static lwpq_t thQueue;

static signed short FixedToShort ( mad_fixed_t Fixed );
static void Resample ( struct mad_pcm * FrameInfo );

static void DataTransferCallback ();

static unsigned int DSPSampleRate ( unsigned int Input );

struct MP3Source
{
	const void * MP3Stream;
	u32 Length;
};

int MP3Player_Play ( const void * MP3Stream, u32 Length )
{
	unsigned int Result = 0;

	struct MP3Source * Source = malloc ( sizeof  ( struct MP3Source ) );
	Source->MP3Stream = MP3Stream;
	Source->Length = Length;

	if ( ! LWP_CreateThread ( & hStreamPlay, StreamPlay, Source, StreamPlay_Stack, STACKSIZE, 150 ) )
	{
		Result = -1;
	}

	return Result;
}

bool MP3Player_IsPlaying ()
{
	return MP3Playing;
}

static void * StreamPlay ( void * Music )
{
	if ( MP3Playing ) { return 0; }

	MP3Playing = true;

	struct MP3Source * Source = ( struct MP3Source * ) Music;
	const void * MP3Stream = Source->MP3Stream;
	u32 Length = Source->Length;

	free ( Source );

	AllocateMemory ();

	struct mad_stream Stream;
	struct mad_frame Frame;
	struct mad_synth Synth;
	mad_timer_t Timer;

	const void * pStream = MP3Stream;
	void * pGuard;
	CurrentBuffer = 0;
	pOutput = OutputBuffer[CurrentBuffer];
	unsigned short FirstTransfer = 1;

	LWP_InitQueue ( & thQueue );
	AUDIO_Init ( NULL );
	AUDIO_SetDSPSampleRate ( AI_SAMPLERATE_48KHZ );
	AUDIO_RegisterDMACallback ( DataTransferCallback );

	AISampleRate = DSPSampleRate ( AUDIO_GetDSPSampleRate () );

	memset ( OutputBuffer[0], 0, ( OUTPUT_BUFFER_PADDED ) );
	DCFlushRange ( OutputBuffer[0], ( OUTPUT_BUFFER_PADDED ) );
	memset ( OutputBuffer[1], 0, ( OUTPUT_BUFFER_PADDED ) );
	DCFlushRange ( OutputBuffer[1], ( OUTPUT_BUFFER_PADDED ) );

	mad_stream_init ( & Stream );
	mad_frame_init ( & Frame );
	mad_synth_init ( & Synth );
	mad_timer_reset ( & Timer );

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

			memcpy ( ReadStart, pStream, ReadSize );

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

		unsigned int n; signed short Sample;

		for ( n = 0; n < Synth.pcm.length; n++ )
		{
			Sample = FixedToShort ( Synth.pcm.samples[0][n] );

			* ( pOutput++ ) = Sample >> 8;
			* ( pOutput++ ) = Sample & 0xff;

			if ( MAD_NCHANNELS ( & Frame.header ) == 2 )
			{
				Sample = FixedToShort ( Synth.pcm.samples[1][n] );
			}

			* ( pOutput++ ) = Sample >> 8;
			* ( pOutput++ ) = Sample & 0xff;
			
			if ( pOutput == OutputBufferEnd[CurrentBuffer] )
			{
				//Resample ( & Synth.pcm );
				DCFlushRange ( OutputBuffer[CurrentBuffer], OUTPUT_BUFFER_SIZE );

				if ( FirstTransfer )
				{
					FirstTransfer = 0;

					AUDIO_InitDMA ( ( unsigned int ) OutputBuffer[CurrentBuffer], OUTPUT_BUFFER_SIZE );
					AUDIO_StartDMA ();
				}

				LWP_SleepThread ( thQueue );
			}
		}
	}

	mad_synth_finish ( & Synth );
	mad_frame_finish ( & Frame );
	mad_stream_finish ( & Stream );

        AUDIO_StopDMA ();
	AUDIO_RegisterDMACallback ( NULL );
	LWP_CloseQueue ( thQueue );

	DeallocateMemory ();

	MP3Playing = false;

	return 0;
}

void AllocateMemory ()
{
	InputBuffer = memalign ( 32, ( sizeof ( unsigned char ) * ( INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD ) ) );
	UpsampleInput = memalign ( 32, ( sizeof ( float ) * OUTPUT_BUFFER_SIZE ) );
	UpsampleOutput = memalign ( 32, ( sizeof ( float ) * OUTPUT_BUFFER_SIZE ) );
	USData = malloc ( sizeof ( SRC_DATA ) );

	memset ( USData, 0, sizeof ( SRC_DATA ) );
	USData->data_in = UpsampleInput;
	USData->data_out = UpsampleOutput;
	USData->input_frames = ( OUTPUT_BUFFER_SIZE / 2 );
	USData->output_frames = USData->input_frames;
	USData->end_of_input = 0;
}

void DeallocateMemory ()
{
	free ( InputBuffer );
	free ( UpsampleInput );
	free ( UpsampleOutput );
	free ( USData );
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

static void Resample ( struct mad_pcm * FrameInfo )
{
	/*
		Resamping for some reason gives corrupted or rather bad output at the momment.
		This is something that I would definately like to fix in the near future!
		One thing I would like to try is to reverse the endian for the upsampling.
		For the rest things seem to work and with the above buffer sizes upsampling does not
		consume too much time.
	*/

	src_short_to_float_array ( ( short * ) OutputBuffer[CurrentBuffer], UpsampleInput, OUTPUT_BUFFER_SIZE );

	USData->src_ratio = ( AISampleRate / ( ( double ) FrameInfo->samplerate ) );
	src_simple ( USData, SRC_SINC_FASTEST, 2 );

	src_float_to_short_array ( UpsampleOutput, ( short * ) OutputBuffer[CurrentBuffer], OUTPUT_BUFFER_SIZE );
}

static void DataTransferCallback ()
{
	AUDIO_StopDMA ();
	AUDIO_InitDMA ( ( unsigned int ) OutputBuffer[CurrentBuffer], OUTPUT_BUFFER_SIZE );
	AUDIO_StartDMA ();

	CurrentBuffer ^= 1;
	pOutput = OutputBuffer[CurrentBuffer];

	LWP_WakeThread ( thQueue );
}

unsigned int DSPSampleRate ( unsigned int Input )
{
	if ( ! Input )
	{
		return 32000;
	}
	else
	{
		return 48000;
	}
}
