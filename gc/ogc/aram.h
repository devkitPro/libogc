/*-------------------------------------------------------------

$Id: aram.h,v 1.4 2005-11-21 10:49:01 shagkur Exp $

aram.h -- ARAM subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

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

$Log: not supported by cvs2svn $

-------------------------------------------------------------*/


#ifndef __ARAM_H__
#define __ARAM_H__

/*! \file aram.h 
\brief ARAM subsystem

*/ 

#include <gctypes.h>


/*! \addtogroup dmamode ARAM DMA transfer direction
 * @{
 */

#define AR_MRAMTOARAM		0		/*!< direction: MRAM -> ARAM (write) */
#define AR_ARAMTOMRAM		1		/*!< direction: ARAM -> MRAM (read) */

/*! @} */


/*! \addtogroup memmode ARAM memory access modes
 * @{
 */

#define AR_ARAMINTALL		0		/*!< use all the internal ARAM memory */
#define AR_ARAMINTUSER		1		/*!< use only the internal user space of the ARAM memory */
#define AR_ARAMEXPANSION	2		/*!< use expansion ARAM memory, if present */

/*! @} */


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*! \typedef void (*ARCallback)(void)
\brief function pointer typedef for the user's ARAM interrupt callback
\param none
*/
typedef void (*ARCallback)(void);


/*! \fn ARCallback AR_RegisterCallback(ARCallback callback)
\brief Register a user callback function for the ARAM interrupt handler.
\param[in] callback pointer to callback to call when ARAM interrupt occured.

\return pointer to old callback function or NULL respectively.
*/
ARCallback AR_RegisterCallback(ARCallback callback);


/*! \fn u32 AR_GetDMAStatus()
\brief Get ARAM DMA status

\return DMA status
*/
u32 AR_GetDMAStatus();


/*! \fn u32 AR_Init(u32 *stack_idx_array,u32 num_entries)
\brief Initializes ARAM subsystem.
\param[in] stack_idx_array pointer to an array of indicies to keep track on allocated ARAM blocks.
\param[in] num_entries maximum count of entries the array can take.

\return ARAM baseaddress
*/
u32 AR_Init(u32 *stack_idx_array,u32 num_entries);


/*! \fn void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len)
\brief Initialize and start ARAM DMA transfer.
\param[in] dir direction of \ref dmamode "ARAM DMA operation"
\param[in] memaddr main memory address to read/write from/to data.
\param[in] aramaddr ARAM memory address to write/read to/from data.
\param[in] len length of data to copy.

\return none
*/
void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len);


/*! \fn u32 AR_Alloc(u32 len)
\brief Allocate a block from ARAM
\param[in] len length of block to allocate.

\return ARAM blockstartaddress
*/
u32 AR_Alloc(u32 len);


/*! \fn u32 AR_Free(u32 *len)
\brief Free a block from ARAM
\param[out] len pointer to receive the length of the free'd ARAM block. This is optional and can be NULL.

\return ARAM current baseaddress after free'ing the block
*/
u32 AR_Free(u32 *len);


/*! \fn void AR_Clear(u32 flag)
\brief Clear ARAM memory
\param[in] flag Mode flag for clear operation (ARAMINTALL, ARAMINTUSER, ARAMEXPANSION)

\return none
*/
void AR_Clear(u32 flag);


/*! \fn BOOL AR_CheckInit()
\brief Check if ARAM subsystem is initialized.

\return boolean value
*/
BOOL AR_CheckInit();


/*! \fn void AR_Reset()
\brief Reset the ARAM subsystem initialization flag

\return none
*/
void AR_Reset();


/*! \fn u32 AR_GetSize()
\brief Get the whole size, including expansion, of ARAM memory

\return ARAM memory size
*/
u32 AR_GetSize();


/*! \fn u32 AR_GetBaseAddress()
\brief Get the baseaddress of ARAM memory

\return ARAM memory baseaddress
*/
u32 AR_GetBaseAddress();


/*! \fn u32 AR_GetInternalSize()
\brief Get the size of the internal ARAM memory

\return ARAM internal memory size
*/
u32 AR_GetInternalSize();


/*! \def AR_StartDMARead(maddr,araddr,tlen)
\brief Wraps the DMA read operation done by AR_StartDMA()
*/
#define AR_StartDMARead(maddr,araddr,tlen)	\
	AR_StartDMA(AR_ARAMTOMRAM,maddr,araddr,tlen);


/*! \def AR_StartDMAWrite(maddr,araddr,tlen)
\brief Wraps the DMA write operation done by AR_StartDMA()
*/
#define AR_StartDMAWrite(maddr,araddr,tlen)	\
	AR_StartDMA(AR_MRAMTOARAM,maddr,araddr,tlen);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif //__ARAM_H__
