#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "context.h"

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void ExceptionSetHandler(u32,void (*)(frame_context*));

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
