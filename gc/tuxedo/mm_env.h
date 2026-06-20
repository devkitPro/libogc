// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "mm.h"

#define MM_ENV_LOW_MEM           0x80000000

#define MM_ENV_PPC_EXCPT         0x80000100

#define MM_ENV_HB_BOOTSTUB       0x80001800
#define MM_ENV_HB_BOOTSTUB_SZ    0x1800

#define MM_ENV_LOW_MEM2          0x80003000

#if defined(__wii__)
#define MM_ENV_RVL_LOW_MEM       0x80003100
#endif

#if defined(__gamecube__)
#define MM_ENV_APP_AREA          0x80003100
#elif defined(__wii__)
#define MM_ENV_APP_AREA          0x80003f00
#endif
#define MM_ENV_APP_AREA_SZ       (MM_ENV_LOADER_AREA-MM_ENV_APP_AREA)

#define MM_ENV_APPLOADER_AREA    0x81200000
#define MM_ENV_APPLOADER_AREA_SZ 0x100000

#if defined(__gamecube__)
#define MM_ENV_LOADER_AREA       0x81300000
#elif defined(__wii__)
#define MM_ENV_LOADER_AREA       0x81330000
#endif
#define MM_ENV_LOADER_AREA_SZ    (MM_ENV_FST_AREA-MM_ENV_LOADER_AREA)

#define MM_ENV_FST_AREA          0x81700000
#define MM_ENV_FST_AREA_SZ       0x100000
