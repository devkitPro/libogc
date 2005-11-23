/*-------------------------------------------------------------

$Id: aram.h,v 1.6 2005-11-23 07:50:15 shagkur Exp $

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
Revision 1.5  2005/11/22 14:05:42  shagkur
- added copyright header (taken from libnds)
- introduced CVS ID and LOG token
- started doxygen styled documentation

Revision 1.4  2005/11/21 10:49:01  shagkur
no message


-------------------------------------------------------------*/


#ifndef __ARAM_H__
#define __ARAM_H__

/*! 
 * \file aram.h 
 * \brief ARAM subsystem
 *
 */ 

#include <gctypes.h>


/*! 
 * \addtogroup dmamode ARAM DMA transfer direction
 * @{
 */

#define AR_MRAMTOARAM		0		/*!< direction: MRAM -> ARAM (write) */
#define AR_ARAMTOMRAM		1		/*!< direction: ARAM -> MRAM (read) */

/*!
 * @}
 */



/*!
 * \addtogroup memmode ARAM memory access modes
 * @{
 */

#define AR_ARAMINTALL		0		/*!< use all the internal ARAM memory */
#define AR_ARAMINTUSER		1		/*!< use only the internal user space of the ARAM memory */
#define AR_ARAMEXPANSION	2		/*!< use expansion ARAM memory, if present */

/*!
 * @}
 */


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*! 
 * \typedef void (*ARCallback)(void)
 * \brief function pointer typedef for the user's ARAM interrupt callback
 *
 * \param none
 */
typedef void (*ARCallback)(void);


/*! 
 * \fn ARCallback AR_RegisterCallback(ARCallback callback)
 * \brief Register the given function as a DMA callback
 *
 *        Any existing callback is replaced unconditionally
 *
 * \param[in] callback to be invoked upon completion of DMA transaction
 *
 * \return pointer to the previously registered callback and NULL respectively
 */
ARCallback AR_RegisterCallback(ARCallback callback);


/*! 
 * \fn u32 AR_GetDMAStatus()
 * \brief Get current status of DMA
 *
 * \return zero if DMA is idle, non-zero if a DMA is in progress
 */
u32 AR_GetDMAStatus();


/*! 
 * \fn u32 AR_Init(u32 *stack_idx_array,u32 num_entries)
 * \brief Initializes ARAM subsystem.
 *
 *        Following tasks are performed:<br>
 *      - Disables ARAM DMA
 *      - Sets DMA callback to NULL
 *      - Initializes ARAM controller
 *      - Determines size of ARAM memory
 *      - Initializes the ARAM stack based memory allocation system<br>
 *      <br>
 *        The parameter u32 *stack_idx_array points to an array of u32 integers. The parameter u32 num_entries specifies the number of entries in this array.<br>
 *        The user application is responsible for determining how many ARAM blocks the device driver can allocate.<br>
 *      <br>
 *        As an example, consider:
 * \code
 *        #define MAX_NUM_BLOCKS 10
 *
 *        u32 aram_blocks[MAX_NUM_BLOCKS];
 *        ...
 *        void func(void)
 *        {
 *           AR_Init(aram_blocks, MAX_NUM_BLOCKS);
 *        }
 * \endcode
 *
 *        Here, we are telling AR that the application will allocate, at most, 10 blocks (of arbitrary size), and that AR should store addresses for those blocks in the array aram_blocks. Note that the array is simply storage supplied by the application so that AR can track the number and size of memory blocks allocated by AR_Alloc().
 *        AR_Free()also uses this array to release blocks.<br>
 *        If you do not wish to use AR_Alloc() and AR_Free() and would rather manage ARAM usage within your application, then call AR_Init() like so:<br>
 *        <br>
 *               AR_Init(NULL, 0);<br>
 *        <br>
 *        The AR_Init() function also calculates the total size of the ARAM aggregate. Note that this procedure is <b><i>destructive</i></b> - i.e., any data stored in ARAM will be corrupted.<br>
 *        AR_Init()may be invoked multiple times. This function checks the state of an initialization flag; if asserted, this function will simply exit on subsequent calls. To perform another initialization of the ARAM driver, call AR_Reset() before invoking AR_Init()again.
 *
 * \param[in] stack_idx_array pointer to an array of u32 integer
 * \param[in] num_entries number of entries in the specified array
 *
 * \return base address of the "user" ARAM area. As of this writing, the operating system reserves the bottom 16 KB of ARAM. Therefore, AR_Init() returns 0x04000 to indicate the starting location of the ARAM user area.
 */
u32 AR_Init(u32 *stack_idx_array,u32 num_entries);


/*! 
 * \fn void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len)
 * \brief Initialize and start ARAM DMA transfer.
 *
 * \param[in] dir direction of \ref dmamode "ARAM DMA operation"
 * \param[in] memaddr main memory address to read/write from/to data.
 * \param[in] aramaddr ARAM memory address to write/read to/from data.
 * \param[in] len length of data to copy.
 *
 * \return none
 */
void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len);


/*! 
 * \fn u32 AR_Alloc(u32 len)
 * \brief Allocate a block from ARAM
 *
 * \param[in] len length of block to allocate.
 *
 * \return ARAM blockstartaddress
 */
u32 AR_Alloc(u32 len);


/*! 
 * \fn u32 AR_Free(u32 *len)
 * \brief Free a block from ARAM
 *
 * \param[out] len pointer to receive the length of the free'd ARAM block. This is optional and can be NULL.
 *
 * \return ARAM current baseaddress after free'ing the block
 */
u32 AR_Free(u32 *len);


/*!
 * \fn void AR_Clear(u32 flag)
 * \brief Clear ARAM memory
 *
 * \param[in] flag Mode flag for clear operation (ARAMINTALL, ARAMINTUSER, ARAMEXPANSION)
 *
 * \return none
 */
void AR_Clear(u32 flag);


/*! 
 * \fn BOOL AR_CheckInit()
 * \brief Check if ARAM subsystem is initialized.
 *
 * \return boolean value
 */
BOOL AR_CheckInit();


/*!
 * \fn void AR_Reset()
 * \brief Reset the ARAM subsystem initialization flag
 *
 * \return none
 */
void AR_Reset();


/*! 
 * \fn u32 AR_GetSize()
 * \brief Get the whole size, including expansion, of ARAM memory
 *
 * \return ARAM memory size
 */
u32 AR_GetSize();


/*! 
 * \fn u32 AR_GetBaseAddress()
 * \brief Get the baseaddress of ARAM memory
 *
 * \return ARAM memory baseaddress
 */
u32 AR_GetBaseAddress();


/*! 
 * \fn u32 AR_GetInternalSize()
 * \brief Get the size of the internal ARAM memory
 *
 * \return ARAM internal memory size
 */
u32 AR_GetInternalSize();


/*! 
 * \def AR_StartDMARead(maddr,araddr,tlen)
 * \brief Wraps the DMA read operation done by AR_StartDMA()
 */
#define AR_StartDMARead(maddr,araddr,tlen)	\
	AR_StartDMA(AR_ARAMTOMRAM,maddr,araddr,tlen);


/*!
 * \def AR_StartDMAWrite(maddr,araddr,tlen)
 * \brief Wraps the DMA write operation done by AR_StartDMA()
 */
#define AR_StartDMAWrite(maddr,araddr,tlen)	\
	AR_StartDMA(AR_MRAMTOARAM,maddr,araddr,tlen);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif //__ARAM_H__
