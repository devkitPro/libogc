#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <irq.h>
#include <video.h>
#include "asm.h"
#include "processor.h"
#include "si.h"
#include "pad.h"

//#define _PAD_DEBUG

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

#define PAD_ENABLEDMASK(chan)		(0x80000000>>chan);

typedef void (*SPECCallback)(u32,u32*,PADStatus*);

static SPECCallback __pad_makestatus = NULL;
static u32 __pad_initialized = 0;
static u32 __pad_enabledbits = 0;
static u32 __pad_resettingbits = 0;
static u32 __pad_recalibratebits = 0;
static u32 __pad_waitingbits = 0;
static u32 __pad_pendingbits = 0;
static u32 __pad_checkingbits = 0;
static u32 __pad_resettingchan = 32;
static u32 __pad_spec = 5;

static u32 __pad_analogmode = 0x00000300;
static u32 __pad_cmdreadorigin = 0x41000000;
static u32 __pad_cmdcalibrate = 0x42000000;
static u32 __pad_xpatchbits = 0xf0000000;

static u32 __pad_type[PAD_CHANMAX];
static s8 __pad_origin[PAD_CHANMAX][12];
static u32 __pad_cmdprobedevice[PAD_CHANMAX];

static vu32* const _siReg = (u32*)0xCC006400;
static vu16* const _viReg = (u16*)0xCC002000;

extern u32 __PADFixBits;

static void __pad_enable(u32 chan);
static void __pad_disable(u32 chan);
static void __pad_doreset();

extern void udelay(int);
extern void __UnmaskIrq(u32);

static void SPEC0_MakeStatus(u32 chan,u32 *data,PADStatus *status)
{
	status->button = 0;

	if(data[0]&0x00080000) status->button |= 0x0100;
	if(data[0]&0x00200000) status->button |= 0x0200;
	if(data[0]&0x01000000) status->button |= 0x0400;
	if(data[0]&0x00010000) status->button |= 0x0800;
	if(data[0]&0x00100000) status->button |= 0x1000;
	
	status->stickX = (s8)(data[1]>>16);
	status->stickY = (s8)(data[1]>>24);
	status->substickX = (s8)data[1];
	status->substickY = (s8)(data[1]>>8);
	status->triggerL = (u8)_SHIFTR(data[0],8,8);
	status->triggerR = (u8)(data[0]&0xff);
	status->analogA = 0;
	status->analogB = 0;
	
	if(status->triggerL>=0xaa) status->button |= 0x40;
	if(status->triggerR>=0xaa) status->button |= 0x20;

	status->stickX -= 128;
	status->stickY -= 128;
	status->substickX -= 128;
	status->substickY -= 128;
}

static void SPEC1_MakeStatus(u32 chan,u32 *data,PADStatus *status)
{
	status->button = 0;

	if(data[0]&0x00800000) status->button |= 0x0100;
	if(data[0]&0x01000000) status->button |= 0x0200;
	if(data[0]&0x00200000) status->button |= 0x0400;
	if(data[0]&0x00100000) status->button |= 0x0800;
	if(data[0]&0x02000000) status->button |= 0x1000;
	
	status->stickX = (s8)(data[1]>>16);
	status->stickY = (s8)(data[1]>>24);
	status->substickX = (s8)data[1];
	status->substickY = (s8)(data[1]>>8);
	status->triggerL = (u8)_SHIFTR(data[0],8,8);
	status->triggerR = (u8)data[0]&0xff;
	status->analogA = 0;
	status->analogB = 0;
	
	if(status->triggerL>=0xaa) status->button |= 0x40;
	if(status->triggerR>=0xaa) status->button |= 0x20;

	status->stickX -= 128;
	status->stickY -= 128;
	status->substickX -= 128;
	status->substickY -= 128;
}

static s8 __pad_clampS8(s8 var,s8 org)
{
	s32 siorg;
#ifdef _PAD_DEBUG
	u32 tmp = var;
	printf("__pad_clampS8(%d,%d)\n",var,org);
#endif
	siorg = (s32)org;
	if(siorg>0) {
		siorg -= 128;
		if((s32)var<siorg) var = siorg;
	} else if(siorg<0) {
		siorg += 127;
		if(siorg<(s32)var) var = siorg;
	}
#ifdef _PAD_DEBUG
	printf("__pad_clampS8(%d,%d,%d,%d)\n",tmp,var,org,(var-org));
#endif
	return (var-org);
}

