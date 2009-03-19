/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2003 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

# ifndef LIBMAD_STREAM_H
# define LIBMAD_STREAM_H

#include <gctypes.h>
# include "bit.h"

# define MAD_BUFFER_GUARD	8
# define MAD_BUFFER_MDLEN	(511 + 2048 + MAD_BUFFER_GUARD)

enum mad_error {
  MAD_ERROR_NONE	   = 0x0000,	/* no error */

  MAD_ERROR_BUFLEN	   = 0x0001,	/* input buffer too small (or EOF) */
  MAD_ERROR_BUFPTR	   = 0x0002,	/* invalid (null) buffer pointer */

  MAD_ERROR_NOMEM	   = 0x0031,	/* not enough memory */

  MAD_ERROR_LOSTSYNC	   = 0x0101,	/* lost synchronization */
  MAD_ERROR_BADLAYER	   = 0x0102,	/* reserved header layer value */
  MAD_ERROR_BADBITRATE	   = 0x0103,	/* forbidden bitrate value */
  MAD_ERROR_BADSAMPLERATE  = 0x0104,	/* reserved sample frequency value */
  MAD_ERROR_BADEMPHASIS	   = 0x0105,	/* reserved emphasis value */

  MAD_ERROR_BADCRC	   = 0x0201,	/* CRC check failed */
  MAD_ERROR_BADBITALLOC	   = 0x0211,	/* forbidden bit allocation value */
  MAD_ERROR_BADSCALEFACTOR = 0x0221,	/* bad scalefactor index */
  MAD_ERROR_BADFRAMELEN	   = 0x0231,	/* bad frame length */
  MAD_ERROR_BADBIGVALUES   = 0x0232,	/* bad big_values count */
  MAD_ERROR_BADBLOCKTYPE   = 0x0233,	/* reserved block_type */
  MAD_ERROR_BADSCFSI	   = 0x0234,	/* bad scalefactor selection info */
  MAD_ERROR_BADDATAPTR	   = 0x0235,	/* bad main_data_begin pointer */
  MAD_ERROR_BADPART3LEN	   = 0x0236,	/* bad audio data length */
  MAD_ERROR_BADHUFFTABLE   = 0x0237,	/* bad Huffman table select */
  MAD_ERROR_BADHUFFDATA	   = 0x0238,	/* Huffman data overrun */
  MAD_ERROR_BADSTEREO	   = 0x0239	/* incompatible block_type for JS */
};

# define MAD_RECOVERABLE(error)	((error) & 0xff00)

struct mad_stream {
  u8 const *buffer;		/* input bitstream buffer */
  u8 const *bufend;		/* end of buffer */
  u32 skiplen;		/* bytes to skip before next frame */

  s32 sync;				/* stream sync found */
  u32 freerate;		/* free bitrate (fixed) */

  u8 const *this_frame;	/* start of current frame */
  u8 const *next_frame;	/* start of next frame */
  struct mad_bitptr ptr;		/* current processing bit pointer */

  struct mad_bitptr anc_ptr;		/* ancillary bits pointer */
  u32 anc_bitlen;		/* number of ancillary bits */

  u8 (*main_data)[MAD_BUFFER_MDLEN];
					/* Layer III main_data() */
  u32 md_len;			/* bytes in main_data */

  s32 options;				/* decoding options (see below) */
  enum mad_error error;			/* error code (see above) */
};

enum {
  MAD_OPTION_IGNORECRC      = 0x0001,	/* ignore CRC errors */
  MAD_OPTION_HALFSAMPLERATE = 0x0002	/* generate PCM at 1/2 sample rate */
# if 0  /* not yet implemented */
  MAD_OPTION_LEFTCHANNEL    = 0x0010,	/* decode left channel only */
  MAD_OPTION_RIGHTCHANNEL   = 0x0020,	/* decode right channel only */
  MAD_OPTION_SINGLECHANNEL  = 0x0030	/* combine channels */
# endif
};

void mad_stream_init(struct mad_stream *);
void mad_stream_finish(struct mad_stream *);

# define mad_stream_options(stream, opts)  \
    ((void) ((stream)->options = (opts)))

void mad_stream_buffer(struct mad_stream *,
		       u8 const *, u32);
void mad_stream_skip(struct mad_stream *, u32);

s32 mad_stream_sync(struct mad_stream *);

char const *mad_stream_errorstr(struct mad_stream const *);

# endif
