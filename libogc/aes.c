/*-------------------------------------------------------------

aes.c -- AES Engine

Copyright (C) 2022
GaryOderNichts 
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
#if defined(HW_RVL)

#include <string.h>
#include "gctypes.h"
#include "gcutil.h"
#include "ipc.h"
#include "aes.h"

#define AES_HEAPSIZE 0x200

#define AES_IOCTLV_ENCRYPT 2
#define AES_IOCTLV_DECRYPT 3

static s32 __aes_fd = -1;
static s32 __aes_hid = -1;

static s32 AES_ExecuteCommand(s32 command, const void* key, u32 key_size, void* iv, u32 iv_size, const void* in_data, void* out_data, u32 data_size)
{
	if ((((u32)in_data | (u32)out_data) & 0xF) != 0)
		return -4;

	if (key_size != 16 || iv_size != 16 || (data_size & 15) != 0)
		return -4;

	ioctlv* params = (ioctlv*)iosAlloc(__aes_hid, sizeof(ioctlv) * 4);
	if (!params)
		return -1;

	s32 ret = -1;
	for (u32 i = 0; i < data_size; i += AES_BLOCK_SIZE) {
		u32 size = i+AES_BLOCK_SIZE >= data_size
			? data_size - i
			: AES_BLOCK_SIZE;

		params[0].data	= (void*)((u32)in_data + i);
		params[0].len	= size;
		params[1].data	= (void*) key;
		params[1].len	= key_size;
		params[2].data	= (void*)((u32)out_data + i);
		params[2].len	= size;
		params[3].data	= iv;
		params[3].len	= iv_size;

		ret = IOS_Ioctlv(__aes_fd, command, 2, 2, params);
		if (ret < 0)
			break;
	}

	iosFree(__aes_hid, params);
	return ret;
}

s32 AES_Init(void)
{
	if (__aes_fd >= 0)
		return -1;

	__aes_fd = IOS_Open("/dev/aes", 0);
	if (__aes_fd < 0)
		return __aes_fd;

	//only create heap if it wasn't created yet.
	//its never disposed, so only create once.
	if(__aes_hid < 0)
		__aes_hid = iosCreateHeap(AES_HEAPSIZE);

	if (__aes_hid < 0) {
		AES_Close();
		return __aes_hid;
	}

	return 0;
}

s32 AES_Close(void)
{
	if (__aes_fd < 0)
		return -1;

	IOS_Close(__aes_fd);
	__aes_fd = -1;

	return 0;
}

s32 AES_Encrypt(const void* key, u32 key_size, void* iv, u32 iv_size, const void* in_data, void* out_data, u32 data_size)
{
	return AES_ExecuteCommand(AES_IOCTLV_ENCRYPT, key, key_size, iv, iv_size, in_data, out_data, data_size);
}

s32 AES_Decrypt(const void* key, u32 key_size, void* iv, u32 iv_size, const void* in_data, void* out_data, u32 data_size)
{
	return AES_ExecuteCommand(AES_IOCTLV_DECRYPT, key, key_size, iv, iv_size, in_data, out_data, data_size);
}

#endif
