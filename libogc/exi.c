#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "asm.h"
#include "irq.h"
#include "processor.h"
#include "spinlock.h"
#include "exi.h"

//#define _EXI_DEBUG

#define EXI_MAX_CHANNELS			3
#define EXI_MAX_DEVICES				3

#define EXI_DEVICE0					0x0080
#define EXI_DEVICE1					0x0100
#define EXI_DEVICE2					0x0200

#define EXI_EXI_IRQ					0x0002
#define EXI_TC_IRQ					0x0008
#define EXI_EXT_IRQ					0x0800
#define EXI_EXT_BIT					0x1000

#define EXI_FLAG_DMA				0x0001
#define EXI_FLAG_IMM				0x0002
#define EXI_FLAG_SELECT				0x0004
#define EXI_FLAG_ATTACH				0x0008
#define EXI_FLAG_LOCKED				0x0010

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))


typedef struct _exibus_priv {
	EXICallback CallbackEXI;
	EXICallback CallbackTC;
	EXICallback CallbackEXT;

	u32 imm_len;
	void *imm_buff;
	u32 lockeddev;
	u32 flags;
	u32 lck_cnt;
	u32 var1,var2;
	struct _lck_dev {
		u32 dev;
		EXICallback unlockcb;
	} lck_dev[EXI_MAX_DEVICES];

} exibus_priv;

static exibus_priv eximap[EXI_MAX_CHANNELS];

static u32 exiids[EXI_MAX_CHANNELS];

static void __exi_irq_handler(u32,void *);
static void __tc_irq_handler(u32,void *);
static void __ext_irq_handler(u32,void *);

static vu32* const _exiReg = (u32*)0xCC006800;

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern void __exi_imm(u32,void *,u32,u32);

static u32 __exi_probe(u32 nChn)
{
	u32 level,val,ret = 1;
	exibus_priv *exi = &eximap[nChn];

	_CPU_ISR_Disable(level);
	val = _exiReg[nChn*5];
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(val&EXI_EXT_IRQ) {
			_exiReg[nChn*5] = (val&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ))|EXI_EXT_IRQ;
			exi->var2 = 0;
		}
		if(_exiReg[nChn*5]&EXI_EXT_BIT) {
		} else {
			exi->var2 = 0;
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	if(!(_exiReg[nChn*5]&EXI_EXT_BIT) || (_exiReg[nChn*5]&EXI_EXT_IRQ)) {
		exi->var2 = 0;
		ret = 0;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

static __inline__ void __exi_clearirqs(u32 nChn,u32 nEXIIrq,u32 nTCIrq,u32 nEXTIrq)
{
	u32 d;
#ifdef _EXI_DEBUG
	printf("__exi_clearirqs(%d,%d,%d,%d)\n",nChn,nEXIIrq,nTCIrq,nEXTIrq);
#endif
	d = (_exiReg[nChn*5]&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ));
	if(nEXIIrq) d |= EXI_EXI_IRQ;
	if(nTCIrq) d |= EXI_TC_IRQ;
	if(nEXTIrq) d |= EXI_EXT_IRQ;
	_exiReg[nChn*5] = d;
}

static inline void __exi_setinterrupts(u32 nChn,exibus_priv *exi)
{
	exibus_priv *pexi = &eximap[EXI_CHANNEL_2];
#ifdef _EXI_DEBUG
	printf("__exi_setinterrupts(%d,%p)\n",nChn,exi);
#endif
	if(nChn==EXI_CHANNEL_0) {
		__MaskIrq((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI2_EXI)));
		if(!(exi->flags&EXI_FLAG_LOCKED) && (exi->CallbackEXI || pexi->CallbackEXI))
			__UnmaskIrq((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI2_EXI)));
	} else if(nChn==EXI_CHANNEL_1) {
		__MaskIrq(IRQMASK(IRQ_EXI1_EXI));
		if(!(exi->flags&EXI_FLAG_LOCKED) && exi->CallbackEXI) __UnmaskIrq(IRQMASK(IRQ_EXI1_EXI));
	} else if(nChn==EXI_CHANNEL_2) {				//explicitly use of channel 2 only if debugger is attached.
		__MaskIrq(IRQMASK(IRQ_EXI0_EXI));
		if(!(exi->flags&EXI_FLAG_LOCKED) && IRQ_GetHandler(IRQ_PI_DEBUG)) __UnmaskIrq(IRQMASK(IRQ_EXI2_EXI));
	}
}