static u8 __pad_clampU8(u8 var,u8 org)
{
	if(var<org) var = org;
	return (var-org);
}

static void SPEC2_MakeStatus(u32 chan,u32 *data,PADStatus *status)
{
	u32 mode;
	
	status->button = _SHIFTR(data[0],16,14);
	
	status->stickX = (s8)(data[0]>>8);
	status->stickY = (s8)data[0];
#ifdef _PAD_DEBUG
//	printf("SPEC2_MakeStatus(%d,%p,%p)",chan,data,status);
#endif
	mode = __pad_analogmode&0x0700;
	if(mode==0x100) {
		status->substickX = (s8)((data[1]>>24)&0xf0);
		status->substickY = (s8)((data[1]>>8)&0xff);
		status->triggerL = (u8)((data[1]>>16)&0xff);
		status->triggerR = (u8)((data[1]>>8)&0xff);
		status->analogA = (u8)(data[1]&0xf0);
		status->analogB = (u8)((data[1]<<4)&0xf0);
	} else if(mode==0x200) {
		status->substickX = (s8)((data[1]>>24)&0xf0);
		status->substickY = (s8)((data[1]>>20)&0xf0);
		status->triggerL = (u8)((data[1]>>16)&0xf0);
		status->triggerR = (u8)((data[1]>>12)&0xf0);
		status->analogA = (u8)((data[1]>>8)&0xff);
		status->analogB = (s8)data[1]&0xff;
	} else if(mode==0x300) {
		status->substickX = (s8)((data[1]>>24)&0xff);
		status->substickY = (s8)((data[1]>>16)&0xff);
		status->triggerL = (u8)((data[1]>>8)&0xff);
		status->triggerR = (u8)data[1]&0xff;
		status->analogA = 0;
		status->analogB = 0;
	} else if(mode==0x400) {
		status->substickX = (s8)((data[1]>>24)&0xff);
		status->substickY = (s8)((data[1]>>16)&0xff);
		status->triggerL = 0;
		status->triggerR = 0;
		status->analogA = (u8)((data[1]>>8)&0xff);
		status->analogB = (u8)data[1]&0xff ;
	} else if(!mode || mode==0x500 || mode==0x600 || mode==0x700) {
		status->substickX = (s8)((data[1]>>24)&0xff);
		status->substickY = (s8)((data[1]>>16)&0xff);
		status->triggerL = (u8)((data[1]>>8)&0xf0);
		status->triggerR = (u8)((data[1]>>4)&0xf0);
		status->analogA = (u8)(data[1]&0xf0);
		status->analogB = (u8)((data[1]<<4)&0xf0);
	}

	status->stickX -= 128;
	status->stickY -= 128;
	status->substickX -= 128;
	status->substickY -= 128;
	
	status->stickX = __pad_clampS8(status->stickX,__pad_origin[chan][2]);
	status->stickY = __pad_clampS8(status->stickY,__pad_origin[chan][3]);
	status->substickX = __pad_clampS8(status->substickX,__pad_origin[chan][4]);
	status->substickY = __pad_clampS8(status->substickY,__pad_origin[chan][5]);
	status->triggerL = __pad_clampU8(status->triggerL,__pad_origin[chan][6]);
	status->triggerR = __pad_clampU8(status->triggerR,__pad_origin[chan][7]);
}

static void __pad_updateorigin(s32 chan)
{
	u32 mode,mask,type;

	mask = PAD_ENABLEDMASK(chan);
	mode = __pad_analogmode&0x0700;
	if(mode==0x0100) {
		__pad_origin[chan][4] &= ~0x0f;
		__pad_origin[chan][5] &= ~0x0f;
		__pad_origin[chan][8] &= ~0x0f;
		__pad_origin[chan][9] &= ~0x0f;
	} else if(mode==0x200) {
		__pad_origin[chan][4] &= ~0x0f;
		__pad_origin[chan][5] &= ~0x0f;
		__pad_origin[chan][6] &= ~0x0f;
		__pad_origin[chan][7] &= ~0x0f;
	}

	__pad_origin[chan][2] -= 128;
	__pad_origin[chan][3] -= 128;
	__pad_origin[chan][4] -= 128;
	__pad_origin[chan][5] -= 128;

	if(__pad_xpatchbits&mask && (s32)__pad_origin[chan][2]>64) {
		type = SI_GetType(chan)&~0xffff;
		if(!(type&~0x09ffffff)) __pad_origin[chan][2] = 0;
	}
}

