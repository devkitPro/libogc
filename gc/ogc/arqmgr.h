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
