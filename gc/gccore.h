#ifndef __GCCORE_H__
#define __GCCORE_H__

#include "ogc/aram.h"
#include "ogc/arqueue.h"
#include "ogc/arqmgr.h"
#include "ogc/audio.h"
#include "ogc/cache.h"
#include "ogc/card.h"
#include "ogc/cast.h"
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

#define ATTRIBUTE_ALIGN(v)				__attribute__((aligned(v)))

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void depackrnc(void *src,void *dst);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
