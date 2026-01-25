// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

#define MM_P_MEM1   0x00000000
#define MM_P_EFB    0x08000000
#define MM_P_IO_CP  0x0c000000
#define MM_P_IO_PE  0x0c001000
#define MM_P_IO_VI  0x0c002000
#define MM_P_IO_PI  0x0c003000
#define MM_P_IO_MI  0x0c004000
#define MM_P_IO_DSP 0x0c005000
#define MM_P_IO_DI  (MM_P_IO_PPC|0x6000)
#define MM_P_IO_SI  (MM_P_IO_PPC|0x6400)
#define MM_P_IO_EXI (MM_P_IO_PPC|0x6800)
#define MM_P_IO_AI  (MM_P_IO_PPC|0x6c00)
#define MM_P_WGPIPE 0x0c008000

#ifdef __wii__
#define MM_P_IO_PPC 0x0d000000
#define MM_P_MEM2   0x10000000
#else
#define MM_P_IO_PPC 0x0c000000
#endif

#define MM_V_CACHED   0x80000000
#define MM_V_UNCACHED 0xc0000000

#define MM_CACHED(_n)   ((_n)|MM_V_CACHED)
#define MM_UNCACHED(_n) ((_n)|MM_V_UNCACHED)

#ifndef __ASSEMBLER__
#include "types.h"

#define HWREG_CP  ((vu16*)MM_UNCACHED(MM_P_IO_CP))
#define HWREG_PE  ((vu16*)MM_UNCACHED(MM_P_IO_PE))
#define HWREG_VI  ((vu16*)MM_UNCACHED(MM_P_IO_VI))
#define HWREG_PI  ((vu32*)MM_UNCACHED(MM_P_IO_PI))
#define HWREG_MI  ((vu16*)MM_UNCACHED(MM_P_IO_MI))
#define HWREG_DSP ((vu16*)MM_UNCACHED(MM_P_IO_DSP))
#define HWREG_DI  ((vu32*)MM_UNCACHED(MM_P_IO_DI))
#define HWREG_SI  ((vu32*)MM_UNCACHED(MM_P_IO_SI))
#define HWREG_EXI ((vu32*)MM_UNCACHED(MM_P_IO_EXI))
#define HWREG_AI  ((vu32*)MM_UNCACHED(MM_P_IO_AI))

#endif
