/*-------------------------------------------------------------

gccore.h -- GC core header

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


#ifndef __GCCORE_H__
#define __GCCORE_H__

/*! \file gccore.h 
\brief Core header which includes all subsequent subsystem headers

*/ 

#include "ogc/aram.h"
#include "ogc/arqueue.h"
#include "ogc/arqmgr.h"
#include "ogc/audio.h"
#include "ogc/cache.h"
#include "ogc/card.h"
#include "ogc/cast.h"
#include "ogc/color.h"
#include "ogc/dvd.h"
#include "ogc/exi.h"
#include "ogc/gu.h"
#include "ogc/gx.h"
#include "ogc/si.h"
#include "ogc/gx_struct.h"
#include "ogc/irq.h"
#include "ogc/lwp.h"
#include "ogc/message.h"
#include "ogc/semaphore.h"
#include "ogc/pad.h"
#include "ogc/system.h"
#include "ogc/video.h"
#include "ogc/video_types.h"

/*
 * Error returns
 */
#define RNC_FILE_IS_NOT_RNC				-1
#define RNC_HUF_DECODE_ERROR			-2
#define RNC_FILE_SIZE_MISMATCH			-3
#define RNC_PACKED_CRC_ERROR			-4
#define RNC_UNPACKED_CRC_ERROR			-5

#ifndef ATTRIBUTE_ALIGN
# define ATTRIBUTE_ALIGN(v)				__attribute__((aligned(v)))
#endif
#ifndef ATTRIBUTE_PACKED
# define ATTRIBUTE_PACKED				__attribute__((packed))
#endif

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

	   
/*!
 * \mainpage
 *
 * - \subpage intro
 * - \subpage api_doc
 */
	   

/*!
 * \page intro Introduction
 * Welcome to the libOGC reference documentation.
 */

/*!
 * \page api_doc Detailed API description
 *
 * - \ref aram.h "ARAM subsystem"<br>
 * - \ref audio.h "AUDIO subsystem"<br>
 * - \ref exi.h "EXI subsystem"<br>
 * - \ref irq.h "IRQ subsystem"<br>
 * - \ref dsp.h "DSP subsystem"<br>
 * - \ref card.h "Memory card subsystem"<br>
 * - \ref cache.h "Cache subsystem"<br>
 * - \ref video.h "VIDEO subsystem"<br>
 * - \ref system.h "OS functions and initialization"<br>
 * - \ref lwp.h "Thread subsystem I"<br>
 * - \ref message.h "Thread subsystem II"<br>
 * - \ref mutex.h "Thread subsystem III"<br>
 * - \ref semaphore.h "Thread subsystem IV"<br>
 * - \ref cond.h "Thread subsystem V"<br>
 */
	   
s32 depackrnc1_ulen(void *packed);
s32 depackrnc1(void *packed,void *unpacked);

void depackrnc2(void *packed,void *unpacked);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