static void __pad_probecallback(s32 chan,u32 type)
{
	if(!(type&0x0f)) {
		__pad_enable(__pad_resettingchan);
		__pad_waitingbits |= PAD_ENABLEDMASK(__pad_resettingchan);
	}
	__pad_doreset();
}

static void __pad_origincallback(s32 chan,u32 type)
{
#ifdef _PAD_DEBUG
	printf("__pad_origincallback(%d,%08x)\n",chan,type);
#endif
	if(!(type&0x0f)) {
		__pad_updateorigin(__pad_resettingchan);
		__pad_enable(__pad_resettingchan);
	}
	__pad_doreset();
}

static void __pad_originupdatecallback(s32 chan,u32 type)
{
	u32 en_bits = __pad_enabledbits&PAD_ENABLEDMASK(chan);

	if(en_bits) {
		if(!(type&0x0f)) __pad_updateorigin(chan);
		if(type&0x08) __pad_disable(chan);
	}
}

static void __pad_typeandstatuscallback(s32 chan,u32 type)
{
	u32 recal_bits,mask,ret = 0;
#ifdef _PAD_DEBUG
	printf("__pad_typeandstatuscallback(%d,%08x)\n",chan,type);
#endif
	mask = PAD_ENABLEDMASK(__pad_resettingchan);
	recal_bits = __pad_recalibratebits&mask;
	__pad_recalibratebits &= ~mask;
	
	if(type&0x0f) {
		__pad_doreset();
		return;
	}
	
	__pad_type[__pad_resettingchan] = (type&~0xff);
	if(((type&0x18000000)-0x08000000)
		|| !(type&0x01000000)) {
		__pad_doreset();
		return;
	}

	if(__pad_spec<2) {
		__pad_enable(__pad_resettingchan);
		__pad_doreset();
		return;
	}
	
	if(!(type&0x80000000) || type&0x04000000) {
		if(recal_bits) ret = SI_Transfer(__pad_resettingchan,&__pad_cmdcalibrate,3,__pad_origin[__pad_resettingchan],10,__pad_origincallback,0);
		else ret = SI_Transfer(__pad_resettingchan,&__pad_cmdreadorigin,1,__pad_origin[__pad_resettingchan],10,__pad_origincallback,0);
	} else if(type&0x00100000 && !(type&0x00080000) && !(type&0x00040000)) {
		if(type&0x40000000) ret = SI_Transfer(__pad_resettingchan,&__pad_cmdreadorigin,1,__pad_origin[__pad_resettingchan],10,__pad_origincallback,0);
		else ret = SI_Transfer(__pad_resettingchan,&__pad_cmdprobedevice[__pad_resettingchan],3,__pad_origin[__pad_resettingchan],8,__pad_probecallback,0);
	}
	if(!ret) {
		__pad_pendingbits |= mask;
		__pad_doreset();
	}
}

static void __pad_receivecheckcallback(s32 chan,u32 type)
{
	u32 mask,tmp;
#ifdef _PAD_DEBUG
	printf("__pad_receivecheckcallback(%d,%08x)\n",chan,type);
#endif
	mask = PAD_ENABLEDMASK(chan);
	if(__pad_enabledbits&mask) {
		tmp = type&0xff;
		type &= ~0xff;
		__pad_waitingbits &= ~mask;
		__pad_checkingbits &= ~mask;
		if(!(tmp&0x0f) && type&0x80100000 && !(type&0x040C0000))  SI_Transfer(chan,&__pad_cmdreadorigin,1,__pad_origin[chan],10,__pad_originupdatecallback,0);
		else __pad_disable(chan);
	}
}

static void __pad_enable(u32 chan)
{
	u32 buf[2];
#ifdef _PAD_DEBUG
	printf("__pad_enable(%d)\n",chan);
#endif
	__pad_enabledbits |= PAD_ENABLEDMASK(chan);
	SI_GetResponse(chan,(void*)buf);
	SI_SetCommand(chan,(__pad_analogmode|0x00400000));
	SI_EnablePolling(__pad_enabledbits);
}

