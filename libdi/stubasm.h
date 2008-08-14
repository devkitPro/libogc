#ifndef __STUBASM_H__
#define __STUBASM_H__

#include <ogc/machine/asm.h>

#define	MEM1_WORDS	0x1002

#ifdef _LANGUAGE_ASSEMBLY

#define	MSR_OFF		0x00
#define	R1_OFF		0x04
#define	R2_OFF		0x08
#define	R13_OFF		0x0c
#define	R14_OFF		0x10
#define	R15_OFF		0x14
#define	R16_OFF		0x18
#define	R17_OFF		0x1c
#define	R18_OFF		0x20
#define	R19_OFF		0x24
#define	R20_OFF		0x28
#define	R21_OFF		0x2c
#define	R22_OFF		0x30
#define	R23_OFF		0x34
#define	R24_OFF		0x38
#define	R25_OFF		0x3c
#define	R26_OFF		0x40
#define	R27_OFF		0x44
#define	R28_OFF		0x48
#define	R29_OFF		0x4c
#define	R30_OFF		0x50
#define	R31_OFF		0x54
#define	MEM1_OFF	0x58
#define	SPRG0_OFF	0x4060
#define	SPRG1_OFF	0x4064
#define	SPRG2_OFF	0x4068
#define	SPRG3_OFF	0x406C
#define	HID0_OFF	0x4070
#define	HID1_OFF	0x4074
#define	HID2_OFF	0x4078
#define	HID4_OFF	0x407C
#define	WPAR_OFF	0x4080

#else

typedef struct {

	u32	_msr;
	u32	_r1, _r2;
	u32	_r13, _r14, _r15, _r16, _r17, _r18;
	u32	_r19, _r20, _r21, _r22, _r23, _r24;
	u32	_r25, _r26, _r27, _r28, _r29, _r30;
	u32	_r31;
	
	u32	_mem1[MEM1_WORDS];
	
	u32	_sprg0, _sprg1, _sprg2, _sprg3;
	u32	_hid0, _hid1, _hid2, _hid4;
	
	u32	_wpar;

} context_storage;

typedef struct {

	u32	piReg[6];
	u64	timebase;

} register_storage;

void __distub_take_plunge(context_storage *ctx);

#endif

#endif
