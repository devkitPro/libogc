/*-------------------------------------------------------------

$Id: arqmgr.h,v 1.9 2006-04-10 05:35:05 shagkur Exp $

arqmgr.h -- ARAM task queue management

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
Revision 1.8  2005/12/09 09:20:49  shagkur
no message

Revision 1.7  2005/11/22 14:00:37  shagkur
- small typo fix

Revision 1.6  2005/11/22 13:58:25  shagkur
- added copyright header (taken from libnds)
- introduced CVS ID and LOG token
- started doxygen styled documenation


-------------------------------------------------------------*/


#ifndef __ARQMGR_H__
#define __ARQMGR_H__


/*!
 * \file arqmgr.h 
 * \brief ARAM queue managemnt subsystem
 *
 */ 


#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*!
 * \typedef void (*ARQMCallback)()
 * \brief function pointer typedef for the user's callback when ARAM operation has completed
 * \param none
 */
typedef void (*ARQMCallback)(s32 result);


/*!
 * \fn void ARQM_Init(u32 arambase,s32 len)
 * \brief Initialize the ARAM queue management system
 *
 * \param[in] arambase ARAM startaddress to take for the queue stack
 * \param[in] len maximum amount of memory to be reserved from the ARAM for the queue management
 *
 * \return none
 */
void ARQM_Init(u32 arambase,s32 len);


/*!
 * \fn u32 ARQM_PushData(void *buff,s32 len)
 * \brief Push the data onto the ARAM queue
 *
 * \param[in] buff startaddress of buffer to be pushed onto the queue. <b><i>NOTE:</i></b> Must be 32-bytealigned.
 * \param[in] len length of data to be pushed onto the queue.
 *
 * \return none
 */
u32 ARQM_PushData(void *buffer,s32 len);


/*!
 * \fn u32 ARQM_GetZeroBuffer()
 * \brief Returns ARAM address of 'zero buffer'
 *
 * \return See description
 */
u32 ARQM_GetZeroBuffer();


/*!
 * \fn u32 ARQM_GetStackPointer()
 * \brief Return the ARAM address of the next free stack pointer
 *
 * \return See description
 */
u32 ARQM_GetStackPointer();


/*!
 * \fn u32 ARQM_GetFreeSize()
 * \brief Return Returns remaining number of bytes on stack
 *
 * \return See description
 */
u32 ARQM_GetFreeSize();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
