#include <tuxedo/interrupt.h>
#include <tuxedo/mm.h>
#include <tuxedo/ppc/exception.h>
#include <tuxedo/ppc/intrinsics.h>

typedef struct KIrqCookie {
	KIrqHandlerFn fn;
	void* user;
} KIrqCookie;

static KIrqCookie s_irqCookies[IRQ_MAX];

void PPCExcptIrqHandler(void);
void KTickTaskRun(void);

void KIrqInit(void)
{
	PPCExcptSetHandler(PPC_EXCPT_IRQ,  PPCExcptIrqHandler);
	PPCExcptSetHandler(PPC_EXCPT_DECR, PPCExcptIrqHandler);

	HWREG_PI[1]    = 0;
	HWREG_MI[14]   = 0;
	HWREG_DSP[5]  &= ~0x1f8;
	HWREG_AI[0]   &= ~0x2c;
	HWREG_EXI[0]  &= ~0x2c0f;
	HWREG_EXI[5]  &= ~0x0c0f;
	HWREG_EXI[10] &= ~0x000f;
	HWREG_PI[1]    = 0xf0;

	PPCIrqUnlockByMsr(MSR_EE);
}

MK_INLINE int KIrqDecodeExiCause(u32 exi_cause)
{
	exi_cause &= exi_cause << 1;
	if (exi_cause & (1U<<1)) return IRQ_EXI0_EXI;
	if (exi_cause & (1U<<3)) return IRQ_EXI0_TC;
	if (exi_cause & (1U<<11)) return IRQ_EXI0_EXT;
	return -1;
}

MK_INLINE int KIrqGetCause(void)
{
	u32 pi_cause = HWREG_PI[0] & HWREG_PI[1];

	if (pi_cause & (1U<<0)) return IRQ_PI_ERROR;
	if (pi_cause & (1U<<12)) return IRQ_PI_DEBUG;

	if (pi_cause & (1U<<7)) {
		u32 mi_cause = (HWREG_MI[14] & HWREG_MI[15]) & 0x1f;
		if (mi_cause) return __builtin_ffs(mi_cause)-1+IRQ_MEM0;
	}

	if (pi_cause & (1U<<1)) return IRQ_PI_RSW;
	if (pi_cause & (1U<<8)) return IRQ_PI_VI;
	if (pi_cause & (1U<<9)) return IRQ_PI_PETOKEN;
	if (pi_cause & (1U<<10)) return IRQ_PI_PEFINISH;
	if (pi_cause & (1U<<13)) return IRQ_PI_HSP;

	bool has_dsp_ai = false;
	if (pi_cause & (1U<<6)) {
		u32 dsp_cause = HWREG_DSP[5];
		dsp_cause &= dsp_cause >> 1;

		if (dsp_cause & (1U<<5)) return IRQ_DSP_ARAM;
		if (dsp_cause & (1U<<7)) return IRQ_DSP_DSP;
		if (dsp_cause & (1U<<3)) has_dsp_ai = true;
	}

	if (pi_cause & (1U<<5)) {
		u32 ai_cause = HWREG_AI[0];
		ai_cause &= ai_cause << 1;
		if (ai_cause & (1U<<3)) return IRQ_AI;
	}

	if (pi_cause & (1U<<4)) {
		int cause;
		if ((cause = KIrqDecodeExiCause(HWREG_EXI[0])) >= 0) return cause;
		if ((cause = KIrqDecodeExiCause(HWREG_EXI[5])) >= 0) return cause + IRQ_EXI1_EXI - IRQ_EXI0_EXI;
		if ((cause = KIrqDecodeExiCause(HWREG_EXI[10]&~(1U<<11))) >= 0) return cause + IRQ_EXI2_EXI - IRQ_EXI0_EXI;
	}

	if (pi_cause & (1U<<3)) return IRQ_PI_SI;
	if (pi_cause & (1U<<2)) return IRQ_PI_DI;
	if (has_dsp_ai) return IRQ_DSP_AI;
	if (pi_cause & (1U<<11)) return IRQ_PI_CP;
#ifdef __wii__
	if (pi_cause & (1U<<14)) return IRQ_PI_ACR;
#endif

	return -1;
}

void KIrqDispatch(unsigned exid)
{
	if (__builtin_expect(exid == PPC_EXCPT_DECR, 1)) {
		KTickTaskRun();
		return;
	}

	int id = KIrqGetCause();
	if (id >= 0) {
		KIrqCookie const* cookie = &s_irqCookies[id];
		if (cookie->fn) {
			cookie->fn((KIrqId)id, cookie->user);
		}
	}
}

MK_INLINE u32 KIrqEncodeMem(u32 mask)
{
	return (mask >> IRQ_MEM0) & 0x1f;
}

