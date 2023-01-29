/*-------------------------------------------------------------

sha.c -- SHA Engine

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
#if defined(HW_RVL)

#include <string.h>
#include "gctypes.h"
#include "gcutil.h"
#include "ipc.h"
#include "sha.h"

static s32 __sha_fd = -1;
static s32 __sha_hid = -1;

#define SHA_HEAPSIZE 0x50
typedef enum 
{ 
	ResetContext = 0x00,
	AddData = 0x01,
	FinalizeHash = 0x02,
} ShaCommand;

static s32 SHA_ExecuteCommand(const ShaCommand command, sha_context* context, const void* in_data, const u32 data_size, void* out_data)
{
	ioctlv* params = (ioctlv*)iosAlloc(__sha_hid, sizeof(ioctlv) * 3);
	s32 ret = -1;
	
	if(params == NULL)
		return -4;
	
	for (u32 i = 0; i < data_size; i += SHA_BLOCK_SIZE) {
		u32 size = SHA_BLOCK_SIZE;
		ShaCommand block_cmd = command;
		
		//if it's the final block, set size correctly.
		//if it's not the final block, and we got a finalize, we will first send the add command
		if(i+SHA_BLOCK_SIZE >= data_size)
			size = data_size - i;
		else if(command == FinalizeHash)
			block_cmd = AddData;

		params[0].data	= (void*)((u32)in_data + i);
		params[0].len	= size;
		params[1].data	= (void*) context;
		params[1].len	= sizeof(sha_context);
		params[2].data	= (void*)((u32)out_data);
		params[2].len	= 0x14;

		ret = IOS_Ioctlv(__sha_fd, block_cmd, 1, 2, params);
		if (ret < 0)
			break;
	}

	iosFree(__sha_hid, params);
	return ret;
}

s32 SHA_Init(void)
{
	if (__sha_fd >= 0)
		return -1;

	__sha_fd = IOS_Open("/dev/sha", 0);
	if (__sha_fd < 0)
		return __sha_fd;

	//only create heap if it wasn't created yet. 
	//its never disposed, so only create once.
	if(__sha_hid < 0)
		__sha_hid = iosCreateHeap(SHA_HEAPSIZE);
	
	if (__sha_hid < 0) {
		SHA_Close();
		return __sha_hid;
	}
	
	return 0;
}

s32 SHA_Close(void)
{
	if (__sha_fd < 0)
		return -1;

	IOS_Close(__sha_fd);
	__sha_fd = -1;

	return 0;
}

s32 SHA_InitializeContext(sha_context* context)
{
	if(context == NULL)
		return -1;
	
	if(((u32)context) & 0x1F)
		return -4;
	
	ioctlv* params = (ioctlv*)iosAlloc(__sha_hid, sizeof(ioctlv) * 4);
	if(params == NULL)
		return -4;
	
	memset(params, 0, sizeof(ioctlv) * 4);
	params[1].data	= (void*) context;
	params[1].len	= sizeof(sha_context);
	s32 ret = IOS_Ioctlv(__sha_fd, ResetContext, 1, 2, params);
	
	iosFree(__sha_hid, params);
	return ret;
}
s32 SHA_Calculate(sha_context* context, const void* data, const u32 data_size, void* message_digest)
{
	if(context == NULL || message_digest == NULL || data_size == 0 || data == NULL)
		return -1;
	
	if(((u32)context & 0x1F) != 0 || ((u32)message_digest & 0x1F) != 0)
		return -4;
	
	if( data != NULL && ((u32)data & 0x3F) != 0)
		return -4;
	
	return SHA_ExecuteCommand(FinalizeHash, context, data, data_size, message_digest);
}

s32 SHA_Input(sha_context* context, const void* data, const u32 data_size)
{
	if(context == NULL || data == NULL || data_size == 0)
		return -1;
	
	if((((u32)context) & 0x1F) || (((u32)data) & 0x3F) || (data_size & ~(SHA_BLOCK_SIZE-1)) != 0)
		return -4;
	
	return SHA_ExecuteCommand(AddData, context, data, data_size, NULL);
}

#endif