#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <irq.h>
#include "asm.h"
#include "processor.h"
#include "pad.h"

#define SI_TC_INT			(1<<31)
#define SI_TC_MSK			(1<<30)
#define SI_COM_ERR			(1<<29)
#define SI_RS_INT			(1<<28)
#define SI_RS_MSK			(1<<27)
#define SI_CHN_MSK(v)		((v&3)<<25)
#define SI_CHN_EN			(1<<24)
#define SI_OUT_LEN(v)		((v&40)<<16)
#define SI_IN_LEN(v)		((v&40)<<8)
#define SI_CMD_EN			(1<<7)
#define SI_CB_EN			(1<<6)
#define SI_CHANNEL(v)		((v&3)<<1)
#define SI_TRS_EN			(1<<0)

typedef void (*SIRSHandler)(u32,void*);

static u32 _padId[PAD_MAX];
static SIRSHandler _sirshandler[PAD_MAX];

static vu32* const _siReg = (u32*)0xCC006400;

extern void udelay(int);
extern void __UnmaskIrq(u32);

static __inline__ void __si_wait_tc_complete()
{
	while(!(_siReg[13]&SI_TC_INT));
	_siReg[13] |= SI_TC_INT;
}

static void __si_tc_handler(u32 nIrq,void *pCtx)
{
	u32 status;

	status = _siReg[13];
	if(status&SI_TC_INT) {
		_siReg[13] |= SI_TC_INT;
	}
	if(status&SI_RS_INT) {
		_siReg[13] |= SI_RS_INT;
	}
}

static void __reset_si()
{
	u32 level,i;

	_CPU_ISR_Disable(level);
	for(i=0;i<PAD_MAX;i++) _siReg[(i*3)+0] = 0;
	for(i=0;i<PAD_MAX;i++) _siReg[(i*3)+1] = 0;
	for(i=0;i<PAD_MAX;i++) _siReg[(i*3)+2] = 0;

	_siReg[12] = 0;
	_siReg[13] = 0;
	_siReg[14] = 0;

	for(i=0;i<12;i++) _siReg[32+i] = 0;
	_CPU_ISR_Restore(level);
}

static void __set_polling_interrupt(u32 intr)
{
	u32 level;

	_CPU_ISR_Disable(level);
	_siReg[13] &= ~SI_RS_MSK;
	if(intr) {
		memset(_sirshandler,0,PAD_MAX*sizeof(SIRSHandler));
		_siReg[13] |= SI_RS_MSK;
	}
	_siReg[13] &= ~(SI_TC_INT|SI_TRS_EN);
	_CPU_ISR_Restore(level);
}

static void __set_polling()
{
	u32 level,i,pad_bits = 0;

	_CPU_ISR_Disable(level);
	for(i=0;i<PAD_MAX;i++) {
		_siReg[(i*3)+0] = 0x00400300;
		pad_bits |= 1<<(7-i);
	}
	_siReg[12] = (0x00f70200|pad_bits);
	_siReg[14] = 0x80000000;
	_siReg[13] = (SI_TC_INT|SI_TC_MSK|SI_OUT_LEN(1)|SI_IN_LEN(8)|SI_TRS_EN);
	_CPU_ISR_Restore(level);

	__si_wait_tc_complete();
}

static u32 __get_controllerid(u32 port)
{
	u32 level;

	__reset_si();

	_CPU_ISR_Disable(level);
	_siReg[12] = 0;
	_siReg[(port*3)+0] = 0;
	_siReg[14] = 0x80000000;
	_siReg[13] = 0xd0010001|(port<<1);
	_CPU_ISR_Restore(level);
	
	__si_wait_tc_complete();

	return _siReg[32];
}

void __pad_init()
{
	u32 i;

	for(i=0;i<PAD_MAX;i++) 
		_padId[i] = __get_controllerid(i)>>16;
	
	__set_polling();

	IRQ_Request(IRQ_PI_SI,__si_tc_handler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_SI));
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
