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

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))


struct _lck_dev {
	u32 dev;
	EXICallback unlockcb;
};

typedef struct _exibus_priv {
	EXICallback CallbackEXI;
	EXICallback CallbackTC;
	EXICallback CallbackEXT;

	u32 imm_len;
	void *imm_buff;
	u32 lockeddev;
	u32 flags;
	u32 lck_cnt;
	u32 exi_id;
	s64 exi_idtime;
	struct _lck_dev lck_dev[EXI_MAX_DEVICES];
} exibus_priv;

static exibus_priv eximap[EXI_MAX_CHANNELS];
static s64 last_exi_idtime[EXI_MAX_CHANNELS];

static void __exi_irq_handler(u32,void *);
static void __tc_irq_handler(u32,void *);
static void __ext_irq_handler(u32,void *);

static vu32* const _exiReg = (u32*)0xCC006800;

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern long long gettime();
extern u32 diff_usec(long long start,long long end);

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

static u32 __exi_probe(u32 nChn)
{
	s64 time;
	u32 level,ret = 1;
	s32 val;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("__exi_probe(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);
	val = _exiReg[nChn*5];
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(val&EXI_EXT_IRQ) {
			__exi_clearirqs(nChn,0,0,1);
			exi->exi_idtime = 0;
			last_exi_idtime[nChn] = 0;
		}
		if(_exiReg[nChn*5]&EXI_EXT_BIT) {
			time = gettime();
			if(last_exi_idtime[nChn]==0) last_exi_idtime[nChn] = time;
			if((val=diff_usec(last_exi_idtime[nChn],time)+10)>=30) ret = 1;
			else ret = 0;
#ifdef _EXI_DEBUG
			printf("val = %08x, last_exi_idtime[chn] = %08d%08d, ret = %d\n",val,(u32)(last_exi_idtime[nChn]>>32),(u32)last_exi_idtime[nChn],ret);
#endif
			_CPU_ISR_Restore(level);
			return ret;
		} else {
			exi->exi_idtime = 0;
			last_exi_idtime[nChn] = 0;
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	if(!(_exiReg[nChn*5]&EXI_EXT_BIT) || (_exiReg[nChn*5]&EXI_EXT_IRQ)) {
		exi->exi_idtime = 0;
		last_exi_idtime[nChn] = 0;
		ret = 0;
	}
	_CPU_ISR_Restore(level);
	return ret;
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

static __inline__ u32 __exi_attach(u32 nChn,EXICallback ext_cb)
{
	u32 level,ret;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("__exi_attach(%d,%p)\n",nChn,ext_cb);
#endif
	_CPU_ISR_Disable(level);
	ret = 0;
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(__exi_probe(nChn)==1) {
			__exi_clearirqs(nChn,1,0,0);
			exi->CallbackEXT = ext_cb;
			__UnmaskIrq(((IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
			exi->flags |= EXI_FLAG_ATTACH;
			ret = 1;
		}
	}
	_CPU_ISR_Restore(level);
	return ret;	
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
		if(!(exi->flags&EXI_FLAG_LOCKED) || exi->lockeddev!=nDev) {
#ifdef _EXI_DEBUG
			printf("EXI_Select(): not locked or wrong dev(%d).\n",exi->lockeddev);
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

u32 EXI_SelectSD(u32 nChn,u32 nDev,u32 nFrq)
{
	u32 val,id,ret;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_SelectSD(%d,%d,%d)\n",nChn,nDev,nFrq);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&EXI_FLAG_SELECT) {
#ifdef _EXI_DEBUG
		printf("EXI_SelectSD(): allready selected.\n");
#endif
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(nChn!=EXI_CHANNEL_2) {
		if(nDev==EXI_DEVICE_0 && !(exi->flags&EXI_FLAG_ATTACH)) {
			if((ret=__exi_probe(nChn))==1) {
				if(!exi->exi_idtime) ret = EXI_GetID(nChn,EXI_DEVICE_0,&id);
			}
			if(ret==0) {
				_CPU_ISR_Restore(level);
				return 0;
			}
		}
		if(!(exi->flags&EXI_FLAG_LOCKED) || exi->lockeddev!=nDev) {
#ifdef _EXI_DEBUG
			printf("EXI_SelectSD(): not locked or wrong dev(%d).\n",exi->lockeddev);
#endif
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	exi->flags |= EXI_FLAG_SELECT;
	val = _exiReg[nChn*5];
	val = (val&0x405)|(nFrq<<4);
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

u32 EXI_Imm(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Imm(%d,%p,%d,%d,%p)\n",nChn,pData,nLen,nMode,tc_cb);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->CallbackTC = tc_cb;
	if(tc_cb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq(IRQMASK((IRQ_EXI0_TC+(nChn*3))));
	}
	exi->flags |= EXI_FLAG_IMM;
	
	exi->imm_buff = pData;
	exi->imm_len = nLen;
	if(nMode!=EXI_READ) _exiReg[nChn*5+4] = *(u32*)pData;
	if(nMode==EXI_WRITE) exi->imm_len = 0;

	_exiReg[nChn*5+3] = (((nLen-1)&0x03)<<4)|((nMode&0x03)<<2)|0x01;
	
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

u32 EXI_Dma(u32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Dma(%d,%p,%d,%d,%p)\n",nChn,pData,nLen,nMode,tc_cb);
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
	printf("EXI_Dma(tccb: %p)\n",tc_cb);
#endif
	exi->CallbackTC = tc_cb;
	if(tc_cb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq((IRQMASK((IRQ_EXI0_TC+(nChn*3)))));
	}

	exi->imm_buff = NULL;
	exi->imm_len = 0;
	exi->flags |= EXI_FLAG_DMA;

	_exiReg[nChn*5+1] = (u32)pData&0x03FFFFE0;
	_exiReg[nChn*5+2] = nLen;
	_exiReg[nChn*5+3] = ((nMode&0x03)<<2)|0x03;
	
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
#ifdef _EXI_DEBUG
	printf("__unlocked_handler(%d,%d)\n",nChn,nDev);
#endif
	EXI_GetID(nChn,nDev,&nId);
	return 1;
}

u32 EXI_GetID(u32 nChn,u32 nDev,u32 *nId)
{
	s64 idtime = 0;
	u32 ret,lck,reg,level;
	exibus_priv *exi = &eximap[nChn];

#ifdef _EXI_DEBUG
	printf("EXI_GetID(exi_buscode = %d)\n",exi->exi_buscode);
#endif
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		if(__exi_probe(nChn)==0) return 0;
		if(exi->exi_idtime==last_exi_idtime[nChn]) {
#ifdef _EXI_DEBUG
			printf("EXI_GetID(exi_id = %d)\n",exi->exi_id);
#endif
			*nId = exi->exi_id;
			return 1;
		}
#ifdef _EXI_DEBUG
		printf("EXI_GetID(setting interrupts,%08x)\n",exi->flags);
#endif
		if(__exi_attach(nChn,NULL)==0) return 0;
		idtime = last_exi_idtime[nChn];
	}
#ifdef _EXI_DEBUG
	printf("EXI_GetID(interrupts set)\n");
#endif
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
			EXI_Unlock(nChn);
		}
	}
	
	ret = 0;
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		EXI_Detach(nChn);
		
		_CPU_ISR_Disable(level);
		if(idtime==last_exi_idtime[nChn]) {
			exi->exi_idtime = idtime;
			exi->exi_id = *nId;
			ret = 1;
		}
		_CPU_ISR_Restore(level);
#ifdef _EXI_DEBUG
		printf("EXI_GetID(exi_id = %d)\n",exi->exi_id);
#endif
	}
	return ret;
}

u32 EXI_Attach(u32 nChn,EXICallback ext_cb)
{
	u32 level,ret;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Attach(%d)\n",nChn);
#endif
	EXI_Probe(nChn);

	_CPU_ISR_Disable(level);
	if(exi->exi_idtime) {
		ret = __exi_attach(nChn,ext_cb);
	} else
		ret = 0;
	_CPU_ISR_Restore(level);
	return ret;	
}

u32 EXI_Detach(u32 nChn)
{
	u32 level,ret = 1;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Detach(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);
	if(exi->flags&EXI_FLAG_ATTACH) {
		if(exi->flags&EXI_FLAG_LOCKED && exi->lockeddev!=EXI_DEVICE_0) ret = 0;
		else {
			exi->flags &= ~EXI_FLAG_ATTACH;
			__MaskIrq(((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI0_TC)|IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

EXICallback EXI_RegisterEXICallback(u32 nChn,EXICallback exi_cb)
{
	u32 level;
	EXICallback old = NULL;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_RegisterEXICallback(%d,%p)\n",nChn,exi_cb);
#endif
	_CPU_ISR_Disable(level);
	old = exi->CallbackEXI;
	exi->CallbackEXI = exi_cb;
	if(nChn==EXI_CHANNEL_2) __exi_setinterrupts(EXI_CHANNEL_0,&eximap[EXI_CHANNEL_0]);
	else __exi_setinterrupts(nChn,exi);
	_CPU_ISR_Restore(level);
	return old;
}

u32 EXI_Probe(u32 nChn)
{
	u32 id,ret;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Probe(%d)\n",nChn);
#endif
	if((ret=__exi_probe(nChn))==1) {
		if(exi->exi_idtime==0) {
			if(EXI_GetID(nChn,EXI_DEVICE_0,&id)==0) ret = 0;
		}
	}
	return ret;
}

u32 EXI_ProbeEx(u32 nChn)
{
	if(EXI_Probe(nChn)==1) return 1;
	if(last_exi_idtime[nChn]==0) return -1;
	return 0;
}

u32 EXI_ProbeReset()
{
	exibus_priv *exi = NULL;

	last_exi_idtime[0] = 0;
	last_exi_idtime[1] = 0;

	exi = &eximap[0];
	exi->exi_idtime = 0;
	exi = &eximap[1];
	exi->exi_idtime = 0;

	__exi_probe(0);
	__exi_probe(1);

	return 1;
}

void __exi_init()
{
#ifdef _EXI_DEBUG
	printf("__exi_init(): init expansion system.\n");
#endif
	__MaskIrq(IM_EXI);

	_exiReg[0] = 0;
	_exiReg[5] = 0;
	_exiReg[10] = 0;

	memset(eximap,0,EXI_MAX_CHANNELS*sizeof(exibus_priv));

	IRQ_Request(IRQ_EXI0_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI0_TC,__tc_irq_handler,NULL);
	IRQ_Request(IRQ_EXI0_EXT,__ext_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_TC,__tc_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_EXT,__ext_irq_handler,NULL);
	IRQ_Request(IRQ_EXI2_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI2_TC,__tc_irq_handler,NULL);

	EXI_ProbeReset();
}

void __exi_irq_handler(u32 nIrq,void *pCtx)
{
	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXI))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	exi = &eximap[chan];
	__exi_clearirqs(chan,1,0,0);

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
	__exi_clearirqs(chan,0,1,0);

	tccb = exi->CallbackTC;
#ifdef _EXI_DEBUG
	printf("__tc_irq_handler(%p)\n",tccb);
#endif
	if(!tccb) return;

	exi->CallbackTC = NULL;
	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		if(exi->flags&EXI_FLAG_IMM) {
			len = exi->imm_len;
			buf = exi->imm_buff;
			if(len>0 && buf) {
				d = _exiReg[chan*5+4]; 
				if(d>0) {
					for(cnt=0;cnt<len;cnt++) ((u8*)buf)[cnt] = (d>>((3-cnt)*8))&0xFF;
				}
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

	exi = &eximap[chan];
	__MaskIrq(IRQMASK(nIrq));
	__exi_clearirqs(chan,0,0,1);
	
	exi->flags &= ~EXI_FLAG_ATTACH;
	if(exi->CallbackEXT) exi->CallbackEXT(chan,dev);
#ifdef _EXI_DEBUG
	printf("__ext_irq_handler(%p)\n",exi->CallbackEXT);
#endif
}