static void __pad_disable(u32 chan)
{
	u32 level,mask;
#ifdef _PAD_DEBUG
	printf("__pad_disable(%d)\n",chan);
#endif
	_CPU_ISR_Disable(level);
	mask = PAD_ENABLEDMASK(chan);
	SI_DisablePolling(mask);
	__pad_enabledbits &= ~mask;
	__pad_waitingbits &= ~mask;
	__pad_pendingbits &= ~mask;
	__pad_checkingbits &= ~mask;
	_CPU_ISR_Restore(level);
}

static void __pad_doreset()
{
	__pad_resettingchan = cntlzw(__pad_resettingbits);
	if(__pad_resettingchan==32) return;
#ifdef _PAD_DEBUG
	printf("__pad_doreset(%d)\n",__pad_resettingchan);
#endif
	__pad_resettingbits &= ~PAD_ENABLEDMASK(__pad_resettingchan);
	
	memset(__pad_origin[__pad_resettingchan],0,12);
	SI_GetTypeAsync(__pad_resettingchan,__pad_typeandstatuscallback);
}

u32 PAD_Init()
{
	u32 chan;
	u16 *pprodpads = (u16*)0x800030e0;
#ifdef _PAD_DEBUG
	printf("PAD_Init()\n");
#endif
	if(__pad_initialized) return 1;

	if(__pad_spec) PAD_SetSpec(__pad_spec);
	
	__pad_initialized = 1;
	__pad_recalibratebits = 0xf0000000;
	
	chan = 0;
	while(chan<4) {
		__pad_cmdprobedevice[chan] = 0x4d000000|(chan<<22)|_SHIFTL(pprodpads[0],8,14);
		chan++;
	}

	SI_RefreshSamplingRate();
	return PAD_Reset(0xf0000000);
}

u32 PAD_Read(PADStatus *status)
{
	u32 chan,mask,ret;
	u32 level,sistatus,type;
	u32 buf[2];
#ifdef _PAD_DEBUG
	printf("PAD_Read(%p)\n",status);
#endif
	_CPU_ISR_Disable(level);
	chan = 0;
	ret = 0;
	while(chan<4) {
		mask = PAD_ENABLEDMASK(chan);
#ifdef _PAD_DEBUG
		printf("PAD_Read(%d,%d,%d,%d,%d)\n",chan,__pad_resettingchan,!!(__pad_pendingbits&mask),!!(__pad_resettingbits&mask),!(__pad_enabledbits&mask));
#endif
		if(__pad_pendingbits&mask) {
			PAD_Reset(0);
			memset(&status[chan],0,sizeof(PADStatus));
			status[chan].err = PAD_ERR_NOT_READY;
		} else if(__pad_resettingbits&mask || __pad_resettingchan==chan) {
			memset(&status[chan],0,sizeof(PADStatus));
			status[chan].err = PAD_ERR_NOT_READY;
		} else if(!(__pad_enabledbits&mask)) {
			memset(&status[chan],0,sizeof(PADStatus));
			status[chan].err = PAD_ERR_NO_CONTROLLER;
		} else {
			if(SI_IsChanBusy(chan)) {
				memset(&status[chan],0,sizeof(PADStatus));
				status[chan].err = PAD_ERR_TRANSFER;
			} else {
				sistatus = SI_GetStatus(chan);
#ifdef _PAD_DEBUG
				printf("PAD_Read(%08x)\n",sistatus);
#endif
				if(sistatus&0x08) {
#ifdef _PAD_DEBUG
					printf("PAD_Read(%08x)\n",sistatus);
#endif
					SI_GetResponse(chan,(void*)buf);
					if(!(__pad_waitingbits&mask)) {
						memset(&status[chan],0,sizeof(PADStatus));
						status[chan].err = PAD_ERR_NONE;
						if(!(__pad_checkingbits&mask)) {
							__pad_checkingbits |= mask;
							SI_GetTypeAsync(chan,__pad_receivecheckcallback);
						}
					} else {
						__pad_disable(chan);
						memset(&status[chan],0,sizeof(PADStatus));
						status[chan].err = PAD_ERR_NO_CONTROLLER;
					}
				} else {
					type = SI_GetType(chan);
#ifdef _PAD_DEBUG
					printf("PAD_Read(%08x)\n",type);
#endif
					if(!(type&0x02000000)) ret |= mask;
					if(!SI_GetResponse(chan,buf)
						|| buf[0]&0x80000000) {
#ifdef _PAD_DEBUG
						printf("PAD_Read(%08x %08x)\n",buf[0],buf[1]);
#endif
						memset(&status[chan],0,sizeof(PADStatus));
						status[chan].err = PAD_ERR_TRANSFER;
					} else {
#ifdef _PAD_DEBUG
						printf("PAD_Read(%08x %08x)\n",buf[0],buf[1]);
#endif
						__pad_makestatus(chan,buf,&status[chan]);
#ifdef _PAD_DEBUG
						printf("PAD_Read(%08x)\n",status[chan].button);
#endif
						if(status[chan].button&0x00002000) {
							memset(&status[chan],0,sizeof(PADStatus));
							status[chan].err = PAD_ERR_TRANSFER;
							SI_Transfer(chan,&__pad_cmdreadorigin,1,__pad_origin[chan],10,__pad_originupdatecallback,0);
						} else {
							status[chan].err = PAD_ERR_NONE;
							status[chan].button &= ~0x80;
						}
					}
				}
			}
		}
		chan++;
		
	}
	_CPU_ISR_Restore(level);

	return ret;
}

