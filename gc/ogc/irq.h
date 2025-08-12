/*-------------------------------------------------------------

irq.h -- Interrupt subsystem

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


-------------------------------------------------------------*/


#ifndef __IRQ_H__
#define __IRQ_H__

/*! \file irq.h
\brief Interrupt subsystem

*/

#include "../gctypes.h"
#include "../tuxedo/ppc/intrinsics.h"
#include "../tuxedo/interrupt.h"

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*! \typedef void (*raw_irq_handler_t)(u32 irq,void *ctx)
\brief function pointer typedef for the interrupt handler callback
\param[in] irq interrupt number of triggered interrupt.
\param[in] ctx pointer to the user data.
*/
typedef void (*raw_irq_handler_t)(u32 irq,void *ctx);


/*! \fn raw_irq_handler_t IRQ_Request(u32 nIrq,raw_irq_handler_t pHndl,void *pCtx)
\brief Register an interrupt handler.
\param[in] nIrq interrupt number to which to register the handler
\param[in] pHndl pointer to the handler callback function which to call when interrupt has triggered
\param[in] pCtx pointer to user data to pass with, when handler is called

\return Old interrupt handler, else NULL
*/
MK_INLINE raw_irq_handler_t IRQ_Request(u32 nIrq,raw_irq_handler_t pHndl,void *pCtx)
{
   return (raw_irq_handler_t)KIrqSet((KIrqId)nIrq, (KIrqHandlerFn)pHndl, pCtx);
}


/*! \fn raw_irq_handler_t IRQ_Free(u32 nIrq)
\brief Free an interrupt handler.
\param[in] nIrq interrupt number for which to free the handler

\return Old interrupt handler, else NULL
*/
MK_INLINE raw_irq_handler_t IRQ_Free(u32 nIrq)
{
   return (raw_irq_handler_t)KIrqSet((KIrqId)nIrq, NULL, NULL);
}


/*! \fn raw_irq_handler_t IRQ_GetHandler(u32 nIrq)
\brief Get the handler from interrupt number
\param[in] nIrq interrupt number for which to retrieve the handler

\return interrupt handler, else NULL
*/
MK_INLINE raw_irq_handler_t IRQ_GetHandler(u32 nIrq)
{
   return (raw_irq_handler_t)KIrqGet((KIrqId)nIrq);
}


/*! \fn u32 IRQ_Disable()
\brief Disable the complete IRQ subsystem. No interrupts will be served. Multithreading kernel fully disabled.

\return Old state of the IRQ subsystem
*/
MK_INLINE u32 IRQ_Disable(void)
{
   return PPCIrqLockByMsr();
}


/*! \fn u32 IRQ_Restore(u32 level)
\brief Restore the IRQ subsystem with the given level. This is function should be used together with IRQ_Disable()
\param[in] level IRQ level to restore to.

\return none
*/
MK_INLINE void IRQ_Restore(u32 level)
{
   PPCIrqUnlockByMsr(level);
}

MK_INLINE void __MaskIrq(u32 nMask)
{
   KIrqDisable(nMask);
}

MK_INLINE void __UnmaskIrq(u32 nMask)
{
   KIrqEnable(nMask);
}

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
