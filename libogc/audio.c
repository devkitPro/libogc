/*-------------------------------------------------------------

audio.c -- Audio subsystem

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


#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "audio.h"
#include "lwp_watchdog.h"

#define STACKSIZE		16384

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_DSPDMA         0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

#if defined(HW_DOL)
	static vu32* const _aiReg = (u32*)0xCC006C00;
#elif defined(HW_RVL)
	static vu32* const _aiReg = (u32*)0xCD006C00;
#else
	#error HW model not supported.
#endif

static vu16* const _dspReg = (u16*)0xCC005000;

static u32 __AIInitFlag = 0;
static u8 *__CallbackStack = NULL;
static u8 *__OldStack = NULL;

static u64 bound_32KHz,bound_48KHz,min_wait,max_wait,buffer;

static AIDCallback __AID_Callback;
static AISCallback __AIS_Callback;

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

static void __AICallbackStackSwitch(AIDCallback handler)
{
	__asm__ __volatile__("mflr	%r0\n\
						  stw	%r0,4(%r1)\n\
						  stwu  %r1,-24(%r1)\n\
						  stw	%r31,20(%r1)\n\
						  mr	%r31,%r3\n\
						  lis	%r5,__OldStack@ha\n\
						  addi	%r5,%r5,__OldStack@l\n\
						  stw	%r1,0(%r5)\n\
						  lis	%r5,__CallbackStack@ha\n\
						  addi	%r5,%r5,__CallbackStack@l\n\
						  lwz	%r1,0(%r5)\n\
						  subi	%r1,%r1,8\n\
						  mtlr	%r31\n\
						  blrl\n\
						  lis	%r5,__OldStack@ha\n\
						  addi	%r5,%r5,__OldStack@l\n\
						  lwz	%r1,0(%r5)\n\
						  lwz	%r0,28(%r1)\n\
						  lwz	%r31,20(%r1)\n\
						  addi	%r1,%r1,24\n\
						  mtlr	%r0\n"
						 );
}

static void __AISHandler(u32 nIrq,void *pCtx)
{
	if(__AIS_Callback)
		__AIS_Callback(_aiReg[2]);
	_aiReg[0] |= 0x0008;
}

static void __AIDHandler(u32 nIrq,void *pCtx)
{
	if(__AID_Callback) {
		if(__CallbackStack)
			__AICallbackStackSwitch(__AID_Callback);
		else
			__AID_Callback();
	}
	_dspReg[5] = (_dspReg[5]&~(DSPCR_DSPINT|DSPCR_ARINT))|DSPCR_AIINT;
}

static void __AISRCINIT()
{
}

static void __AISetStreamSampleRate(u32 rate)
{
	u32 currrate,level;
	u32 playstate,volright,volleft,dsprate;

	currrate = AUDIO_GetStreamSampleRate();
	if(currrate!=rate) {
		playstate = AUDIO_GetStreamPlayState();
		volleft = AUDIO_GetStreamVolLeft();
		volright = AUDIO_GetStreamVolRight();
		AUDIO_SetStreamVolLeft(0);
		AUDIO_SetStreamVolRight(0);
		dsprate = _aiReg[0]&0x40;
		_aiReg[0] = _aiReg[0]&~0x40;

		_CPU_ISR_Disable(level);
		__AISRCINIT();
		_aiReg[0] |= dsprate;
		_aiReg[0] = (_aiReg[0]&~0x20)|0x20;
		_aiReg[0] = (_aiReg[0]&~0x02)|(_SHIFTL(rate,1,1));
		_CPU_ISR_Restore(level);

		AUDIO_SetStreamPlayState(playstate);
		AUDIO_SetStreamVolLeft(volleft);
		AUDIO_SetStreamVolRight(volright);
	}
}

AISCallback AUDIO_RegisterStreamCallback(AISCallback callback)
{
	u32 level;

	AISCallback old = __AIS_Callback;
	_CPU_ISR_Disable(level);
	__AIS_Callback = callback;
	_CPU_ISR_Restore(level);
	return old;
}

void AUDIO_Init(u8 *stack)
{
	u32 busfreq = TB_BUS_CLOCK;

	if(!__AIInitFlag) {
		bound_32KHz = (u64)((((busfreq>>2)/125000)*31524)/8000)<<32;
		bound_48KHz = (u64)((((busfreq>>2)/125000)*42024)/8000)<<32;
		min_wait = (u64)((((busfreq>>2)/125000)*42000)/8000)<<32;
		max_wait = (u64)((((busfreq>>2)/125000)*63000)/8000)<<32;
		buffer = (u64)((((busfreq>>2)/125000)*3000)/8000)<<32;

		_aiReg[0] = (_aiReg[0]&~0x20)|0x20;
		_aiReg[1] = _aiReg[1]&~0x000000ff;
		_aiReg[1] = _aiReg[1]&~0x0000ff00;
		_aiReg[3] = 0;

		__AISetStreamSampleRate(AI_SAMPLERATE_48KHZ);
		AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);

		__AIS_Callback = NULL;
		__AID_Callback = NULL;

		__OldStack = NULL;	// davem - use it or lose it
							// looks like 3.4 isn't picking up the use from the asm below
		__CallbackStack = stack;

		IRQ_Request(IRQ_AI,__AISHandler,NULL);
		__UnmaskIrq(IRQMASK(IRQ_AI));

		IRQ_Request(IRQ_DSP_AI,__AIDHandler,NULL);
		__UnmaskIrq(IRQMASK(IRQ_DSP_AI));

		__AIInitFlag = 1;
	}
}

void AUDIO_SetStreamVolLeft(u8 vol)
{
	_aiReg[1] = (_aiReg[1]&~0x000000ff)|(vol&0xff);
}

u8 AUDIO_GetStreamVolLeft()
{
	return (u8)(_aiReg[1]&0xff);
}

void AUDIO_SetStreamVolRight(u8 vol)
{
	_aiReg[1] = (_aiReg[1]&~0x0000ff00)|(_SHIFTL(vol,8,8));
}

u8 AUDIO_GetStreamVolRight()
{
	return (u8)(_SHIFTR(_aiReg[1],8,8));
}

void AUDIO_SetStreamSampleRate(u32 rate)
{
	_aiReg[0] = (_aiReg[0]&~0x0002)|(_SHIFTL(rate,1,1));
}

u32 AUDIO_GetStreamSampleRate()
{
	return _SHIFTR(_aiReg[0],1,1);
}

AIDCallback AUDIO_RegisterDMACallback(AIDCallback callback)
{
	u32 level;
	AIDCallback old;
	
	_CPU_ISR_Disable(level);
	old = __AID_Callback;
	__AID_Callback = callback;
	_CPU_ISR_Restore(level);
	return old;
}

void AUDIO_InitDMA(u32 startaddr,u32 len)
{
	u32 level;

	_CPU_ISR_Disable(level);
	_dspReg[24] = (_dspReg[24]&~0x03ff)|(_SHIFTR(startaddr,16,10));
	_dspReg[25] = (_dspReg[25]&~0xffe0)|(startaddr&0xffff);
	_dspReg[27] = (_dspReg[27]&~0x7fff)|(_SHIFTR(len,5,15));
	_CPU_ISR_Restore(level);
}

u16 AUDIO_GetDMAEnableFlag()
{
	return (_SHIFTR(_dspReg[27],15,1));
}

void AUDIO_StartDMA()
{
	_dspReg[27] = (_dspReg[27]&~0x8000)|0x8000;
}

void AUDIO_StopDMA()
{
	_dspReg[27] = (_dspReg[27]&~0x8000);
}

u32 AUDIO_GetDMABytesLeft()
{
	return (_SHIFTL(_dspReg[29],5,15));
}

u32 AUDIO_GetDMAStartAddr()
{
	return (_SHIFTL((_dspReg[24]&0x3ff),16,10)|(_dspReg[25]&0xffe0));
}

u32 AUDIO_GetDMALength()
{
	return ((_dspReg[26]&0x7fff)<<5);
}

void AUDIO_SetStreamTrigger(u32 cnt)
{
	_aiReg[3] = cnt;
}

void AUDIO_ResetStreamSampleCnt()
{
	_aiReg[0] = (_aiReg[0]&~0x20)|0x20;
}

void AUDIO_SetDSPSampleRate(u8 rate)
{
	u32 currate,level;
	u32 playstate,volright,volleft;

	if(AUDIO_GetDSPSampleRate()!=rate) {
		_aiReg[0] = (_aiReg[0]&~0x40);
		if(rate==AI_SAMPLERATE_32KHZ) {
			volright = AUDIO_GetStreamVolRight();
			AUDIO_SetStreamVolRight(0);
			volleft = AUDIO_GetStreamVolLeft();
			AUDIO_SetStreamVolLeft(0);
			playstate = _aiReg[0]&0x01;
			currate = AUDIO_GetStreamSampleRate();

			_CPU_ISR_Disable(level);
			__AISRCINIT();
			_aiReg[0] = (_aiReg[0]&~0x20)|0x20;
			_aiReg[0] = (_aiReg[0]&~0x02)|(_SHIFTL(currate,1,1));
			_aiReg[0] = (_aiReg[0]&~0x01)|(playstate&0x01);
			_aiReg[0] |= 0x40;
			_CPU_ISR_Restore(level);
			AUDIO_SetStreamVolRight(volright);
			AUDIO_SetStreamVolLeft(volleft);
		}
	}
}

u32 AUDIO_GetDSPSampleRate()
{
	return (_SHIFTR(_aiReg[0],6,1))^1;		//0^1(1) = 48Khz, 1^1(0) = 32Khz
}

void AUDIO_SetStreamPlayState(u32 state)
{
	u32 playstate,streamrate;
	u32 volright,volleft,level;

	playstate = AUDIO_GetStreamPlayState();
	streamrate = AUDIO_GetStreamSampleRate();
	if(playstate!=state && state==AI_STREAM_START && streamrate==AI_SAMPLERATE_32KHZ ) {
		volright = AUDIO_GetStreamVolRight();
		AUDIO_SetStreamVolRight(0);
		volleft = AUDIO_GetStreamVolLeft();
		AUDIO_SetStreamVolLeft(0);

		_CPU_ISR_Disable(level);
		__AISRCINIT();
		_aiReg[0] = (_aiReg[0]&~0x20)|0x20;
		_aiReg[0] = (_aiReg[0]&~0x01)|0x01;
		_CPU_ISR_Restore(level);
		AUDIO_SetStreamVolRight(volright);
		AUDIO_SetStreamVolLeft(volleft);
	} else {
		_aiReg[0] = (_aiReg[0]&~0x01)|(state&0x01);
	}
}

u32 AUDIO_GetStreamPlayState()
{
	return (_aiReg[0]&0x01);
}
