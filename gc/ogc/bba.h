// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro

/*!
 * \file bba.h
 * \brief GameCube Broadband Adapter driver
 */

#pragma once

#ifdef __gamecube__

#include <gctypes.h>
#include <ogc/exi.h>

MK_EXTERN_C_START

typedef enum BbaLinkState {
	BbaLinkState_Down     = 0,
	BbaLinkState_Up_10HD  = 0x80,
	BbaLinkState_Up_10FD  = 0x40,
	BbaLinkState_Up_100HD = 0x20,
	BbaLinkState_Up_100FD = 0x10,
} BbaLinkState;

typedef struct BbaBuffer BbaBuffer;
typedef void (*BbaBufferCb)(BbaBuffer* buf, bool ok);
typedef bool (*BbaBeginRxCb)(BbaBuffer* buf, u32 size);

struct BbaBuffer {
	BbaBuffer* next;
	BbaBufferCb cb;
	void* user;
	void* mem;
	u32 size;
};

extern u8 g_bbaMac[6];

bool BBA_Init(void);

BbaLinkState BBA_GetLinkState(void);

void BBA_StartThread(void* stack_top, u16 prio);

void BBA_Poll(void);

void BBA_OnRxBegin(BbaBeginRxCb cb);

bool BBA_TxEnqueue(BbaBuffer* buf);

MK_EXTERN_C_END

#endif
