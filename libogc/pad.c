#include <stdlib.h>
#include <time.h>
#include "asm.h"
#include "pad.h"

static vu32* const siReg = (u32*)0xCC006400;

extern void udelay(int);

void __pad_init()
{
	*(vu32*)0xCC006430 = 0x00000000;
	*(vu32*)0xCC006438 = 0x80000000;
	*(vu32*)0xCC006430 = 0x00f70200;
	*(vu32*)0xCC006438 = 0x80000000;
	*(vu32*)0xCC006480 = 0x00000000;
	*(vu32*)0xCC006434 = 0xc0010301;
	*(vu32*)0xCC006438 = 0x00000000;

	(void)*(vu32*)0xCC006434;

	*(vu32*)0xCC006430 |= 0xF0; // enable all four controller ports

	*(vu32*)0xCC006400  = 0x00400300;
	*(vu32*)0xCC00640C  = 0x00400300;
	*(vu32*)0xCC006418  = 0x00400300;
	*(vu32*)0xCC006424  = 0x00400300;

	udelay(1000);

	*(vu32*)0xCC006430 = 0x00000000;
	*(vu32*)0xCC006438 = 0x80000000;
	*(vu32*)0xCC006430 = 0x00f70200;
	*(vu32*)0xCC006438 = 0x80000000;
	*(vu32*)0xCC006480 = 0x00000000;
	*(vu32*)0xCC006434 = 0xc0010301;
	*(vu32*)0xCC006438 = 0x00000000;

	(void)*(vu32*)0xCC006434;

	*(vu32*)0xCC006430 |= 0xF0; // enable all four controller ports

	*(vu32*)0xCC006400  = 0x00400300;
	*(vu32*)0xCC00640C  = 0x00400300;
	*(vu32*)0xCC006418  = 0x00400300;
	*(vu32*)0xCC006424  = 0x00400300;
}

void PAD_ReadState(PAD *pPad, u8 PadChannel)
{
	u32 Value32[3];
	u8 Value8[2];
	s8 ValueS8[2];
	//u32 PadChannelAddr = 0xCC006400 + (PadChannel*12);
	u32 PadChannelAddr = MEM_PAD_CHANNEL(PadChannel); /* added in 0.2 */
	PAD_BUTTONS_DIGITAL *pDigital=&pPad->Digital;

	Value32[0] = *(u32 *)(PadChannelAddr+0);
	Value32[1] = *(u32 *)(PadChannelAddr+4);
	Value32[2] = *(u32 *)(PadChannelAddr+8);

	// digital buttons
	Value8[0] = (Value32[1] >> 16);
	Value8[1] = (Value32[1] >> 16) >> 8;

	// digital buttons
	pDigital->Left  = Value8[0] & 0x01;
	pDigital->Right = Value8[0] & 0x02;
	pDigital->Down  = Value8[0] & 0x04;
	pDigital->Up    = Value8[0] & 0x08;

	pDigital->Z     = Value8[0] & 0x10;
	pDigital->L     = Value8[0] & 0x20;
	pDigital->R     = Value8[0] & 0x40;

	pDigital->A     = Value8[1] & 0x01;
	pDigital->B     = Value8[1] & 0x02;
	pDigital->X     = Value8[1] & 0x04;
	pDigital->Y     = Value8[1] & 0x08;

	pDigital->Start = Value8[1] & 0x10;


	// analog stick
	ValueS8[0] = ((Value32[1]) & 0xff);
	ValueS8[1] = ((Value32[1]) & 0xff00) >> 8;

	pPad->Analog.X = 0x80+ValueS8[1];
	pPad->Analog.Y = 0x80-ValueS8[0];


	// trigger L and R
	Value8[0] = ((Value32[2]) & 0xff);
	Value8[1] = ((Value32[2]) & 0xff00) >> 8;

	pPad->Trigger.L = Value8[0];
	pPad->Trigger.R = Value8[1];


	// analoc C stick
	ValueS8[0] = (Value32[2] >> 16);
	ValueS8[1] = (Value32[2] >> 16) >> 8;

	pPad->AnalogC.X = 0x80+ValueS8[1];
	pPad->AnalogC.Y = 0x80-ValueS8[0];
}
