/*-------------------------------------------------------------

$Id: arqmgr.h,v 1.3 2005-11-21 12:41:27 shagkur Exp $

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

-------------------------------------------------------------*/


#ifndef __ARQMGR_H__
#define __ARQMGR_H__

#include <gctypes.h>

#define ARQM_STACKENTRIES		16
#define ARQM_ZEROBYTES			256

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*ARQMCallback)();

void ARQM_Init(u32 arambase,u32 len);
u32 ARQM_PushData(void *buff,u32 len,ARQMCallback tccb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
