#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "cache.h"
#include "mad.h"
#include "madplayer.h"

#define INPUT_BUFFERSIZE		(5*8192)
#define OUTPUT_BUFFERSIZE		8192

struct buffer {
	void *stream;
	u32 len;
	u32 pos;
	u32 eof;
};

static struct buffer buf;
static u32 curaudio = 0;

static u8 inBuffer[INPUT_BUFFERSIZE+MAD_BUFFER_GUARD];
static u8 outBuffer[2][OUTPUT_BUFFERSIZE];

static enum mad_flow input(void *data,struct mad_stream *stream)
{
	u32 remain;
	struct buffer *buf = (struct buffer*)data;
	
	
	u8 *cpy_start,*guard;
	u32 cpy_size;
	if(stream->next_frame!=NULL) {
		remain = stream->bufend-stream->next_frame;
		memmove(inBuffer,stream->next_frame,remain);
		cpy_start = inBuffer+remain;
		cpy_size = INPUT_BUFFERSIZE - remain;
	} else {
		cpy_start = inBuffer;
		cpy_size = INPUT_BUFFERSIZE;
		remain = 0;
	}
	if(cpy_size<=(buf->len-buf->pos))
		memcpy(cpy_start,buf->stream+buf->pos,cpy_size);
	else {
		buf->eof = 1;
		cpy_size = (buf->len - buf->pos);
		memcpy(cpy_start,buf->stream+buf->pos,cpy_size);
	}
	buf->pos += cpy_size;
	
	if(buf->eof) {
		guard = cpy_start + cpy_size;
		memset(guard,0,MAD_BUFFER_GUARD);
		cpy_size += MAD_BUFFER_GUARD;
	}
	mad_stream_buffer(stream,inBuffer,cpy_size+remain);

	return MAD_FLOW_CONTINUE;
}

static inline s32 scale(mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* clip */
	if (sample >= MAD_F_ONE)
	sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
	sample = -MAD_F_ONE;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static enum mad_flow output(void *data,struct mad_header const *header,struct mad_pcm *pcm)
{
	u32 chans,samples;
	mad_fixed_t const *left, *right;
	struct buffer *buf = (struct buffer*)data;

	chans = pcm->channels;
	samples = pcm->length;
	left = pcm->samples[0];
	right = pcm->samples[1];
	
	return MAD_FLOW_CONTINUE;
}

static enum mad_flow error(void *data,struct mad_stream *stream,struct mad_frame *frame)
{
	struct buffer *buf = (struct buffer*)data;
	return MAD_FLOW_CONTINUE;
}

static void dmaCallback()
{
	u32 datalen = 0;

	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)outBuffer[curaudio],datalen);
	AUDIO_StartDMA();
}

u32 MP3_Decode(void *mp3stream,u32 len)
{
	struct mad_decoder decoder;

	buf.stream = mp3stream;
	buf.len = len;
	buf.pos = 0;
	buf.eof = 0;

	mad_decoder_init(&decoder,&buf,input,NULL,NULL,output,error,NULL);
	mad_decoder_run(&decoder,MAD_DECODER_MODE_SYNC);
	mad_decoder_finish(&decoder);

	return 1;
}
