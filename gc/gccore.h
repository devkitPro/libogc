#ifndef __GCCORE_H__
#define __GCCORE_H__

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
#ifndef ATTRIBUTE_PAKED
# define ATTRIBUTE_PAKED				__attribute__((packed))
#endif

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s32 depackrnc1_ulen(void *packed);
s32 depackrnc1(void *packed,void *unpacked);

void depackrnc2(void *packed,void *unpacked);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
