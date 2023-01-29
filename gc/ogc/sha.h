/*-------------------------------------------------------------

sha.h -- SHA Engine

Copyright (C) 2023
Joris 'DacoTaco' Vermeylen info@dacotaco.com

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/


#ifndef __SHA_H___
#define __SHA_H___

#if defined(HW_RVL)
#include <gctypes.h>
#include <gcutil.h>

#ifdef __cplusplus
	extern "C" {
#endif /* __cplusplus */

#define SHA_BLOCK_SIZE 0x10000

typedef struct
{
	u32 states[5];
	u32 upper_length;
	u32 lower_length;
} sha_context;

s32 SHA_Init(void);
s32 SHA_Close(void);
s32 SHA_InitializeContext(sha_context* context);

//calculate hash or add data manually - input data should *always* be 64bit aligned!
s32 SHA_Calculate(sha_context* context, const void* data, const u32 data_size, void* message_digest);
s32 SHA_Input(sha_context* context, const void* data, const u32 data_size);


#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif
#endif