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

# ifndef LIBMAD_BIT_H
# define LIBMAD_BIT_H

#include <gctypes.h>

struct mad_bitptr {
  u8 const *byte;
  u16 cache;
  u16 left;
};

void mad_bit_init(struct mad_bitptr *, u8 const *);

# define mad_bit_finish(bitptr)		/* nothing */

u32 mad_bit_length(struct mad_bitptr const *,
			    struct mad_bitptr const *);

# define mad_bit_bitsleft(bitptr)  ((bitptr)->left)
u8 const *mad_bit_nextbyte(struct mad_bitptr const *);

void mad_bit_skip(struct mad_bitptr *, u32);
u32 mad_bit_read(struct mad_bitptr *, u32);
void mad_bit_write(struct mad_bitptr *, u32, u32);

u16 mad_bit_crc(struct mad_bitptr, u32, u16);

# endif
