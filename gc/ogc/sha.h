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

#define SHA_MSGBLOCK_SIZE 0x10000

typedef struct
{
	u32 states[5];
	u32 upper_length;
	u32 lower_length;
} sha_context;

/*! 
 * \fn s32 SHA_Init(void)
 * \brief Initializes the SHA1 subsystem. This call could be done in the early stages of your main()
 *
 * \return 0 or higher on success, otherwise the returned error code
 */
s32 SHA_Init(void);

/*! 
 * \fn s32 SHA_Close(void)
 * \brief Closes the SHA1 subsystem handlers. This call could be done when exiting your application or before reloading IOS
 *
 * \return 0 or higher on success, otherwise the returned error code
 */
s32 SHA_Close(void);

/*! 
 * \fn s32 SHA_Calculate(const void* data, const u32 data_size, void* message_digest)
 * \brief Calculates the SHA1 hash of the given data, and puts it in message_digest
 *
 * \param[in] data pointer to the data to hash. if it is not 64-byte aligned an internal buffer will be used
 * \param[in] data_size size of the data to hash
 * \param[out] message_digest pointer to where to write the hash to
 *
 * \return 0 or higher on success, otherwise the returned error code
 */
s32 SHA_Calculate(const void* data, const u32 data_size, void* message_digest);

/*! 
 * \fn s32 SHA_InitializeContext(sha_context* context)
 * \brief Initializes the given sha context
 *
 * \param[in] context pointer to the sha_context to initialize
 * 
 * \return 0 or higher on success, otherwise the returned error code
 */
s32 SHA_InitializeContext(const sha_context* context);

/*! 
 * \fn s32 SHA_Input(const sha_context* context, const void* data, const u32 data_size)
 * \brief Adds data to the given sha_context and hashes it
 *
 * \param[in] context pointer to the sha_context to use
 * \param[in] data pointer to the data to hash. if it is not 64-byte aligned an internal buffer will be used
 * \param[in] data_size size of the data to hash
 * 
 * \return 0 or higher on success, otherwise the returned error code
 */
s32 SHA_Input(const sha_context* context, const void* data, const u32 data_size);

/*! 
 * \fn s32 SHA_Finalize(const sha_context* context, const void* data, const u32 data_size, void* message_digest)
 * \brief Calculates the final SHA1 hash of the given context and last data, and puts it in message_digest
 *
 * \param[in] context pointer to the sha_context to use
 * \param[in] data pointer to the data to hash. if it is not 64-byte aligned an internal buffer will be used
 * \param[in] data_size size of the data to hash
 * \param[out] message_digest pointer to where to write the final SHA1 hash to
 *
 * \return 0 or higher on success, otherwise the returned error code
 */
s32 SHA_Finalize(const sha_context* context, const void* data, const u32 data_size, void* message_digest);

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif
#endif