u32 EXI_Lock(u32 nChn,u32 nDev,EXICallback unlockCB)
{
	u32 level,i;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Lock(%d,%d,%p)\n",nChn,nDev,unlockCB);
#endif
	_CPU_ISR_Disable(level);
	if(exi->flags&EXI_FLAG_LOCKED) {
		if(unlockCB) {
			for(i=0;i<exi->lck_cnt;i++) {
				if(exi->lck_dev[i].dev==nDev)  {
					_CPU_ISR_Restore(level);
					return 0;
				}	
			}
			exi->lck_dev[i].unlockcb = unlockCB;
			exi->lck_dev[i].dev = nDev;
			exi->lck_cnt++;
		}
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->lockeddev = nDev;
	exi->flags |= EXI_FLAG_LOCKED;
	__exi_setinterrupts(nChn,exi);

	_CPU_ISR_Restore(level);
	return 1;
}

u32 EXI_Unlock(u32 nChn)
{
	u32 level,dev;
	EXICallback cb;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Unlock(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);
	if(!(exi->flags&EXI_FLAG_LOCKED)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->flags &= ~EXI_FLAG_LOCKED;
	__exi_setinterrupts(nChn,exi);
	
	if(!exi->lck_cnt) {
		_CPU_ISR_Restore(level);
		return 1;
	}

	cb = exi->lck_dev[0].unlockcb;
	dev = exi->lck_dev[0].dev;
	if((--exi->lck_cnt)>0) memmove(&(exi->lck_dev[0]),&(exi->lck_dev[1]),(exi->lck_cnt*sizeof(struct _lck_dev)));
	if(cb) cb(nChn,dev);

	_CPU_ISR_Restore(level);
	return 1;
}

u32 EXI_Select(u32 nChn,u32 nDev,u32 nFrq)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Select(%d,%d,%d)\n",nChn,nDev,nFrq);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&EXI_FLAG_SELECT) {
#ifdef _EXI_DEBUG
		printf("EXI_Select(): allready selected.\n");
#endif
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(nChn!=EXI_CHANNEL_2) {
		if(nDev==EXI_DEVICE_0 && !(exi->flags&EXI_FLAG_ATTACH)) {
			if(__exi_probe(nChn)==0) {
				_CPU_ISR_Restore(level);
				return 0;
			}
		}
		if(exi->flags&EXI_FLAG_LOCKED && exi->lockeddev!=nDev) {
#ifdef _EXI_DEBUG
			printf("EXI_Select(): allready locked(%d).\n",exi->lockeddev);
#endif
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	exi->flags |= EXI_FLAG_SELECT;
	val = _exiReg[nChn*5];
	val = (val&0x405)|(0x80<<nDev)|(nFrq<<4);
	_exiReg[nChn*5] = val;

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __MaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __MaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}
	
	_CPU_ISR_Restore(level);
	return 1;
}

u32 EXI_Deselect(u32 nChn)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Deselect(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);

	if(!(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}
	
	exi->flags &= ~EXI_FLAG_SELECT;
	val = _exiReg[nChn*5];
	_exiReg[nChn*5] = (val&0x405);
	
	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __UnmaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __UnmaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	if(nChn!=EXI_CHANNEL_2 && val&EXI_DEVICE0) {
		if(__exi_probe(nChn)==0) {
			_CPU_ISR_Restore(level);
			return 0;
		}
	}
	_CPU_ISR_Restore(level);
	return 1;
}

u32 EXI_Sync(u32 nChn)
{
	u8 *buf;
	u32 level,ret,i,cnt,val;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Sync(%d)\n",nChn);
#endif
	while(_exiReg[nChn*5+3]&0x0001);
	
	_CPU_ISR_Disable(level);

	ret = 0;
	if(exi->flags&EXI_FLAG_SELECT && exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		if(exi->flags&EXI_FLAG_IMM) {
			cnt = exi->imm_len;
			buf = exi->imm_buff;
			if(buf && cnt>0) {
				val = _exiReg[nChn*5+4];
				for(i=0;i<cnt;i++) ((u8*)buf)[i] = (val>>((3-i)*8))&0xFF;
			}
		}
		exi->flags &= ~(EXI_FLAG_DMA|EXI_FLAG_IMM);
		ret = 1;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

u32 EXI_Imm(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tccb)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Imm(%d,%p,%d,%d,%p)\n",nChn,pData,nLen,nMode,tccb);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->CallbackTC = tccb;
	if(tccb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq(IRQMASK((IRQ_EXI0_TC+(nChn*3))));
	}
	exi->flags |= EXI_FLAG_IMM;
	
	if(nMode==EXI_WRITE) {
		exi->imm_buff = NULL;
		exi->imm_len = 0;
		_exiReg[nChn*5+4] = *(u32*)pData;
	} else if(nMode==EXI_READ) {
		exi->imm_buff = pData;
		exi->imm_len = nLen;
	}
	_exiReg[nChn*5+3] = ((nLen-1)<<4)|(nMode<<2)|0x01;
	
	_CPU_ISR_Restore(level);
	return 1;
}

u32 EXI_ImmEx(u32 nChn,void *pData,u32 nLen,u32 nMode)
{
	u8 *buf = pData;
	u32 tc,ret = 0;
#ifdef _EXI_DEBUG
	printf("EXI_ImmEx(%d,%p,%d,%d)\n",nChn,pData,nLen,nMode);
#endif
	while(nLen) {
		ret = 0;
		tc = nLen;
		if(tc>4) tc = 4;
		
		if(!EXI_Imm(nChn,buf,tc,nMode,NULL)) break;
		if(!EXI_Sync(nChn)) break;
		nLen -= tc;
		buf += tc;

		ret = 1;
	}
	return ret;
}

u32 EXI_Dma(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tccb)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Dma(%d,%p,%d,%d,%p)\n",nChn,pData,nLen,nMode,tccb);
#endif
	_CPU_ISR_Disable(level);
	
	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
#ifdef _EXI_DEBUG
		printf("EXI_Dma(%04x): abort\n",exi->flags);
#endif
		_CPU_ISR_Restore(level);
		return 0;
	}
#ifdef _EXI_DEBUG
	printf("EXI_Dma(tccb: %p)\n",tccb);
#endif
	exi->CallbackTC = tccb;
	if(tccb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq(IRQMASK((IRQ_EXI0_TC+(nChn*3))));
	}
	exi->flags |= EXI_FLAG_DMA;

	_exiReg[nChn*5+1] = (u32)pData&0x03FFFFE0;
	_exiReg[nChn*5+2] = nLen;
	_exiReg[nChn*5+3] = (nMode<<2)|0x03;
	
	_CPU_ISR_Restore(level);
	return 1;
}

u32 EXI_GetState(u32 nChn)
{
	exibus_priv *exi = &eximap[nChn];
	return exi->flags;
}

static u32 __unlocked_handler(u32 nChn,u32 nDev)
{
	u32 nId;
	EXI_GetID(nChn,nDev,&nId);
	return 1;
}

u32 EXI_GetID(u32 nChn,u32 nDev,u32 *nId)
{
	u32 ret,level,lck,reg;
	EXICallback cb;
	exibus_priv *exi = &eximap[nChn];

	if(nChn<EXI_CHANNEL_2) {
		if(__exi_probe(nChn)==0) return 0;
		
		_CPU_ISR_Disable(level);
		_CPU_ISR_Restore(level);
	}

	lck = 0;
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) lck = 1;

	if(lck)  ret = EXI_Lock(nChn,nDev,__unlocked_handler);
	else ret = EXI_Lock(nChn,nDev,NULL);

	if(ret) {
		if(EXI_Select(nChn,nDev,EXI_SPEED1MHZ)==1) {
			reg = 0;
			EXI_Imm(nChn,&reg,2,EXI_WRITE,NULL);
			EXI_Sync(nChn);
			EXI_Imm(nChn,nId,4,EXI_READ,NULL);
			EXI_Sync(nChn);
			EXI_Deselect(nChn);
		}
		_CPU_ISR_Disable(level);
		if(exi->flags&EXI_FLAG_LOCKED) {
			exi->flags &= ~EXI_FLAG_LOCKED;
			__exi_setinterrupts(nChn,exi);
			if(exi->lck_cnt>0) {
				cb = exi->lck_dev[0].unlockcb;
				if((--exi->lck_cnt)>0) memmove(&(exi->lck_dev[0]),&(exi->lck_dev[1]),(exi->lck_cnt*sizeof(struct _lck_dev)));
				if(cb) cb(nChn,nDev);	
			}
		}
		_CPU_ISR_Restore(level);
	}
	
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		_CPU_ISR_Disable(level);
		if(exi->flags&EXI_FLAG_ATTACH) {
			if(exi->flags&EXI_FLAG_LOCKED && exi->lockeddev!=EXI_DEVICE_0) {
				exi->flags &= ~EXI_FLAG_ATTACH;
				__MaskIrq(((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI0_TC)|IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
			}
		}
		_CPU_ISR_Restore(level);
	}
	return 1;
}

EXICallback EXI_RegisterEXICallback(u32 nChn,EXICallback cb)
{
	u32 level;
	EXICallback old = NULL;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_RegisterEXICallback(%d,%p)\n",nChn,cb);
#endif
	_CPU_ISR_Disable(level);
	old = exi->CallbackEXI;
	exi->CallbackEXI = cb;
	if(nChn==EXI_CHANNEL_2) __exi_setinterrupts(EXI_CHANNEL_0,&eximap[EXI_CHANNEL_0]);
	else __exi_setinterrupts(nChn,exi);
	_CPU_ISR_Restore(level);
	return old;
}

u32 EXI_Probe(u32 nChn)
{
	if(_exiReg[nChn*5]&0x1000) 
		return 1;
	else
		return 0;
}

void __exi_init()
{
#ifdef _EXI_DEBUG
	printf("__exit_init(): init expansion system.\n");
#endif

	_exiReg[0] = 0;
	_exiReg[5] = 0;
	_exiReg[10] = 0;

	__MaskIrq(IM_EXI);
	
	memset(eximap,0,EXI_MAX_CHANNELS*sizeof(exibus_priv));
	memset(exiids,0,EXI_MAX_CHANNELS*sizeof(u32));

	IRQ_Request(IRQ_EXI0_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI0_TC,__tc_irq_handler,NULL);
	IRQ_Request(IRQ_EXI0_EXT,__ext_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_TC,__tc_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_EXT,__ext_irq_handler,NULL);
	IRQ_Request(IRQ_EXI2_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI2_TC,__tc_irq_handler,NULL);
}

void __exi_irq_handler(u32 nIrq,void *pCtx)
{
	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXI))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	exi = &eximap[chan];
	_exiReg[chan*5] = ((_exiReg[chan*5]&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ))|EXI_EXI_IRQ);

	if(!exi->CallbackEXI) return;
