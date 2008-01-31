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

# ifndef LIBMAD_HUFFMAN_H
# define LIBMAD_HUFFMAN_H

#include <gctypes.h>

union huffquad {
  struct {
    u16 final  :  1;
    u16 bits   :  3;
    u16 offset : 12;
  } ptr;
  struct {
    u16 final  :  1;
    u16 hlen   :  3;
    u16 v      :  1;
    u16 w      :  1;
    u16 x      :  1;
    u16 y      :  1;
  } value;
  u16 final    :  1;
};

union huffpair {
  struct {
    u16 final  :  1;
    u16 bits   :  3;
    u16 offset : 12;
  } ptr;
  struct {
    u16 final  :  1;
    u16 hlen   :  3;
    u16 x      :  4;
    u16 y      :  4;
  } value;
  u16 final    :  1;
};

struct hufftable {
  union huffpair const *table;
  u16 linbits;
  u16 startbits;
};

extern union huffquad const *const mad_huff_quad_table[2];
extern struct hufftable const mad_huff_pair_table[32];

# endif