MK_INLINE u32 KIrqEncodeDsp(u32 mask)
{
	u32 ret = 0;
	if (mask & IM_DSP_ARAM) ret |= 1U<<6;
	if (mask & IM_DSP_DSP) ret |= 1U<<8;
	if (mask & IM_DSP_AI) ret |= 1U<<4;
	return ret;
}

MK_INLINE u32 KIrqEncodeAi(u32 mask)
{
	u32 ret = 0;
	if (mask & IM_AI) ret |= 1U<<2;
	return ret;
}

MK_INLINE u32 KIrqEncodeExi(u32 mask)
{
	u32 ret = 0;
	if (mask & IM_EXI0_EXI) ret |= 1U<<0;
	if (mask & IM_EXI0_TC) ret |= 1U<<2;
	if (mask & IM_EXI0_EXT) ret |= 1U<<10;
	return ret;
}

MK_INLINE u32 KIrqEncodePi(u32 mask)
{
	u32 ret = 0;
	if (mask & IM_PI_ERROR) ret |= 1U<<0;
	if (mask & IM_PI_DEBUG) ret |= 1U<<12;
	if (mask & IM_PI_RSW) ret |= 1U<<1;
	if (mask & IM_PI_VI) ret |= 1U<<8;
	if (mask & IM_PI_PETOKEN) ret |= 1U<<9;
	if (mask & IM_PI_PEFINISH) ret |= 1U<<10;
	if (mask & IM_PI_HSP) ret |= 1U<<13;
	if (mask & IM_PI_SI) ret |= 1U<<3;
	if (mask & IM_PI_DI) ret |= 1U<<2;
	if (mask & IM_PI_CP) ret |= 1U<<11;
#ifdef __wii__
	if (mask & IM_PI_ACR) ret |= 1U<<14;
#endif
	return ret;
}

MK_INLINE void KIrqModify(u32 clr_mask, u32 set_mask)
{
	PPCIrqState st = PPCIrqLockByMsr();
	u32 mod_mask = clr_mask^set_mask;
	clr_mask &= mod_mask;
	set_mask &= mod_mask;

	if (mod_mask & IM_MEM) {
		u32 reg = HWREG_MI[14];
		reg &= ~KIrqEncodeMem(clr_mask);
		reg |=  KIrqEncodeMem(set_mask);
		HWREG_MI[14] = reg;
	}

	if (mod_mask & IM_DSP) {
		u32 reg = HWREG_DSP[5] &~ 0xa8;
		reg &= ~KIrqEncodeDsp(clr_mask);
		reg |=  KIrqEncodeDsp(set_mask);
		HWREG_DSP[5] = reg;
	}

	if (mod_mask & IM_AI) {
		u32 reg = HWREG_AI[0] &~ 0x8;
		reg &= ~KIrqEncodeAi(clr_mask);
		reg |=  KIrqEncodeAi(set_mask);
		HWREG_AI[0] = reg;
	}

	if (mod_mask & IM_EXI0) {
		u32 reg = HWREG_EXI[0] &~ 0x280a;
		reg &= ~KIrqEncodeExi(clr_mask);
		reg |=  KIrqEncodeExi(set_mask);
		HWREG_EXI[0] = reg;
	}

	if (mod_mask & IM_EXI1) {
		u32 reg = HWREG_EXI[5] &~ 0x80a;
		reg &= ~KIrqEncodeExi(clr_mask << 3);
		reg |=  KIrqEncodeExi(set_mask << 3);
		HWREG_EXI[5] = reg;
	}

	if (mod_mask & IM_EXI2) {
		u32 reg = HWREG_EXI[10] &~ 0xa;
		reg &= ~KIrqEncodeExi((clr_mask << 6) &~ IM_EXI0_EXT);
		reg |=  KIrqEncodeExi((set_mask << 6) &~ IM_EXI0_EXT);
		HWREG_EXI[10] = reg;
	}

	if (mod_mask & IM_PI) {
		u32 reg = HWREG_PI[1];
		reg &= ~KIrqEncodePi(clr_mask);
		reg |=  KIrqEncodePi(set_mask);
		HWREG_PI[1] = reg;
	}

	PPCIrqUnlockByMsr(st);
}

void KIrqEnable(u32 mask)
{
	KIrqModify(0, mask);
}

void KIrqDisable(u32 mask)
{
	KIrqModify(mask, 0);
}

KIrqHandlerFn KIrqGet(KIrqId id)
{
	return s_irqCookies[id].fn;
}

KIrqHandlerFn KIrqSet(KIrqId id, KIrqHandlerFn fn, void* user)
{
	PPCIrqState st = PPCIrqLockByMsr();

	KIrqHandlerFn old = s_irqCookies[id].fn;
	s_irqCookies[id].fn = fn;
	s_irqCookies[id].user = user;

	PPCIrqUnlockByMsr(st);
	return old;
}