#ifdef _EXI_DEBUG
	printf("__exi_irq_handler(%p)\n",exi->CallbackEXI);
#endif
	exi->CallbackEXI(chan,dev);
}

void __tc_irq_handler(u32 nIrq,void *pCtx)
{
	u32 cnt,len,d,chan,dev;
	EXICallback tccb;
	void *buf = NULL;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_TC))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	exi = &eximap[chan];
	__MaskIrq(IRQMASK(nIrq));
	_exiReg[chan*5] = ((_exiReg[chan*5]&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ))|EXI_TC_IRQ);

	tccb = exi->CallbackTC;
#ifdef _EXI_DEBUG
	printf("__tc_irq_handler(%p)\n",tccb);
#endif
	if(!tccb) return;

	exi->CallbackTC = NULL;
	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		len = exi->imm_len;
		if(len) {
			buf = exi->imm_buff;
			d = _exiReg[chan*5+4]; 
			if(d>0) {
				for(cnt=0;cnt<len;cnt++) ((u8*)buf)[cnt] = (d>>((3-cnt)*8))&0xFF;
			}
		}
		exi->flags &= ~(EXI_FLAG_DMA|EXI_FLAG_IMM);
	}
	tccb(chan,dev);
}

void __ext_irq_handler(u32 nIrq,void *pCtx)
{

	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXT))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	__MaskIrq(IRQMASK(nIrq));

	exi = &eximap[chan];
	_exiReg[chan*5] = ((_exiReg[chan*5]&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ))|EXI_EXT_IRQ);
	
	exi->flags = (exi->flags&~EXI_FLAG_ATTACH);
	if(exi->CallbackEXT) exi->CallbackEXT(chan,dev);
#ifdef _EXI_DEBUG
	printf("__ext_irq_handler(%p)\n",exi->CallbackEXT);
#endif
}