u32 PAD_Reset(u32 mask)
{
	u32 level;
	u32 pend_bits,en_bits;

	_CPU_ISR_Disable(level);
	pend_bits = (__pad_pendingbits|mask);
	__pad_pendingbits = 0;
	
	pend_bits &= ~(__pad_waitingbits|__pad_checkingbits);
	__pad_resettingbits |= pend_bits;
	
	en_bits = (__pad_resettingbits&__pad_enabledbits);
	__pad_enabledbits &= ~pend_bits;
	
	if(__pad_spec==4) __pad_recalibratebits |= pend_bits;
	
	SI_DisablePolling(en_bits);
	if(__pad_resettingchan==32) __pad_doreset();
	_CPU_ISR_Restore(level);
	
	return 1;
}

void PAD_SetSpec(u32 spec)
{
	if(__pad_initialized) return;

	__pad_spec = 0;
	if(spec==0) __pad_makestatus = SPEC0_MakeStatus;
	else if(spec==1) __pad_makestatus = SPEC1_MakeStatus;
	else if(spec<6) __pad_makestatus = SPEC2_MakeStatus;
	
	__pad_spec = spec;
}

void PAD_ControlMotor(s32 chan,u32 cmd)
{
	u32 level;
	u32 mask,type;

	_CPU_ISR_Disable(level);
	
	mask = PAD_ENABLEDMASK(chan);
	if(__pad_enabledbits&mask) {
		type = SI_GetType(chan);
		if(!(type&0x20000000)) {
			if(__pad_spec<2 && cmd==PAD_MOTOR_STOP_HARD) cmd = 0;

			cmd = 0x00400000|__pad_analogmode|(cmd&0x03);
			SI_SetCommand(chan,cmd);
			SI_TransferCommands();
		}
	}
	_CPU_ISR_Restore(level);
}

//---------------------------------------------------------------------------------
typedef struct tagKeyInput{
	vs16	up,
			down,
			state,
			oldstate;
	}__attribute__ ((packed)) KeyInput;

static volatile KeyInput Keys[4] = { {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
static PADStatus pad[4];


//---------------------------------------------------------------------------------
void PAD_ScanPads() {
//---------------------------------------------------------------------------------

	PAD_Read(pad);
	int index;

	for ( index = 0; index < 4; index++) {

		Keys[index].oldstate	= Keys[index].state;
		Keys[index].state		= pad[index].button;
		Keys[index].up			= Keys[index].oldstate & ~Keys[index].state;
		Keys[index].down		= ~Keys[index].oldstate & Keys[index].state;
	}			
}

//---------------------------------------------------------------------------------
u16 PAD_ButtonsUp(int pad) {
//---------------------------------------------------------------------------------
	if (pad<0 || pad >3) return 0;
	
	return 	Keys[pad].up;
}
//---------------------------------------------------------------------------------
u16 PAD_ButtonsDown(int pad) {
//---------------------------------------------------------------------------------
	if (pad<0 || pad >3) return 0;

	return 	Keys[pad].down;
}

//---------------------------------------------------------------------------------
u16 PAD_ButtonsHeld(int pad) {
//---------------------------------------------------------------------------------
	if (pad<0 || pad >3) return 0;

	return Keys[pad].state;
}
