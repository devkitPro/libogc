/*
Copyright (c) 2002,2003, Christian Nowak <chnowak@web.de>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are 
permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of 
  conditions and the following disclaimer. 
- Redistributions in binary form must reproduce the above copyright notice, this list 
  of conditions and the following disclaimer in the documentation and/or other 
  materials provided with the distribution. 
- The names of the contributors may not be used to endorse or promote products derived 
  from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <gctypes.h>

#undef LITTLE_ENDIAN

#ifdef __cplusplus
extern "C" {
#endif

typedef union
  {
#ifdef LITTLE_ENDIAN
    struct
      {
        u8 low;
        u8 high;
      } abyte;
#else
    struct
      {
        u8 high;
        u8 low;
      } abyte;
#endif
    u16 aword;
  } union_word;


typedef union
  {
#ifdef LITTLE_ENDIAN
    struct
      {
        u16 low;
        u16 high;
      } aword;
#else
    struct
      {
        u16 high;
        u16 low;
      } aword;
#endif
    u32 adword;
  } union_dword;

#ifdef __cplusplus
  }
#endif

#endif


