#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "lwp.h"
#include "system.h"
#include "video.h"
#include "video_types.h"
#include "lwp_watchdog.h"
#include "gx.h"

//#define _GP_DEBUG

#define GX_FINISH		2
#define BIG_NUMBER		(1024*1024)
#define WGPIPE			(0xCC008000)

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

#define FIFO_PUTU8(x)	*(vu8*)WGPIPE = (u8)(x)
#define FIFO_PUTS8(x)	*(vs8*)WGPIPE = (s8)(x)
#define FIFO_PUTU16(x)	*(vu16*)WGPIPE = (u16)(x)
#define FIFO_PUTS16(x)	*(vs16*)WGPIPE = (s16)(x)
#define FIFO_PUTU32(x)	*(vu32*)WGPIPE = (u32)(x)
#define FIFO_PUTS32(x)	*(vs32*)WGPIPE = (s32)(x)
#define FIFO_PUTF32(x)	*(vf32*)WGPIPE = (f32)(x)

#define GX_LOAD_BP_REG(x)				\
	do {								\
		FIFO_PUTU8(0x61);				\
		FIFO_PUTU32(x);					\
	} while(0)

#define GX_LOAD_CP_REG(x, y)			\
	do {								\
		FIFO_PUTU8(0x08);				\
		FIFO_PUTU8(x);					\
		FIFO_PUTU32(y);					\
	} while(0)

#define GX_LOAD_XF_REG(x, y)			\
	do {								\
		FIFO_PUTU8(0x10);				\
		FIFO_PUTU16(0);					\
		FIFO_PUTU16(x);					\
		FIFO_PUTU32(y);					\
	} while(0)

#define GX_LOAD_XF_REGS(x, n)			\
	do {								\
		FIFO_PUTU8(0x10);				\
		FIFO_PUTU16(n-1);				\
		FIFO_PUTU16(x);					\
	} while(0)

#define XY(x, y)   (((y) << 10) | (x))

#define GX_DEFAULT_BG	{64,64,64,255}
#define BLACK			{0,0,0,0}
#define WHITE			{255,255,255,255}

struct __gxfifo {
	u32 buf_start;
	u32 buf_end;
	u32 size;
	u32 hi_mark;
	u32 lo_mark;
	u32 rd_ptr;
	u32 wt_ptr;
	u32 rdwt_dst;
	u8 pad0[96];
};

static void *_gxcurrbp = NULL;
static lwp_t _gxcurrentlwp = LWP_THREAD_NULL;

static GXFifoObj _gxdefiniobj;

static u8 _cpgplinked = 0;
static u16 _gxgpstatus = 0;
static vu32 _gxoverflowsuspend = 0;
static vu32 _gxoverflowcount = 0;
static vu32 _gxfinished = 0;
static lwpq_t _gxwaitfinish;

static struct __gxfifo *_gpfifo = NULL;
static struct __gxfifo *_cpufifo = NULL;
static GXFifoObj *_gxoldcpufifo = NULL;
static GXBreakPtCallback breakPtCB = NULL;
static GXDrawDoneCallback drawDoneCB = NULL;
static GXDrawSyncCallback tokenCB = NULL;

static GXTexRegionCallback regionCB = NULL;
static GXTlutRegionCallback tlut_regionCB = NULL;

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _cpReg = (u16*)0xCC000000;
static vu16* const _peReg = (u16*)0xCC001000;
static vu16* const _memReg = (u16*)0xCC004000;

static u8 _gxtevcolid[9] = {0,1,0,1,0,1,7,5,6};
static u8 _gxtexmode0ids[8] = {0x80,0x81,0x82,0x83,0xA0,0xA1,0xA2,0xA3};
static u8 _gxtexmode1ids[8] = {0x84,0x85,0x86,0x87,0xA4,0xA5,0xA6,0xA7};
static u8 _gxteximg0ids[8] = {0x88,0x89,0x8A,0x8B,0xA8,0xA9,0xAA,0xAB};
static u8 _gxteximg1ids[8] = {0x8C,0x8D,0x8E,0x8F,0xAC,0xAD,0xAE,0xAF};
static u8 _gxteximg2ids[8] = {0x90,0x91,0x92,0x93,0xB0,0xB1,0xB2,0xB3};
static u8 _gxteximg3ids[8] = {0x94,0x95,0x96,0x97,0xB4,0xB5,0xB6,0xB7};
static u8 _gxtextlutids[8] = {0x98,0x99,0x9A,0x9B,0xB8,0xB9,0xBA,0xBB};

struct __gxfifo _gx_dl_fifo;
u8 _gx_saved_data[1280];

extern u8 __gxregs[];
static u32 *_gx = (u32*)__gxregs;

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);
extern long long gettime();

#ifdef _GP_DEBUG
extern int printk(const char *fmt,...);
#endif

static __inline__ s32 IsWriteGatherBufferEmpty()
{
	_sync();
	return (mfwpar()&0x0001);
}

static __inline__ void DisableWriteGatherPipe()
{
	mtspr(920,(mfspr(920)&~0x40000000));
}

static __inline__ void EnableWriteGatherPipe()
{
	mtwpar(0x0C008000);
	mtspr(920,(mfspr(920)|0x40000000));
}

static __inline__ void __GX_FifoLink(u8 enable)
{
	((u16*)_gx)[1] = ((((u16*)_gx)[1]&~0x10)|(_SHIFTL(enable,4,1)));
	_cpReg[1] = ((u16*)_gx)[1];
}

static __inline__ void __GX_WriteFifoIntReset(u8 inthi,u8 intlo)
{
	((u16*)_gx)[2] = ((((u16*)_gx)[2]&~0x03)|(_SHIFTL(intlo,1,1))|(inthi&1));
	_cpReg[2] = ((u16*)_gx)[2];
}

static __inline__ void __GX_WriteFifoIntEnable(u8 inthi, u8 intlo)
{
	((u16*)_gx)[1] = ((((u16*)_gx)[1]&~0x0C)|(_SHIFTL(intlo,3,1))|(_SHIFTL(inthi,2,1)));
	_cpReg[1] = ((u16*)_gx)[1];
}

static __inline__ void __GX_FifoReadEnable()
{
	((u16*)_gx)[1] = ((((u16*)_gx)[1]&~0x01)|1);
	_cpReg[1] = ((u16*)_gx)[1];
}

static __inline__ void __GX_FifoReadDisable()
{
	((u16*)_gx)[1] = ((((u16*)_gx)[1]&~0x01)|0);
	_cpReg[1] = ((u16*)_gx)[1];
}

static void __GX_SaveCPUFifoAux(struct __gxfifo *fifo)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GX_Flush();
	
	fifo->buf_start = (u32)MEM_PHYSICAL_TO_K0(_piReg[0x03]);
	fifo->buf_end = (u32)MEM_PHYSICAL_TO_K0(_piReg[0x04]);
	fifo->wt_ptr = (u32)MEM_PHYSICAL_TO_K0(((_piReg[0x05]&~0x04000000)));

	if(_cpgplinked) {
		fifo->rd_ptr = (u32)MEM_PHYSICAL_TO_K0(_SHIFTL(_cpReg[29],16,16)|(_cpReg[28]&0xffff));
		fifo->rdwt_dst = (_SHIFTL(_cpReg[25],16,16)|(_cpReg[24]&0xffff));
	} else {
		fifo->rdwt_dst = (fifo->wt_ptr - fifo->rd_ptr);
		if(fifo->rdwt_dst<0) fifo->rdwt_dst = (fifo->rdwt_dst + fifo->size);
	}
	
	_CPU_ISR_Restore(level);
}

static void __GX_CleanGPFifo()
{
	u32 level,buf_start;
	struct __gxfifo tmp_fifo;
	struct __gxfifo *gpfifo;
	struct __gxfifo *cpufifo;

	gpfifo = (struct __gxfifo*)GX_GetGPFifo();
	cpufifo = (struct __gxfifo*)GX_GetCPUFifo();

	buf_start = gpfifo->buf_start;
	memcpy(&tmp_fifo,gpfifo,sizeof(struct __gxfifo));
	
	_CPU_ISR_Disable(level);
	tmp_fifo.rd_ptr = buf_start;
	tmp_fifo.wt_ptr = buf_start;
	tmp_fifo.rdwt_dst = 0;
	_CPU_ISR_Restore(level);

	GX_SetGPFifo((GXFifoObj*)&tmp_fifo);
	if(gpfifo==cpufifo) GX_SetCPUFifo((GXFifoObj*)&tmp_fifo);
	
	_CPU_ISR_Disable(level);
	gpfifo->rd_ptr = buf_start;
	gpfifo->wt_ptr = buf_start;
	gpfifo->rdwt_dst = 0;
	_CPU_ISR_Restore(level);
	
	GX_SetGPFifo((GXFifoObj*)gpfifo);
	if(gpfifo==cpufifo) GX_SetCPUFifo((GXFifoObj*)cpufifo);
}

static void __GXOverflowHandler()
{
	if(!_gxoverflowsuspend) {
		_gxoverflowsuspend = 1;
		_gxoverflowcount++;
		__GX_WriteFifoIntEnable(GX_DISABLE,GX_ENABLE);
		__GX_WriteFifoIntReset(GX_TRUE,GX_FALSE);
		LWP_SuspendThread(_gxcurrentlwp);
	}
}

static void __GXUnderflowHandler()
{	
	if(_gxoverflowsuspend) {
		_gxoverflowsuspend = 0;
		LWP_ResumeThread(_gxcurrentlwp);
		__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
	}
}

static void __GXCPInterruptHandler(u32 irq,void *ctx)
{
	((u16*)_gx)[0] = _cpReg[0];

	if((((u16*)_gx)[1]&0x08) && (((u16*)_gx)[0]&0x02)) 
		__GXUnderflowHandler();

	if((((u16*)_gx)[1]&0x04) && (((u16*)_gx)[0]&0x01))
		__GXOverflowHandler();

	if((((u16*)_gx)[1]&0x20) && (((u16*)_gx)[0]&0x10)) {
		((u16*)_gx)[1] &= ~0x20;
		_cpReg[1] = ((u16*)_gx)[1];
		if(breakPtCB)
			breakPtCB();
	}
}

static void __GXTokenInterruptHandler(u32 irq,void *ctx)
{
	u16 token = _peReg[7];
	
	if(tokenCB)
		tokenCB(token);
	
	_peReg[5] = (_peReg[5]&~0x04)|0x04;	
}

static void __GXFinishInterruptHandler(u32 irq,void *ctx)
{
#ifdef _GP_DEBUG
	printf("__GXFinishInterruptHandler()\n");
#endif
	_peReg[5] = (_peReg[5]&~0x08)|0x08;	
	_gxfinished = 1;

	if(drawDoneCB)
		drawDoneCB();
	
	LWP_ThreadBroadcast(_gxwaitfinish);
}

static void __GX_PEInit()
{
	IRQ_Request(IRQ_PI_PETOKEN,__GXTokenInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_PETOKEN));

	IRQ_Request(IRQ_PI_PEFINISH,__GXFinishInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_PEFINISH));

	_peReg[5] = 0x0F;
}

static void __GX_FifoInit()
{
	IRQ_Request(IRQ_PI_CP,__GXCPInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_CP));

	_gxoverflowsuspend = 0;
	_cpufifo = NULL;
	_gpfifo = NULL;
	_gxcurrentlwp = LWP_GetSelf();
}

static void __GX_SetTmemConfig(u8 nr)
{
	if(nr==1) {
		//  Set_TextureImage0-3, GXTexMapID=0-3 tmem_offset=00000000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0x8c0d8000);
		GX_LOAD_BP_REG(0x900dc000);
		GX_LOAD_BP_REG(0x8d0d8800);
		GX_LOAD_BP_REG(0x910dc800);
		GX_LOAD_BP_REG(0x8e0d9000);
		GX_LOAD_BP_REG(0x920dd000);
		GX_LOAD_BP_REG(0x8f0d9800);
		GX_LOAD_BP_REG(0x930dd800);

		//  Set_TextureImage0-3, GXTexMapID=4-7 tmem_offset=00010000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0xac0da000);
		GX_LOAD_BP_REG(0xb00de000);
		GX_LOAD_BP_REG(0xad0da800);
		GX_LOAD_BP_REG(0xb10de800);
		GX_LOAD_BP_REG(0xae0db000);
		GX_LOAD_BP_REG(0xb20df000);
		GX_LOAD_BP_REG(0xaf0db800);
		GX_LOAD_BP_REG(0xb30df800);

		return;
	}

	//  Set_TextureImage0-3, GXTexMapID=0-3 tmem_offset=00000000, cache_width=32 kb, cache_height=32 kb, image_type=cached
	GX_LOAD_BP_REG(0x8c0d8000);
	GX_LOAD_BP_REG(0x900dc000);
	GX_LOAD_BP_REG(0x8d0d8400);
	GX_LOAD_BP_REG(0x910dc400);
	GX_LOAD_BP_REG(0x8e0d8800);
	GX_LOAD_BP_REG(0x920dc800);
	GX_LOAD_BP_REG(0x8f0d8c00);
	GX_LOAD_BP_REG(0x930dcc00);

	//  Set_TextureImage0-3, GXTexMapID=4-7 tmem_offset=00010000, cache_width=32 kb, cache_height=32 kb, image_type=cached
	GX_LOAD_BP_REG(0xac0d9000);
	GX_LOAD_BP_REG(0xb00dd000);
	GX_LOAD_BP_REG(0xad0d9400);
	GX_LOAD_BP_REG(0xb10dd400);
	GX_LOAD_BP_REG(0xae0d9800);
	GX_LOAD_BP_REG(0xb20dd800);
	GX_LOAD_BP_REG(0xaf0d9c00);
	GX_LOAD_BP_REG(0xb30ddc00);
}

static GXTexRegion* __GXDefRegionCallback(GXTexObj *obj,u8 mapid)
{
	u8 fmt;
	u32 idx;
	static u32 regionA = 0;
	static u32 regionB = 0;
	GXTexRegion *ret = NULL;

	fmt = GX_GetTexFmt(obj);
	if(fmt==0x0008 || fmt==0x0009 || fmt==0x000a) {
		idx = regionB++;
		ret = (GXTexRegion*)(&_gx[0x120+((idx&0x3)*(sizeof(GXTexRegion)>>2))]);
	} else {
		idx = regionA++;
		ret = (GXTexRegion*)(&_gx[0x100+((idx&0x7)*(sizeof(GXTexRegion)>>2))]);
	}
	return ret;
}

static GXTlutRegion* __GXDefTlutRegionCallback(u32 tlut_name)
{
	return (GXTlutRegion*)(&_gx[0x150+(tlut_name*(sizeof(GXTlutRegion)>>2))]);
}

static void __GX_InitGX()
{
	s32 i;
	u32 flag;
	GXRModeObj *rmode;
	Mtx identity_matrix = 
	{
		{1,0,0,0},
		{0,1,0,0},
		{0,0,1,0}
	};

	switch(VIDEO_GetCurrentTvMode()) {
		case VI_NTSC:
			rmode = &TVNtsc480IntDf;
			break;
		case VI_PAL:
			rmode = &TVPal528IntDf;
			break;
		case VI_MPAL:
			rmode = &TVMpal480IntDf;
			break;
		default:
			rmode = &TVNtsc480IntDf;
			break;
	}
	
	GX_SetCopyClear((GXColor)BLACK,0xffffff);
	GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,GX_TG_TEX1,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD2,GX_TG_MTX2x4,GX_TG_TEX2,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD3,GX_TG_MTX2x4,GX_TG_TEX3,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD4,GX_TG_MTX2x4,GX_TG_TEX4,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD5,GX_TG_MTX2x4,GX_TG_TEX5,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD6,GX_TG_MTX2x4,GX_TG_TEX6,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD7,GX_TG_MTX2x4,GX_TG_TEX7,GX_IDENTITY);
	GX_SetNumTexGens(1);
	GX_ClearVtxDesc();
	GX_InvVtxCache();
	
	GX_SetLineWidth(6,GX_TO_ZERO);
	GX_SetPointSize(6,GX_TO_ZERO);

	GX_EnableTexOffsets(GX_TEXCOORD0,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD1,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD2,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD3,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD4,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD5,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD6,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD7,GX_DISABLE,GX_DISABLE);

	GX_LoadPosMtxImm(identity_matrix,GX_PNMTX0);
	GX_LoadNrmMtxImm(identity_matrix,GX_PNMTX0);
	GX_SetCurrentMtx(GX_PNMTX0);
	GX_LoadTexMtxImm(identity_matrix,GX_IDENTITY,GX_MTX3x4);
	GX_LoadTexMtxImm(identity_matrix,GX_DTTIDENTITY,GX_MTX3x4);

	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetCullMode(GX_CULL_BACK);
	GX_SetClipMode(GX_CLIP_DISABLE);

	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetScissorBoxOffset(0,0);

	GX_SetNumChans(0);

	GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
	GX_SetChanAmbColor(GX_COLOR0A0,(GXColor)BLACK);
	GX_SetChanMatColor(GX_COLOR0A0,(GXColor)WHITE);
	
	GX_SetChanCtrl(GX_COLOR1A1,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
	GX_SetChanAmbColor(GX_COLOR1A1,(GXColor)BLACK);
	GX_SetChanMatColor(GX_COLOR1A1,(GXColor)WHITE);

	GX_InvalidateTexAll();
	GX_SetTexRegionCallback(__GXDefRegionCallback);
	GX_SetTlutRegionCallback(__GXDefTlutRegionCallback);

	GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE1,GX_TEXCOORD1,GX_TEXMAP1,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE2,GX_TEXCOORD2,GX_TEXMAP2,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE3,GX_TEXCOORD3,GX_TEXMAP3,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE4,GX_TEXCOORD4,GX_TEXMAP4,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE5,GX_TEXCOORD5,GX_TEXMAP5,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE6,GX_TEXCOORD6,GX_TEXMAP6,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE7,GX_TEXCOORD7,GX_TEXMAP7,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE8,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE9,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE10,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE11,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE12,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE13,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE14,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE15,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetNumTevStages(1);
	GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	for(i=0;i<GX_MAX_TEVSTAGE;i++) {
		GX_SetTevKColorSel(i,GX_TEV_KCSEL_1_4);
		GX_SetTevKAlphaSel(i,GX_TEV_KASEL_1);
		GX_SetTevSwapMode(i,GX_TEV_SWAP0,GX_TEV_SWAP0);
	}

	GX_SetTevSwapModeTable(GX_TEV_SWAP0,GX_CH_RED,GX_CH_GREEN,GX_CH_BLUE,GX_CH_ALPHA);
	GX_SetTevSwapModeTable(GX_TEV_SWAP1,GX_CH_RED,GX_CH_RED,GX_CH_RED,GX_CH_ALPHA);
	GX_SetTevSwapModeTable(GX_TEV_SWAP2,GX_CH_GREEN,GX_CH_GREEN,GX_CH_GREEN,GX_CH_ALPHA);
	GX_SetTevSwapModeTable(GX_TEV_SWAP3,GX_CH_BLUE,GX_CH_BLUE,GX_CH_BLUE,GX_CH_ALPHA);
	for(i=0;i<GX_MAX_TEVSTAGE;i++) {
		GX_SetTevDirect(i);
	}

	GX_SetNumIndStages(0);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE0,GX_ITS_1,GX_ITS_1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE1,GX_ITS_1,GX_ITS_1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE2,GX_ITS_1,GX_ITS_1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE3,GX_ITS_1,GX_ITS_1);

	GX_SetFog(GX_FOG_NONE,0,1,0.1,1,(GXColor)BLACK);
	GX_SetFogRangeAdj(GX_DISABLE,0,NULL);

	GX_SetBlendMode(GX_BM_NONE,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetZMode(GX_ENABLE,GX_LEQUAL,GX_TRUE);
	GX_SetZCompLoc(GX_TRUE);
	GX_SetDither(GX_ENABLE);
	GX_SetDstAlpha(GX_DISABLE,0);
	GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);

	GX_SetFieldMask(GX_ENABLE,GX_ENABLE);
	
	flag = 0;
	if(rmode->viHeight==(rmode->xfbHeight<<1)) flag = 1;
	GX_SetFieldMode(rmode->field_rendering,flag);

	GX_SetCopyClear((GXColor)GX_DEFAULT_BG,0x00ffffff);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyYScale(1.0);
	GX_SetCopyClamp(GX_CLAMP_TOP|GX_CLAMP_BOTTOM);
	GX_SetCopyFilter(GX_FALSE,NULL,GX_FALSE,NULL);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetDispCopyFrame2Field(GX_COPY_PROGRESSIVE);
	GX_ClearBoundingBox();
	
	GX_PokeColorUpdate(GX_TRUE);
	GX_PokeAlphaUpdate(GX_TRUE);
	GX_PokeDither(GX_FALSE);
	GX_PokeBlendMode(GX_BM_NONE,GX_BL_ZERO,GX_BL_ONE,GX_LO_SET);
	GX_PokeAlphaMode(GX_ALWAYS,0);
	GX_PokeAlphaRead(GX_READ_FF);
	GX_PokeDstAlpha(GX_DISABLE,0);
	GX_PokeZMode(GX_TRUE,GX_ALWAYS,GX_TRUE);

	GX_SetGPMetric(GX_PERF0_NONE,GX_PERF1_NONE);
	GX_ClearGPMetric();
}

static void __GX_FlushTextureState()
{
	GX_LOAD_BP_REG(_gx[0xaf]);
}

static void __GX_XfVtxSpecs()
{
	u32 xfvtxspecs = 0;
	u32 nrms,texs,cols;

	cols = 0;
	if(_gx[0x06]&0x6000) cols++;
	if(_gx[0x06]&0x18000) cols++;

	nrms = 0;
	if(_gx[0x08]==1) nrms = 1;
	else if(_gx[0x08]==2) nrms = 2;
	
	texs = 0;
	if(_gx[0x07]&0x3) texs++;
	if(_gx[0x07]&0xc) texs++;
	if(_gx[0x07]&0x30) texs++;
	if(_gx[0x07]&0xc0) texs++;
	if(_gx[0x07]&0x300) texs++;
	if(_gx[0x07]&0xc00) texs++;
	if(_gx[0x07]&0x3000) texs++;
	if(_gx[0x07]&0xc000) texs++;
	
	xfvtxspecs = (_SHIFTL(texs,4,4))|(_SHIFTL(nrms,2,2))|(cols&0x3);
	GX_LOAD_XF_REG(0x1008,xfvtxspecs);
}

static void __GX_SetMatrixIndex(u32 mtx)
{
	if(mtx<5) {
		GX_LOAD_CP_REG(0x30,_gx[0x03]);
		GX_LOAD_XF_REG(0x1018,_gx[0x03]);
	} else {
		GX_LOAD_CP_REG(0x40,_gx[0x04]);
		GX_LOAD_XF_REG(0x1019,_gx[0x04]);
	}
}

static void __GX_SendFlushPrim()
{
	u32 tmp,tmp2,cnt;

	tmp = ((u16*)_gx)[30]*((u16*)_gx)[31];

	FIFO_PUTU8(0x98);
	FIFO_PUTU16(((u16*)_gx)[30]);

	tmp2 = (tmp+3)/4;
	if(tmp>0) {
		cnt = tmp2/8;
		while(cnt) {
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			FIFO_PUTU32(0);
			cnt--;
		}
		tmp2 &= 0x0007;
		if(tmp2) {
			while(tmp2) {
				FIFO_PUTU32(0);
				tmp2--;
			}
		}
	}
	((u16*)_gx)[29] = 1;
}

static void __GX_SetVCD()
{
	GX_LOAD_CP_REG(0x50,_gx[0x06]);
	GX_LOAD_CP_REG(0x60,_gx[0x07]);
	__GX_XfVtxSpecs();
}

static void __GX_SetVAT()
{
	u8 setvtx = 0;
	s32 i;

	for(i=0;i<8;i++) {
		setvtx = (1<<i);
		if(_gx[0x02]&setvtx) {
			GX_LOAD_CP_REG((0x70|(i&7)),_gx[0x10+i]);
			GX_LOAD_CP_REG((0x80|(i&7)),_gx[0x18+i]);
			GX_LOAD_CP_REG((0x90|(i&7)),_gx[0x20+i]);
		}
	}
	_gx[0x02] = 0;
}

static void __SetSURegs(u8 texmap,u8 texcoord)
{
	u16 wd,ht;
	u8 wrap_s,wrap_t;
	u32 regA,regB;

	wd = _gx[0x40+texmap]&0x3ff;
	ht = _SHIFTR(_gx[0x40+texmap],10,10);
	wrap_s = _gx[0x50+texmap]&3;
	wrap_t = _SHIFTR(_gx[0x50+texmap],2,2);
	
	regA = 0xa0+(texcoord&0x7);
	regB = 0xb0+(texcoord&0x7);
	_gx[regA] = (_gx[regA]&~0x0000ffff)|wd;
	_gx[regB] = (_gx[regB]&~0x0000ffff)|ht;
	_gx[regA] = (_gx[regA]&~0x00010000)|(_SHIFTL(wrap_s,16,1));
	_gx[regB] = (_gx[regB]&~0x00010000)|(_SHIFTL(wrap_t,16,1));

	GX_LOAD_BP_REG(_gx[regA]);
	GX_LOAD_BP_REG(_gx[regB]);
}

static void __GX_SetSUTexRegs()
{
	u32 i;
	u32 indtev,dirtev;
	u8 texcoord,texmap;
	u32 tevreg,tevm,texcm;

	dirtev = (_SHIFTR(_gx[0xac],10,4))+1;
	indtev = _SHIFTR(_gx[0xac],16,3);

	//indirect texture order
	for(i=0;i<indtev;i++) {
		switch(i) {
			case GX_INDTEXSTAGE0:
				texmap = _gx[0xc2]&7;
				texcoord = _SHIFTR(_gx[0xc2],3,3);
				break;
			case GX_INDTEXSTAGE1:
				texmap = _SHIFTR(_gx[0xc2],6,3);
				texcoord = _SHIFTR(_gx[0xc2],9,3);
				break;
			case GX_INDTEXSTAGE2:
				texmap = _SHIFTR(_gx[0xc2],12,3);
				texcoord = _SHIFTR(_gx[0xc2],15,3);
				break;
			case GX_INDTEXSTAGE3:
				texmap = _SHIFTR(_gx[0xc2],18,3);
				texcoord = _SHIFTR(_gx[0xc2],21,3);
				break;
			default:
				texmap = 0;
				texcoord = 0;
				break;
		}

		texcm = _SHIFTL(1,texcoord,1); 
		if(!(_gx[0x05]&texcm))
			__SetSURegs(texmap,texcoord);
	}

	//direct texture order
	for(i=0;i<dirtev;i++) {
		tevreg = 0xc3+(_SHIFTR(i,1,3));
		texmap = (_gx[0x30+i]&0xff);

		if(i&1) texcoord = _SHIFTR(_gx[tevreg],15,3);
		else texcoord = _SHIFTR(_gx[tevreg],3,3);
		
		tevm = _SHIFTL(1,i,1);
		texcm = _SHIFTL(1,texcoord,1);
		if(texmap!=0xff && (_gx[0x0a]&tevm) && !(_gx[0x05]&texcm)) {
			__SetSURegs(texmap,texcoord);
		}
	}
}

static void __GX_SetGenMode()
{
	GX_LOAD_BP_REG(_gx[0xac]);
}

static void __GX_UpdateBPMask()
{
	u32 i;
	u32 nbmp,nres;
	u8 ntexmap;

	nbmp = _SHIFTR(_gx[0xac],16,3);

	nres = 0;
	for(i=0;i<nbmp;i++) {
		switch(i) {
			case GX_INDTEXSTAGE0:
				ntexmap = _gx[0xc2]&7;
				break;
			case GX_INDTEXSTAGE1:
				ntexmap = _SHIFTR(_gx[0xc2],6,3);
				break;
			case GX_INDTEXSTAGE2:
				ntexmap = _SHIFTR(_gx[0xc2],12,3);
				break;
			case GX_INDTEXSTAGE3:
				ntexmap = _SHIFTR(_gx[0xc2],18,3);
				break;
			default:
				ntexmap = 0;
				break;
		}
		nres |= (1<<ntexmap);
	}

	if((_gx[0xaf]&0xff)!=nres) {
		_gx[0xaf] = (_gx[0xaf]&~0xff)|(nres&0xff);
		GX_LOAD_BP_REG(_gx[0xaf]);
	}
}

static void __GX_SetDirtyState()
{
	if(_gx[0x09]&0x0001) {
		__GX_SetSUTexRegs();
	}
	if(_gx[0x09]&0x0002) {
		__GX_UpdateBPMask();
	}
	if(_gx[0x09]&0x0004) {
		__GX_SetGenMode();
	}
	if(_gx[0x09]&0x0008) {
		__GX_SetVCD();
	}
	if(_gx[0x09]&0x0010) {
		__GX_SetVAT();
	}
	_gx[0x09] = 0;
}

static u32 __GX_GetNumXfbLines(u16 efbHeight,u32 yscale)
{
	u32 tmp,tmp1;

	tmp = (((efbHeight-1)<<8)/yscale)+1;
	if(yscale>128 && yscale<256) {
		while(yscale&0x01) yscale >>= 1;
		tmp1 = yscale*(efbHeight/yscale);
		if(!(efbHeight-tmp1)) tmp++;
	}
	if(tmp>1024) tmp = 1024;
	
	return tmp;
}

GXFifoObj* GX_Init(void *base,u32 size)
{
	s32 i,re0,re1,addr;
	u32 tmem_even,tmem_odd;
	u32 divis,res;
	u32 divid = 162000000;
	GXTexRegion *region = NULL;
	GXTlutRegion *tregion = NULL;

	LWP_InitQueue(&_gxwaitfinish);

	memset(_gx,0,1280);

	__GX_FifoInit();
	GX_InitFifoBase(&_gxdefiniobj,base,size);
	GX_SetCPUFifo(&_gxdefiniobj);
	GX_SetGPFifo(&_gxdefiniobj);
	__GX_PEInit();
	EnableWriteGatherPipe();

	_gx[0xaf] = 0xff;
	_gx[0xaf] = (_gx[0xaf]&~0xff000000)|(_SHIFTL(0x0f,24,8));

	i=0;
	re0 = 0xc0;
	re1 = 0xc1;
	while(i<16) {
		addr = 0x80+i;
		_gx[addr] = (_gx[addr]&~0xff000000)|(_SHIFTL(re0,24,8));
		addr = 0x90+i;
		_gx[addr] = (_gx[addr]&~0xff000000)|(_SHIFTL(re1,24,8));
		re0 += 2; re1 += 2; i++;
	}
	
	_gx[0x05] = 0;
	_gx[0x09] = 0;

	((u8*)_gx)[0x371] = 1;
	((u8*)_gx)[0x372] = 0;
	
	_gx[0xa8] = (_gx[0xa8]&~0xff000000)|(_SHIFTL(0x20,24,8));
	_gx[0xa9] = (_gx[0xa9]&~0xff000000)|(_SHIFTL(0x21,24,8));
	_gx[0xaa] = (_gx[0xaa]&~0xff000000)|(_SHIFTL(0x22,24,8));
	_gx[0xac] = (_gx[0xac]&~0xff000000)|(_SHIFTL(0x00,24,8));

	i=0;
	re0 = 0x30;
	re1 = 0x31;
	while(i<8) {
		addr = 0xa0+i;
		_gx[addr] = (_gx[addr]&~0xff000000)|(_SHIFTL(re0,24,8));
		addr = 0xb0+i;
		_gx[addr] = (_gx[addr]&~0xff000000)|(_SHIFTL(re1,24,8));
		re0 += 2; re1 += 2; i++;
	}
	
	_gx[0xb8] = (_gx[0xb8]&~0xff000000)|(_SHIFTL(0x40,24,8));
	_gx[0xb9] = (_gx[0xb9]&~0xff000000)|(_SHIFTL(0x41,24,8));
	_gx[0xba] = (_gx[0xba]&~0xff000000)|(_SHIFTL(0x42,24,8));
	_gx[0xbb] = (_gx[0xbb]&~0xff000000)|(_SHIFTL(0x43,24,8));

	i=0;
	re0 = 0x25;
	while(i<11) {
		addr = 0xc0+i;
		_gx[addr] = (_gx[addr]&~0xff000000)|(_SHIFTL(re0,24,8));
		re0++; i++;
	}

	divis = 500;
	res = (u32)(divid/divis);
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x69000000|((_SHIFTR(res,11,24))|0x0400));
	
	divis = 4224;
	res = (u32)(res/divis);
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x46000000|(res|0x0200));

	i=0;
	re0 = 0xf6;
	while(i<8) {
		addr = 0xd0+i;
		_gx[addr] = (_gx[addr]&~0xff000000)|(_SHIFTL(re0,24,8));
		re0++; i++;
	}

	_gx[0x0a] = 0;
	_gx[0x0b] = GX_PERF0_NONE;
	_gx[0x0c] = GX_PERF1_NONE;
	_gx[0x0d] = 0;

	i=0;
	while(i<8) {
		_gx[0x18+i] = 0x80000000;
		GX_LOAD_CP_REG((0x0080|i),_gx[0x18+i]);
		i++;
	}
	
	GX_LOAD_XF_REG(0x1000,0x3f);
	GX_LOAD_XF_REG(0x1012,0x01);
	
	GX_LOAD_BP_REG(0x5800000f);

	i=0;
	while(i<16) {
		_gx[0x30+i] = 0xff;
		i++;
	}

	for(i=0;i<8;i++) {
		tmem_even = tmem_odd = (i<<15);
		region = (GXTexRegion*)(&_gx[0x100+(i*(sizeof(GXTexRegion)>>2))]);
		GX_InitTexCacheRegion(region,GX_FALSE,tmem_even,GX_TEXCACHE_32K,(tmem_odd+0x00080000),GX_TEXCACHE_32K);
	}
	for(i=0;i<4;i++) {
		tmem_even = ((0x08+(i<<1))<<15);
		tmem_odd = ((0x09+(i<<1))<<15);
		region = (GXTexRegion*)(&_gx[0x120+(i*(sizeof(GXTexRegion)>>2))]);
		GX_InitTexCacheRegion(region,GX_FALSE,tmem_even,GX_TEXCACHE_32K,tmem_odd,GX_TEXCACHE_32K);
	}
	for(i=0;i<16;i++) {
		tmem_even = (i<<13)+0x000C0000;
		tregion = (GXTlutRegion*)(&_gx[0x150+(i*(sizeof(GXTlutRegion)>>2))]);
		GX_InitTlutRegion(tregion,tmem_even,GX_TLUT_256);
	}
	for(i=0;i<4;i++) {
		tmem_even = (i<<15)+0x000E0000;
		tregion = (GXTlutRegion*)(&_gx[0x150+((i+16)*(sizeof(GXTlutRegion)>>2))]);
		GX_InitTlutRegion(tregion,tmem_even,GX_TLUT_1K);
	}

	_cpReg[3] = 0;
	GX_LOAD_CP_REG(0x20,0x00000000);
	GX_LOAD_XF_REG(0x1006,0x0);
	
	GX_LOAD_BP_REG(0x23000000);
	GX_LOAD_BP_REG(0x24000000);
	GX_LOAD_BP_REG(0x67000000);

	__GX_SetTmemConfig(0);
	__GX_InitGX();
	
	return &_gxdefiniobj;
}

void GX_InitFifoBase(GXFifoObj *fifo,void *base,u32 size)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;

	if(!ptr || size<GX_FIFO_MINSIZE || ptr==_cpufifo || ptr==_gpfifo) return;

	ptr->buf_start = (u32)base;
	ptr->buf_end = (u32)base + size - 4;
	ptr->size = size;
	ptr->rdwt_dst = 0;

	GX_InitFifoLimits(fifo,(size-GX_FIFO_HIWATERMARK),((size>>1)&0x7fffffe0));
	GX_InitFifoPtrs(fifo,base,base);
}

void GX_InitFifoLimits(GXFifoObj *fifo,u32 hiwatermark,u32 lowatermark)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;
	
	ptr->hi_mark = hiwatermark;
	ptr->lo_mark = lowatermark;
}

void GX_InitFifoPtrs(GXFifoObj *fifo,void *rd_ptr,void *wt_ptr)
{
	u32 level;
	s32 rdwt_dst;
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;

	_CPU_ISR_Disable(level);
	rdwt_dst =  wt_ptr-rd_ptr;
	ptr->rd_ptr = (u32)rd_ptr;
	ptr->wt_ptr = (u32)wt_ptr;
	ptr->rdwt_dst = rdwt_dst;
	if(rdwt_dst<0) {
		rdwt_dst += ptr->size;
		ptr->rd_ptr = rdwt_dst;
	}
	_CPU_ISR_Restore(level);
}

void GX_GetFifoPtrs(GXFifoObj *fifo,void **rd_ptr,void **wt_ptr)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;

	if(_cpufifo==ptr) ptr->wt_ptr = (u32)MEM_PHYSICAL_TO_K0((_piReg[5]&~0x04000000));
	if(_gpfifo==ptr) {
		ptr->rd_ptr = MEM_VIRTUAL_TO_PHYSICAL(_SHIFTL(_cpReg[29],16,16)|(_cpReg[28]&0xffff));
		ptr->rdwt_dst = (_SHIFTL(_cpReg[25],16,16)|(_cpReg[24]&0xffff));
	} else {
		ptr->rdwt_dst = (ptr->wt_ptr - ptr->rd_ptr);
		if(ptr->rd_ptr<0) ptr->rdwt_dst += ptr->size;
	}
	*rd_ptr = (void*)ptr->rd_ptr;
	*wt_ptr = (void*)ptr->wt_ptr;
}

void GX_SetCPUFifo(GXFifoObj *fifo)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	_cpufifo = (struct __gxfifo*)fifo;
	if(_cpufifo==_gpfifo) {
		_piReg[3] = MEM_VIRTUAL_TO_PHYSICAL(_cpufifo->buf_start);
		_piReg[4] = MEM_VIRTUAL_TO_PHYSICAL(_cpufifo->buf_end);
		_piReg[5] = ((_cpufifo->wt_ptr&0x3FFFFFE0)&~0x04000000);
		_cpgplinked = 1;

		__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
		__GX_FifoLink(GX_TRUE);

		_CPU_ISR_Restore(level);
		return;
	}
	if(_cpgplinked) {
		__GX_FifoLink(GX_FALSE);
		_cpgplinked = 0;
	}
	__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);

	_piReg[3] = MEM_VIRTUAL_TO_PHYSICAL(_cpufifo->buf_start);
	_piReg[4] = MEM_VIRTUAL_TO_PHYSICAL(_cpufifo->buf_end);
	_piReg[5] = ((_cpufifo->wt_ptr&0x3FFFFFE0)&~0x04000000);
	_CPU_ISR_Restore(level);
}

GXFifoObj* GX_GetCPUFifo()
{
	return (GXFifoObj*)_cpufifo;
}

void GX_SetGPFifo(GXFifoObj *fifo)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__GX_FifoReadDisable();
	__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);
	
	_gpfifo = (struct __gxfifo*)fifo;
	
	/* setup fifo base */
	_cpReg[16] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->buf_start),0,16);
	_cpReg[17] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->buf_start),16,16);
	
	/* setup fifo end */
	_cpReg[18] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->buf_end),0,16);
	_cpReg[19] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->buf_end),16,16);
	
	/* setup hiwater mark */
	_cpReg[20] = _SHIFTL(_gpfifo->hi_mark,0,16);
	_cpReg[21] = _SHIFTR(_gpfifo->hi_mark,16,16);
	
	/* setup lowater mark */
	_cpReg[22] = _SHIFTL(_gpfifo->lo_mark,0,16);
	_cpReg[23] = _SHIFTR(_gpfifo->lo_mark,16,16);
	
	/* setup rd<->wd dist */
	_cpReg[24] = _SHIFTL(_gpfifo->rdwt_dst,0,16);
	_cpReg[25] = _SHIFTR(_gpfifo->rdwt_dst,16,16);
	
	/* setup wt ptr */
	_cpReg[26] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->wt_ptr),0,16);
	_cpReg[27] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->wt_ptr),16,16);
	
	/* setup rd ptr */
	_cpReg[28] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->rd_ptr),0,16);
	_cpReg[29] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(_gpfifo->rd_ptr),16,16);

	if(_cpufifo==_gpfifo) {
		_cpgplinked = 1;
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
		__GX_FifoLink(GX_TRUE);
	} else {
		_cpgplinked = 0;
		__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);
		__GX_FifoLink(GX_FALSE);
	}
	__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
	__GX_FifoReadEnable();
	_CPU_ISR_Restore(level);
}

GXFifoObj* GX_GetGPFifo()
{
	return (GXFifoObj*)_gpfifo;
}

void GX_SaveCPUFifo(GXFifoObj *fifo)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;
	__GX_SaveCPUFifoAux(ptr);
}

u32 GX_GetOverflowCount()
{
	return _gxoverflowcount;
}

u32 GX_ResetOverflowCount()
{
	u32 ret = _gxoverflowcount;
	_gxoverflowcount = 0;
	return ret;
}

lwp_t GX_GetCurrentGXThread()
{
	return _gxcurrentlwp;
}

lwp_t GX_SetCurrentGXThread()
{
	u32 level;

	_CPU_ISR_Disable(level);
	lwp_t ret = _gxcurrentlwp;
	_gxcurrentlwp = LWP_GetSelf();
	_CPU_ISR_Restore(level);

	return ret;
}

volatile void* GX_RedirectWriteGatherPipe(void *ptr)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GX_Flush();
	while(!IsWriteGatherBufferEmpty());

	mtwpar(0x0C008000);
	if(_cpgplinked) {
		__GX_FifoLink(GX_FALSE);
		__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);
	}
	_cpufifo->wt_ptr = (u32)MEM_PHYSICAL_TO_K0(_piReg[5]&~0x04000000);
	
	_piReg[3] = 0;
	_piReg[4] = 0x04000000;
	_piReg[5] = (((u32)ptr&0x3FFFFFE0)&~0x04000000);
	_sync();
	
	_CPU_ISR_Restore(level);

	return (volatile void*)0x0C008000;
}

void GX_RestoreWriteGatherPipe()
{
	u32 level;

	_CPU_ISR_Disable(level);
	
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	
	ppcsync();
	while(!IsWriteGatherBufferEmpty());
	
	mtwpar(0x0C008000);
	_piReg[3] = MEM_VIRTUAL_TO_PHYSICAL(_cpufifo->buf_start);
	_piReg[4] = MEM_VIRTUAL_TO_PHYSICAL(_cpufifo->buf_end);
	_piReg[5] = (((u32)_cpufifo->wt_ptr&0x3FFFFFE0)&~0x04000000);
	if(_cpgplinked) {
		__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
		__GX_FifoLink(GX_TRUE);
	}	
	_sync();
	_CPU_ISR_Restore(level);
}

void GX_Flush()
{
	if(_gx[0x09]) 
		__GX_SetDirtyState();

	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	
	ppcsync();
}

void GX_EnableBreakPt(void *break_pt)
{
	u32 level = 0;
	_CPU_ISR_Disable(level);
	__GX_FifoReadDisable();
	_cpReg[30] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(break_pt),0,16);
	_cpReg[31] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(break_pt),16,16);
	((u16*)_gx)[1] = (((u16*)_gx)[1]&~0x22)|0x22;
	_cpReg[1] = ((u16*)_gx)[1];
	_gxcurrbp = break_pt;
	__GX_FifoReadEnable();
 	_CPU_ISR_Restore(level);
}

void GX_DisableBreakPt()
{
	u32 level = 0;
	_CPU_ISR_Disable(level);
	((u16*)_gx)[1] = (((u16*)_gx)[1]&~0x22);
	_cpReg[1] = ((u16*)_gx)[1];
	_gxcurrbp = NULL;
	_CPU_ISR_Restore(level);
}

void GX_AbortFrame()
{
	u64 start;

	_piReg[6] = 1;
	start = gettime();
	while(diff_ticks(start,gettime())<50);
	_piReg[6] = 0;
	start = gettime();
	while(diff_ticks(start,gettime())<5);
	
	__GX_CleanGPFifo();
}

void GX_SetDrawSync(u16 token)
{
	u32 level = 0;
	_CPU_ISR_Disable(level);
	GX_LOAD_BP_REG(0x48000000 | token);
	GX_LOAD_BP_REG(0x47000000 | token);
	GX_Flush();
	_CPU_ISR_Restore(level);
}

u16 GX_GetDrawSync()
{
	return _peReg[7];
}

void GX_SetDrawDone()
{
	u32 level;
	_CPU_ISR_Disable(level);
	GX_LOAD_BP_REG(0x45000002); // set draw done!
	GX_Flush();
	_gxfinished = 0;
	_CPU_ISR_Restore(level);
}


void GX_WaitDrawDone()
{
	u32 level;
#ifdef _GP_DEBUG
	printf("GX_WaitDrawDone()\n\n");
#endif
	_CPU_ISR_Disable(level);
	while(!_gxfinished)
		LWP_ThreadSleep(_gxwaitfinish);
	_CPU_ISR_Restore(level);
}

void GX_DrawDone()
{
	u32 level;

	_CPU_ISR_Disable(level);
	GX_LOAD_BP_REG(0x45000002); // set draw done!
	GX_Flush();

	_gxfinished = 0;
	_CPU_ISR_Flash(level);

	while(!_gxfinished)
		LWP_ThreadSleep(_gxwaitfinish);
	_CPU_ISR_Restore(level);
}

GXDrawDoneCallback GX_SetDrawDoneCallback(GXDrawDoneCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GXDrawDoneCallback ret = drawDoneCB;
	drawDoneCB = cb;
	_CPU_ISR_Restore(level);
	return ret;
}

GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GXDrawSyncCallback ret = tokenCB;
	tokenCB = cb;
	_CPU_ISR_Restore(level);
	return ret;
}

GXBreakPtCallback GX_SetBreakPtCallback(GXBreakPtCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GXBreakPtCallback ret = breakPtCB;
	breakPtCB = cb;
	_CPU_ISR_Restore(level);
	return ret;
}

void GX_PixModeSync()
{
	GX_LOAD_BP_REG(_gx[0xbb]);
}

void GX_TexModeSync()
{
	GX_LOAD_BP_REG(0x63000000);
}

void GX_SetMisc(u32 token,u32 value)
{
	u32 cnt;

	if(token==GX_MT_XF_FLUSH) {
		((u16*)_gx)[30] = value;
		cnt = cntlzw(((u16*)_gx)[30]);
		((u16*)_gx)[28] = _SHIFTR(cnt,5,16);

		((u16*)_gx)[29] = 1;
		if(!((u16*)_gx)[30]) return;

		_gx[0x09] |= 0x0008;
	} else if(token==GX_MT_DL_SAVE_CTX) {
		((u8*)_gx)[0x371] = (value&0xff);
	}
	return;
}

void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field)
{
	f32 x0,y0,x1,y1,n,f,z;
	static f32 Xfactor = 0.5;
	static f32 Yfactor = 342.0;
	static f32 Zfactor = 16777215.0;
	
	if(!field) yOrig -= Xfactor;
	
	x0 = wd*Xfactor;
	y0 = (-ht)*Xfactor;
	x1 = (xOrig+(wd*Xfactor))+Yfactor;
	y1 = (yOrig+(ht*Xfactor))+Yfactor;
	n = Zfactor*nearZ;
	f = Zfactor*farZ;
	z = f-n;
	
	GX_LOAD_XF_REGS(0x101a,6);
	FIFO_PUTF32(x0);
	FIFO_PUTF32(y0);
	FIFO_PUTF32(z);
	FIFO_PUTF32(x1);
	FIFO_PUTF32(y1);
	FIFO_PUTF32(f);
}

void GX_SetViewport(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ)
{
	GX_SetViewportJitter(xOrig,yOrig,wd,ht,nearZ,farZ,1);
}

void GX_LoadProjectionMtx(Mtx44 mt,u8 type)
{
	f32 tmp[7];

	((u32*)((void*)tmp))[6] = (u32)type;
	tmp[0] = mt[0][0];
	tmp[2] = mt[1][1];
	tmp[4] = mt[2][2];
	tmp[5] = mt[2][3];
	
	switch(type) {
		case GX_PERSPECTIVE:
			tmp[1] = mt[0][2];
			tmp[3] = mt[1][2];
			break;
		case GX_ORTHOGRAPHIC:
			tmp[1] = mt[0][3];
			tmp[3] = mt[1][3];
			break;
	}

	GX_LOAD_XF_REGS(0x1020,7);
	FIFO_PUTF32(tmp[0]);
	FIFO_PUTF32(tmp[1]);
	FIFO_PUTF32(tmp[2]);
	FIFO_PUTF32(tmp[3]);
	FIFO_PUTF32(tmp[4]);
	FIFO_PUTF32(tmp[5]);
	FIFO_PUTF32(tmp[6]);
}

static void __GetImageTileCount(u32 fmt,u16 wd,u16 ht,u32 *xtiles,u32 *ytiles,u32 *zplanes)
{
	u32 xshift,yshift,tile;
	
	switch(fmt) {
		case GX_TF_I4:
		case GX_TF_IA4:
		case GX_CTF_R4:
		case GX_CTF_RA4:
		case GX_CTF_Z4:
			xshift = 3;
			yshift = 3;
			break;
		case GX_TF_Z8:
		case GX_TF_I8:
		case GX_TF_IA8:
		case GX_CTF_RA8:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
			xshift = 3;
			yshift = 2;
			break;
		case GX_TF_Z16:
		case GX_TF_Z24X8:
		case GX_CTF_Z16L:
		case GX_TF_RGB565:
		case GX_TF_RGB5A3:
		case GX_TF_RGBA8:
			xshift = 2;
			yshift = 2;
			break;
		default:
			xshift = 0;
			yshift = 0;
			break;
	}
	
	if(!(wd&0xffff)) wd = 1;
	if(!(ht&0xffff)) ht = 1;

	wd &= 0xffff;
	tile = (wd+((1<<xshift)-1))>>xshift;
	*xtiles = tile;

	ht &= 0xffff;
	tile = (ht+((1<<yshift)-1))>>yshift;
	*ytiles = tile;

	*zplanes = 1;
	if(fmt==GX_TF_RGBA8 || fmt==GX_TF_Z24X8) *zplanes = 2;
}

void GX_SetCopyClear(GXColor color,u32 zvalue)
{
	u32 val;

	val = (_SHIFTL(color.a,8,8))|(color.r&0xff);
	GX_LOAD_BP_REG(0x4f000000|val);

	val = (_SHIFTL(color.g,8,8))|(color.b&0xff);
	GX_LOAD_BP_REG(0x50000000|val);

	val = zvalue&0x00ffffff;
	GX_LOAD_BP_REG(0x51000000|val);
}

void GX_SetCopyClamp(u8 clamp)
{
	_gx[0xab] = (_gx[0xab]&~1)|(clamp&1);
	_gx[0xab] = (_gx[0xab]&~2)|(clamp&2);
}

void GX_SetDispCopyGamma(u8 gamma)
{
	_gx[0xab] = (_gx[0xab]&~0x180)|(_SHIFTL(gamma,7,2));
}

void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7])
{
	u32 reg01=0,reg02=0,reg03=0,reg04=0,reg53=0,reg54=0;

	if(aa) {
		reg01 = sample_pattern[0][0]&0xf;
		reg01 = (reg01&~0xf0)|(_SHIFTL(sample_pattern[0][1],4,4));
		reg01 = (reg01&~0xf00)|(_SHIFTL(sample_pattern[1][0],8,4));
		reg01 = (reg01&~0xf000)|(_SHIFTL(sample_pattern[1][1],12,4));
		reg01 = (reg01&~0xf0000)|(_SHIFTL(sample_pattern[2][0],16,4));
		reg01 = (reg01&~0xf00000)|(_SHIFTL(sample_pattern[2][1],20,4));
		reg01 = (reg01&~0xff000000)|(_SHIFTL(0x01,24,8));

		reg02 = sample_pattern[3][0]&0xf;
		reg02 = (reg02&~0xf0)|(_SHIFTL(sample_pattern[3][1],4,4));
		reg02 = (reg02&~0xf00)|(_SHIFTL(sample_pattern[4][0],8,4));
		reg02 = (reg02&~0xf000)|(_SHIFTL(sample_pattern[4][1],12,4));
		reg02 = (reg02&~0xf0000)|(_SHIFTL(sample_pattern[5][0],16,4));
		reg02 = (reg02&~0xf00000)|(_SHIFTL(sample_pattern[5][1],20,4));
		reg02 = (reg02&~0xff000000)|(_SHIFTL(0x02,24,8));

		reg03 = sample_pattern[6][0]&0xf;
		reg03 = (reg03&~0xf0)|(_SHIFTL(sample_pattern[6][1],4,4));
		reg03 = (reg03&~0xf00)|(_SHIFTL(sample_pattern[7][0],8,4));
		reg03 = (reg03&~0xf000)|(_SHIFTL(sample_pattern[7][1],12,4));
		reg03 = (reg03&~0xf0000)|(_SHIFTL(sample_pattern[8][0],16,4));
		reg03 = (reg03&~0xf00000)|(_SHIFTL(sample_pattern[8][1],20,4));
		reg03 = (reg03&~0xff000000)|(_SHIFTL(0x03,24,8));
	
		reg04 = sample_pattern[9][0]&0xf;
		reg04 = (reg04&~0xf0)|(_SHIFTL(sample_pattern[9][1],4,4));
		reg04 = (reg04&~0xf00)|(_SHIFTL(sample_pattern[10][0],8,4));
		reg04 = (reg04&~0xf000)|(_SHIFTL(sample_pattern[10][1],12,4));
		reg04 = (reg04&~0xf0000)|(_SHIFTL(sample_pattern[11][0],16,4));
		reg04 = (reg04&~0xf00000)|(_SHIFTL(sample_pattern[11][1],20,4));
		reg04 = (reg04&~0xff000000)|(_SHIFTL(0x04,24,8));
	} else {
		reg01 = 0x01666666;
		reg02 = 0x02666666;
		reg03 = 0x03666666;
		reg04 = 0x04666666;
	}
	GX_LOAD_BP_REG(reg01);
	GX_LOAD_BP_REG(reg02);
	GX_LOAD_BP_REG(reg03);
	GX_LOAD_BP_REG(reg04);

	reg53 = 0x53595000;
	reg54 = 0x54000015;
	if(vf) {
		reg53 = 0x53000000|(vfilter[0]&0x3f);
		reg53 = (reg53&~0xfc0)|(_SHIFTL(vfilter[1],6,6));
		reg53 = (reg53&~0x3f000)|(_SHIFTL(vfilter[2],12,6));
		reg53 = (reg53&~0xfc0000)|(_SHIFTL(vfilter[3],18,6));
		
		reg54 = 0x54000000|(vfilter[4]&0x3f);
		reg54 = (reg54&~0xfc0)|(_SHIFTL(vfilter[5],6,6));
		reg54 = (reg54&~0x3f000)|(_SHIFTL(vfilter[6],12,6));
	}
	GX_LOAD_BP_REG(reg53);
	GX_LOAD_BP_REG(reg54);
}

void GX_SetDispCopyFrame2Field(u8 mode)
{
	_gx[0xab] = (_gx[0xab]&~0x3000)|(_SHIFTL(mode,12,2));
}

u32 GX_SetDispCopyYScale(f32 yscale)
{
	u32 ht,yScale = 0;

	yScale = ((u32)(256.0f/yscale))&0x1ff;
	GX_LOAD_BP_REG(0x4e000000|yScale);

	_gx[0xab] = (_gx[0xab]&~0x400)|(_SHIFTL(((256-yScale)>0),10,1));
	ht = _SHIFTR(_gx[0xcc],12,10)+1;
	return __GX_GetNumXfbLines(ht,yScale);
}

void GX_SetDispCopyDst(u16 wd,u16 ht)
{
	_gx[0xcd] = (_gx[0xcd]&~0x3ff)|(_SHIFTR(wd,4,10));
	_gx[0xcd] = (_gx[0xcd]&~0xff000000)|(_SHIFTL(0x4d,24,8));
}

void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht)
{
	_gx[0xcb] = (_gx[0xcb]&~0x00ffffff)|XY(left,top);
	_gx[0xcb] = (_gx[0xcb]&~0xff000000)|(_SHIFTL(0x49,24,8));
	_gx[0xcc] = (_gx[0xcc]&~0x00ffffff)|XY((wd-1),(ht-1));
	_gx[0xcc] = (_gx[0xcc]&~0xff000000)|(_SHIFTL(0x4a,24,8));
}

void GX_CopyDisp(void *dest,u8 clear)
{
	u8 clflag;
	u32 val;

	if(clear) {
		val= (_gx[0xb8]&~0xf)|0xf;
		GX_LOAD_BP_REG(val);
		val = (_gx[0xb9]&~0x3);
		GX_LOAD_BP_REG(val);
	}
	
	clflag = 0;
	if(clear || (_gx[0xbb]&0x7)==0x0003) {
		if(_gx[0xbb]&0x40) {
			clflag = 1;
			val = (_gx[0xbb]&~0x40);
			GX_LOAD_BP_REG(val);
		}
	}
	
	GX_LOAD_BP_REG(_gx[0xcb]);  // set source top
	GX_LOAD_BP_REG(_gx[0xcc]);

	GX_LOAD_BP_REG(_gx[0xcd]);

	val = 0x4b000000|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24));
	GX_LOAD_BP_REG(val);

	_gx[0xab] = (_gx[0xab]&~0x800)|(_SHIFTL(clear,11,1));
	_gx[0xab] = (_gx[0xab]&~0x4000)|0x4000;
	_gx[0xab] = (_gx[0xab]&~0xff000000)|(_SHIFTL(0x52,24,8));
	
	GX_LOAD_BP_REG(_gx[0xab]);

	if(clear) {
		GX_LOAD_BP_REG(_gx[0xb8]);
		GX_LOAD_BP_REG(_gx[0xb9]);
	}
	if(clflag) GX_LOAD_BP_REG(_gx[0xbb]);
}

void GX_CopyTex(void *dest,u8 clear)
{
	u8 clflag;
	u32 val;

	if(clear) {
		val = (_gx[0xb8]&~0xf)|0xf;
		GX_LOAD_BP_REG(val);
		val = (_gx[0xb9]&~0x3);
		GX_LOAD_BP_REG(val);
	}

	clflag = 0;
	val = _gx[0xbb];
	if(((u8*)_gx)[0x370] && (val&0x7)!=0x0003) {
		clflag = 1;
		val = (val&~0x7)|0x0003;
	}
	if(clear || (val&0x7)==0x0003) {
		if(val&0x40) {
			clflag = 1;
			val = (val&~0x40);
		}
	}
	if(clflag) GX_LOAD_BP_REG(val);

	val = 0x4b000000|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24));
	
	GX_LOAD_BP_REG(_gx[0xd8]);
	GX_LOAD_BP_REG(_gx[0xd9]);
	GX_LOAD_BP_REG(_gx[0xda]);
	GX_LOAD_BP_REG(val);

	_gx[0xdb] = (_gx[0xdb]&~0x800)|(_SHIFTL(clear,11,1));
	_gx[0xdb] = (_gx[0xdb]&~0x4000);
	_gx[0xdb] = (_gx[0xdb]&~0xff000000)|(_SHIFTL(0x52,24,8));
	GX_LOAD_BP_REG(_gx[0xdb]);

	if(clear) {
		GX_LOAD_BP_REG(_gx[0xb8]);
		GX_LOAD_BP_REG(_gx[0xb9]);
	}
	if(clflag) GX_LOAD_BP_REG(_gx[0xbb]);
}

void GX_SetTexCopySrc(u16 left,u16 top,u16 wd,u16 ht)
{
	_gx[0xd8] = (_gx[0xd8]&~0x00ffffff)|XY(left,top);
	_gx[0xd8] = (_gx[0xd8]&~0xff000000)|(_SHIFTL(0x49,24,8));
	_gx[0xd9] = (_gx[0xd9]&~0x00ffffff)|XY((wd-1),(ht-1));
	_gx[0xd9] = (_gx[0xd9]&~0xff000000)|(_SHIFTL(0x4a,24,8));
}

void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap)
{
	u8 lfmt = fmt&0xf;
	u32 xtiles,ytiles,zplanes;

	__GetImageTileCount(fmt,wd,ht,&xtiles,&ytiles,&zplanes);
	_gx[0xda] = (_gx[0xda]&~0x3ff)|((xtiles*zplanes)&0x3ff);

	if(fmt==GX_TF_Z16) lfmt = 11;
	if(fmt==GX_CTF_YUVA8 || (fmt>=GX_TF_I4 && fmt<GX_TF_RGB565)) _gx[0xdb] = (_gx[0xdb]&~0x18000)|0x18000;
	else _gx[0xdb] = (_gx[0xdb]&~0x18000)|0x10000;

	_gx[0xdb] = (_gx[0xdb]&~0x8)|(lfmt&0x8);
	_gx[0xdb] = (_gx[0xdb]&~0x200)|(_SHIFTL(mipmap,9,1));
	_gx[0xdb] = (_gx[0xdb]&~0x70)|(_SHIFTL(lfmt,4,3));

	_gx[0xda] = (_gx[0xda]&~0xff000000)|(_SHIFTL(0x4d,24,8));

	((u8*)_gx)[0x370] = 0;
	((u8*)_gx)[0x370] = ((fmt&0x10)-16)?0:1;
}

void GX_ClearBoundingBox()
{
	GX_LOAD_BP_REG(0x550003ff);
	GX_LOAD_BP_REG(0x560003ff);
}

void GX_BeginDispList(void *list,u32 size)
{
	GXFifoObj *curr_fifo;

	curr_fifo = GX_GetCPUFifo();
	if(_gx[0x09]) 
		__GX_SetDirtyState();

	if(((u8*)_gx)[0x371]) 
		memcpy(_gx_saved_data,_gx,1280);

	_gx_dl_fifo.buf_start = (u32)list;
	_gx_dl_fifo.buf_end = (u32)list + size - 4;
	_gx_dl_fifo.size = size;

	_gx_dl_fifo.rd_ptr = (u32)list;
	_gx_dl_fifo.wt_ptr = (u32)list;
	_gx_dl_fifo.rdwt_dst = 0;
	
	((u8*)_gx)[0x372] = 1;

	GX_SaveCPUFifo(curr_fifo);
	_gxoldcpufifo = curr_fifo;

	GX_SetGPFifo((GXFifoObj*)&_gx_dl_fifo);
}

u32 GX_EndDispList()
{
	u32 val;
	u32 level,ret = 0;
	
	if(_gx[0x09])
		__GX_SetDirtyState();

	__GX_SaveCPUFifoAux(&_gx_dl_fifo);
	GX_SetCPUFifo(_gxoldcpufifo);

	val = _SHIFTR(_piReg[5],26,1);
	if(((u8*)_gx)[0x371]) {
		_CPU_ISR_Disable(level);
		memcpy(_gx,_gx_saved_data,1272);
		_CPU_ISR_Restore(level);
	}

	((u8*)_gx)[0x372] = 0;
	if(	_SHIFTR(_piReg[5],26,1)) ret = _gx_dl_fifo.rdwt_dst;

	return ret;
}

void GX_CallDispList(void *list,u32 nbytes)
{
	if(_gx[0x09]) 
		__GX_SetDirtyState();

	if(!_gx[0x00])
		__GX_SendFlushPrim();
	
	FIFO_PUTU8(0x40);			//call displaylist
	FIFO_PUTU32(MEM_VIRTUAL_TO_PHYSICAL(list));
	FIFO_PUTU32(nbytes);
}

void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn)
{
	u32 difffn = (attn_fn==GX_AF_SPEC)?GX_DF_NONE:diff_fn;
	u32 val = (matsrc&1)|(_SHIFTL(enable,1,1))|(_SHIFTL(litmask,2,4))|(_SHIFTL(ambsrc,6,1))|(_SHIFTL(difffn,7,2))|(_SHIFTL(((GX_AF_NONE-attn_fn)>0),9,1))|(_SHIFTL((attn_fn&1),10,1))|(_SHIFTL((_SHIFTR(litmask,4,4)),11,4));
	switch(channel) {
		case GX_COLOR0:
			GX_LOAD_XF_REG(0x100e,val);
			break;
		case GX_COLOR1:
			GX_LOAD_XF_REG(0x100f,val);
			break;
		case GX_ALPHA0:
			GX_LOAD_XF_REG(0x1010,val);
			break;
		case GX_ALPHA1:
			GX_LOAD_XF_REG(0x1011,val);
			break;
		case GX_COLOR0A0:
			GX_LOAD_XF_REG(0x100e,val);
			GX_LOAD_XF_REG(0x1010,val);
			break;
		case GX_COLOR1A1:
			GX_LOAD_XF_REG(0x100f,val);
			GX_LOAD_XF_REG(0x1011,val);
			break;
	}
}

void GX_SetChanAmbColor(s32 channel,GXColor color)
{
	u32 val = (_SHIFTL(color.r,24,8))|(_SHIFTL(color.g,16,8))|(_SHIFTL(color.b,8,8))|(color.a&0xFF);
	switch(channel) {
		case GX_COLOR0:
			GX_LOAD_XF_REG(0x100a,val);
			break;
		case GX_COLOR1:
			GX_LOAD_XF_REG(0x100b,val);
			break;
		case GX_COLOR0A0:
			GX_LOAD_XF_REG(0x100a,val);
			break;
		case GX_COLOR1A1:
			GX_LOAD_XF_REG(0x100b,val);
			break;
	}
}

void GX_SetChanMatColor(s32 channel,GXColor color)
{
	u32 val = (_SHIFTL(color.r,24,8))|(_SHIFTL(color.g,16,8))|(_SHIFTL(color.b,8,8))|0x00;
	switch(channel) {
		case GX_COLOR0:
			GX_LOAD_XF_REG(0x100c,val);
			break;
		case GX_COLOR1:
			GX_LOAD_XF_REG(0x100d,val);
			break;
		case GX_COLOR0A0:
			val |= (color.a&0xFF);
			GX_LOAD_XF_REG(0x100c,val);
			break;
		case GX_COLOR1A1:
			val |= (color.a&0xFF);
			GX_LOAD_XF_REG(0x100d,val);
			break;
	}
}

void GX_SetArray(u32 attr,void *ptr,u8 stride)
{
	u32 idx = 0;
	if(attr>=GX_VA_POS && attr<=GX_LITMTXARRAY) {
		idx = attr-GX_VA_POS;
		GX_LOAD_CP_REG((0xA0+idx),((u32)ptr)&0x03FFFFFF);
		GX_LOAD_CP_REG((0xB0+idx),(u32)stride);
	}
}

static __inline__ void __SETVCDATTR(u8 attr,u8 type)
{
	switch(attr) {
		case GX_VA_PTNMTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x1)|(type&0x1);
			break;
		case GX_VA_TEX0MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x2)|(_SHIFTL(type,1,1));
			break;
		case GX_VA_TEX1MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x4)|(_SHIFTL(type,2,1));
			break;
		case GX_VA_TEX2MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x8)|(_SHIFTL(type,3,1));
			break;
		case GX_VA_TEX3MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x10)|(_SHIFTL(type,4,1));
			break;
		case GX_VA_TEX4MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x20)|(_SHIFTL(type,5,1));
			break;
		case GX_VA_TEX5MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x40)|(_SHIFTL(type,6,1));
			break;
		case GX_VA_TEX6MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x80)|(_SHIFTL(type,7,1));
			break;
		case GX_VA_TEX7MTXIDX:
			_gx[0x06] = (_gx[0x06]&~0x100)|(_SHIFTL(type,8,1));
			break;
		case GX_VA_POS:
			_gx[0x06] = (_gx[0x06]&~0x600)|(_SHIFTL(type,9,2));
			break;
		case GX_VA_NRM:
			_gx[0x06] = (_gx[0x06]&~0x1800)|(_SHIFTL(type,11,2));
			_gx[0x08] = 1;
			break;
		case GX_VA_NBT:
			_gx[0x06] = (_gx[0x06]&~0x1800)|(_SHIFTL(type,11,2));
			_gx[0x08] = 2;
			break;
		case GX_VA_CLR0:
			_gx[0x06] = (_gx[0x06]&~0x6000)|(_SHIFTL(type,13,2));
			break;
		case GX_VA_CLR1:
			_gx[0x06] = (_gx[0x06]&~0x18000)|(_SHIFTL(type,15,2));
			break;
		case GX_VA_TEX0:
			_gx[0x07] = (_gx[0x07]&~0x3)|(type&0x3);
			break;
		case GX_VA_TEX1:
			_gx[0x07] = (_gx[0x07]&~0xc)|(_SHIFTL(type,2,2));
			break;
		case GX_VA_TEX2:
			_gx[0x07] = (_gx[0x07]&~0x30)|(_SHIFTL(type,4,2));
			break;
		case GX_VA_TEX3:
			_gx[0x07] = (_gx[0x07]&~0xc0)|(_SHIFTL(type,6,2));
			break;
		case GX_VA_TEX4:
			_gx[0x07] = (_gx[0x07]&~0x300)|(_SHIFTL(type,8,2));
			break;
		case GX_VA_TEX5:
			_gx[0x07] = (_gx[0x07]&~0xc00)|(_SHIFTL(type,10,2));
			break;
		case GX_VA_TEX6:
			_gx[0x07] = (_gx[0x07]&~0x3000)|(_SHIFTL(type,12,2));
			break;
		case GX_VA_TEX7:
			_gx[0x07] = (_gx[0x07]&~0xc000)|(_SHIFTL(type,14,2));
			break;
	}
}

void GX_SetVtxDesc(u8 attr,u8 type)
{
	__SETVCDATTR(attr,type);
	_gx[0x09] |= 0x0008;
}

void GX_SetVtxDescv(GXVtxDesc *attr_list)
{
	u32 i;

	if(!attr_list) return;

	for(i=0;i<GX_MAX_VTXDESC_LISTSIZE;i++){
		if(attr_list[i].attr==GX_VA_NULL) break;
		
		__SETVCDATTR(attr_list[i].attr,attr_list[i].type);
	}
	_gx[0x09] |= 0x0008;
}

static __inline__ void __SETVCDFMT(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac)
{
	u8 vat0 = 0x10+vtxfmt;
	u8 vat1 = 0x18+vtxfmt;
	u8 vat2 = 0x20+vtxfmt;

	if(vtxattr==GX_VA_POS && (comptype==GX_POS_XY || comptype==GX_POS_XYZ)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat0] = (_gx[vat0]&~0x1)|(comptype&1);
		_gx[vat0] = (_gx[vat0]&~0xe)|(_SHIFTL(compsize,1,3));
		_gx[vat0] = (_gx[vat0]&~0x1f0)|(_SHIFTL(frac,4,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_NRM && comptype==GX_NRM_XYZ
		&& (compsize==GX_S8 || compsize==GX_S16 || compsize==GX_F32)) {
		_gx[vat0] = (_gx[vat0]&~0x200);
		_gx[vat0] = (_gx[vat0]&~0x1C00)|(_SHIFTL(compsize,10,3));
		_gx[vat0] = (_gx[vat0]&~0x80000000);
	} else if(vtxattr==GX_VA_NBT && (comptype==GX_NRM_NBT || comptype==GX_NRM_NBT3)
		&& (compsize==GX_S8 || compsize==GX_S16 || compsize==GX_F32)) {
		_gx[vat0] = (_gx[vat0]&~0x200)|0x200;
		_gx[vat0] = (_gx[vat0]&~0x1C00)|(_SHIFTL(compsize,10,3));
		if(comptype==GX_NRM_NBT3)
			_gx[vat0] = (_gx[vat0]&~0x80000000)|0x80000000;
	} else if(vtxattr==GX_VA_CLR0 && (comptype==GX_CLR_RGB || comptype==GX_CLR_RGBA)
		&& (compsize>=GX_RGB565 && compsize<=GX_RGBA8)) {
		_gx[vat0] = (_gx[vat0]&~0x2000)|(_SHIFTL(comptype,13,1));
		_gx[vat0] = (_gx[vat0]&~0x1C000)|(_SHIFTL(compsize,14,3));
	} else if(vtxattr==GX_VA_CLR1 && (comptype==GX_CLR_RGB || comptype==GX_CLR_RGBA)
		&& (compsize>=GX_RGB565 && compsize<=GX_RGBA8)) {
		_gx[vat0] = (_gx[vat0]&~0x20000)|(_SHIFTL(comptype,17,1));
		_gx[vat0] = (_gx[vat0]&~0x1C0000)|(_SHIFTL(compsize,18,3));
	} else if(vtxattr==GX_VA_TEX0 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat0] = (_gx[vat0]&~0x200000)|(_SHIFTL(comptype,21,1));
		_gx[vat0] = (_gx[vat0]&~0x1C00000)|(_SHIFTL(compsize,22,3));
		_gx[vat0] = (_gx[vat0]&~0x3E000000)|(_SHIFTL(frac,25,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX1 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat1] = (_gx[vat1]&~0x1)|(comptype&1);
		_gx[vat1] = (_gx[vat1]&~0xe)|(_SHIFTL(compsize,1,3));
		_gx[vat1] = (_gx[vat1]&~0x1F0)|(_SHIFTL(frac,4,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX2 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat1] = (_gx[vat1]&~0x200)|(_SHIFTL(comptype,9,1));
		_gx[vat1] = (_gx[vat1]&~0x1C00)|(_SHIFTL(compsize,10,3));
		_gx[vat1] = (_gx[vat1]&~0x3E000)|(_SHIFTL(frac,13,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX3 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat1] = (_gx[vat1]&~0x40000)|(_SHIFTL(comptype,18,1));
		_gx[vat1] = (_gx[vat1]&~0x380000)|(_SHIFTL(compsize,19,3));
		_gx[vat1] = (_gx[vat1]&~0x7C00000)|(_SHIFTL(frac,22,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX4 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat1] = (_gx[vat1]&~0x8000000)|(_SHIFTL(comptype,27,1));
		_gx[vat1] = (_gx[vat1]&~0x70000000)|(_SHIFTL(compsize,28,3));
		_gx[vat2] = (_gx[vat2]&~0x1f)|(frac&0x1f);
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX5 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat2] = (_gx[vat2]&~0x20)|(_SHIFTL(comptype,5,1));
		_gx[vat2] = (_gx[vat2]&~0x1C0)|(_SHIFTL(compsize,6,3));
		_gx[vat2] = (_gx[vat2]&~0x3E00)|(_SHIFTL(frac,9,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX6 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat2] = (_gx[vat2]&~0x4000)|(_SHIFTL(comptype,14,1));
		_gx[vat2] = (_gx[vat2]&~0x38000)|(_SHIFTL(compsize,15,3));
		_gx[vat2] = (_gx[vat2]&~0x7C0000)|(_SHIFTL(frac,18,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX7 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		_gx[vat2] = (_gx[vat2]&~0x800000)|(_SHIFTL(comptype,23,1));
		_gx[vat2] = (_gx[vat2]&~0x7000000)|(_SHIFTL(compsize,24,3));
		_gx[vat2] = (_gx[vat2]&~0xF8000000)|(_SHIFTL(frac,27,5));
		if(frac)
			_gx[vat0] = (_gx[vat0]&~0x40000000)|0x40000000;
	}
}

void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac)
{
	__SETVCDFMT(vtxfmt,vtxattr,comptype,compsize,frac);
	_gx[0x02] |= (1<<vtxfmt);
	_gx[0x09] |= 0x0010;
}

void GX_SetVtxAttrFmtv(u8 vtxfmt,GXVtxAttrFmt *attr_list)
{
	u32 i;

	for(i=0;i<GX_MAX_VTXATTRFMT_LISTSIZE;i++) {
		if(attr_list[i].vtxattr==GX_VA_NULL) break;

		__SETVCDFMT(vtxfmt,attr_list[i].vtxattr,attr_list[i].comptype,attr_list[i].compsize,attr_list[i].frac);
	}
	_gx[0x02] |= (1<<vtxfmt);
	_gx[0x09] |= 0x0010;
}

void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt)
{
	u8 reg = primitve|(vtxfmt&7);
	
	if(_gx[0x09]) 
		__GX_SetDirtyState();

	if(!_gx[0x00])
		__GX_SendFlushPrim();

	FIFO_PUTU8(reg);
	FIFO_PUTU16(vtxcnt);
}

void GX_End()
{

}

void GX_Position3f32(f32 x,f32 y,f32 z)
{
	FIFO_PUTF32(x);
	FIFO_PUTF32(y);
	FIFO_PUTF32(z);
}

void GX_Position3u16(u16 x,u16 y,u16 z)
{
	FIFO_PUTU16(x);
	FIFO_PUTU16(y);
	FIFO_PUTU16(z);
}

void GX_Position3s16(s16 x,s16 y,s16 z)
{
	FIFO_PUTS16(x);
	FIFO_PUTS16(y);
	FIFO_PUTS16(z);
}

void GX_Position3u8(u8 x,u8 y,u8 z)
{
	FIFO_PUTU8(x);
	FIFO_PUTU8(y);
	FIFO_PUTU8(z);
}

void GX_Position3s8(s8 x,s8 y,s8 z)
{
	FIFO_PUTS8(x);
	FIFO_PUTS8(y);
	FIFO_PUTS8(z);
}

void GX_Position2f32(f32 x,f32 y)
{
	FIFO_PUTF32(x);
	FIFO_PUTF32(y);
}

void GX_Position2u16(u16 x,u16 y)
{
	FIFO_PUTU16(x);
	FIFO_PUTU16(y);
}

void GX_Position2s16(s16 x,s16 y)
{
	FIFO_PUTS16(x);
	FIFO_PUTS16(y);
}

void GX_Position2u8(u8 x,u8 y)
{
	FIFO_PUTU8(x);
	FIFO_PUTU8(y);
}

void GX_Position2s8(s8 x,s8 y)
{
	FIFO_PUTS8(x);
	FIFO_PUTS8(y);
}

void GX_Position1x8(u8 index)
{
	FIFO_PUTU8(index);
}

void GX_Position1x16(u16 index)
{
	FIFO_PUTU16(index);
}

void GX_Normal3f32(f32 nx,f32 ny,f32 nz)
{
	FIFO_PUTF32(nx);
	FIFO_PUTF32(ny);
	FIFO_PUTF32(nz);
}

void GX_Normal3s16(s16 nx,s16 ny,s16 nz)
{
	FIFO_PUTS16(nx);
	FIFO_PUTS16(ny);
	FIFO_PUTS16(nz);
}

void GX_Normal3s8(s8 nx,s8 ny,s8 nz)
{
	FIFO_PUTS8(nx);
	FIFO_PUTS8(ny);
	FIFO_PUTS8(nz);
}

void GX_Normal1x8(u8 index)
{
	FIFO_PUTU8(index);
}

void GX_Normal1x16(u16 index)
{
	FIFO_PUTU16(index);
}

void GX_Color4u8(u8 r,u8 g,u8 b,u8 a)
{
	FIFO_PUTU8(r);
	FIFO_PUTU8(g);
	FIFO_PUTU8(b);
	FIFO_PUTU8(a);
}

void GX_Color3u8(u8 r,u8 g,u8 b)
{
	FIFO_PUTU8(r);
	FIFO_PUTU8(g);
	FIFO_PUTU8(b);
}

void GX_Color1u32(u32 clr)
{
	FIFO_PUTU32(clr);
}

void GX_Color1u16(u16 clr)
{
	FIFO_PUTU16(clr);
}

void GX_Color1x8(u8 index)
{
	FIFO_PUTU8(index);
}

void GX_Color1x16(u16 index)
{
	FIFO_PUTU16(index);
}

void GX_TexCoord2f32(f32 s,f32 t)
{
	FIFO_PUTF32(s);
	FIFO_PUTF32(t);
}

void GX_TexCoord2u16(u16 s,u16 t)
{
	FIFO_PUTU16(s);
	FIFO_PUTU16(t);
}

void GX_TexCoord2s16(s16 s,s16 t)
{
	FIFO_PUTS16(s);
	FIFO_PUTS16(t);
}

void GX_TexCoord2u8(u8 s,u8 t)
{
	FIFO_PUTU8(s);
	FIFO_PUTU8(t);
}

void GX_TexCoord2s8(s8 s,s8 t)
{
	FIFO_PUTS8(s);
	FIFO_PUTS8(t);
}

void GX_TexCoord1f32(f32 s)
{
	FIFO_PUTF32(s);
}

void GX_TexCoord1u16(u16 s)
{
	FIFO_PUTU16(s);
}

void GX_TexCoord1s16(s16 s)
{
	FIFO_PUTS16(s);
}

void GX_TexCoord1u8(u8 s)
{
	FIFO_PUTU8(s);
}

void GX_TexCoord1s8(s8 s)
{
	FIFO_PUTS8(s);
}

void GX_TexCoord1x8(u8 index)
{
	FIFO_PUTU8(index);
}

void GX_TexCoord1x16(u16 index)
{
	FIFO_PUTU16(index);
}

void GX_MatrixIndex1x8(u8 index)
{
	FIFO_PUTU8(index);
}

void GX_SetTexCoordGen(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc)
{
		GX_SetTexCoordGen2(texcoord,tgen_typ,tgen_src,mtxsrc,GX_FALSE,GX_DTTIDENTITY);
}

void GX_SetTexCoordGen2(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc,u32 normalize,u32 postmtx)
{
	u32 texcoords = 0;
	u32 dttexcoords = 0;
	u8 vtxrow,stq = 0;

	if(texcoord>=GX_MAXCOORD) return;
	
	switch(tgen_src) {
		case GX_TG_POS:
			vtxrow = 0;
			stq = 1;
			break;
		case GX_TG_NRM:
			vtxrow = 1;
			stq = 1;
			break;
		case GX_TG_BINRM:
			vtxrow = 3;
			stq = 1;
			break;
		case GX_TG_TANGENT:
			vtxrow = 4;
			stq = 1;
			break;
		case GX_TG_COLOR0:
			vtxrow = 2;
			break;
		case GX_TG_COLOR1:
			vtxrow = 2;
			break;
		case GX_TG_TEX0:
			vtxrow = 5;
			break;
		case GX_TG_TEX1:
			vtxrow = 6;
			break;
		case GX_TG_TEX2:
			vtxrow = 7;
			break;
		case GX_TG_TEX3:
			vtxrow = 8;
			break;
		case GX_TG_TEX4:
			vtxrow = 9;
			break;
		case GX_TG_TEX5:
			vtxrow = 10;
			break;
		case GX_TG_TEX6:
			vtxrow = 11;
			break;
		case GX_TG_TEX7:
			vtxrow = 12;
			break;
		default:
			vtxrow = 5;
			stq = 0;
			break;
	}
	texcoords = (texcoords&~0xF80)|(_SHIFTL(vtxrow,7,5));

	texcoords = (texcoords&~0x70);
	if((tgen_typ==GX_TG_MTX3x4 || tgen_typ==GX_TG_MTX2x4)
		&& (tgen_src>=GX_TG_POS && tgen_src<=GX_TG_TEX7)
		&& ((mtxsrc>=GX_TEXMTX0 && mtxsrc<=GX_TEXMTX9)
		|| mtxsrc==GX_IDENTITY)) {

		texcoords = (texcoords&~0x2);
		if(tgen_typ==GX_TG_MTX3x4) texcoords |= 0x2;
		
		texcoords = (texcoords&~0x4)|(_SHIFTL(stq,2,1));
		if(tgen_src>=GX_TG_TEX0 && tgen_src<=GX_TG_TEX7) {
			switch(tgen_src) {
				case GX_TG_TEX0:
					_gx[0x03] = (_gx[0x03]&~0xfc0)|(_SHIFTL(mtxsrc,6,6));
					break;
				case GX_TG_TEX1:
					_gx[0x03] = (_gx[0x03]&~0x3f000)|(_SHIFTL(mtxsrc,12,6));
					break;
				case GX_TG_TEX2:
					_gx[0x03] = (_gx[0x03]&~0xfc0000)|(_SHIFTL(mtxsrc,18,6));
					break;
				case GX_TG_TEX3:
					_gx[0x03] = (_gx[0x03]&~0x3f000000)|(_SHIFTL(mtxsrc,24,6));
					break;
				case GX_TG_TEX4:
					_gx[0x04] = (_gx[0x04]&~0x3f)|(mtxsrc&0x3f);
					break;
				case GX_TG_TEX5:
					_gx[0x04] = (_gx[0x04]&~0xfc0)|(_SHIFTL(mtxsrc,6,6));
					break;
				case GX_TG_TEX6:
					_gx[0x04] = (_gx[0x04]&~0x3f000)|(_SHIFTL(mtxsrc,12,6));
					break;
				case GX_TG_TEX7:
					_gx[0x04] = (_gx[0x04]&~0xfc0000)|(_SHIFTL(mtxsrc,18,6));
					break;
			}
		}
	} else if((tgen_typ>=GX_TG_BUMP0 && tgen_typ<=GX_TG_BUMP7)
		&& (tgen_src>=GX_TG_TEXCOORD0 && tgen_src<=GX_TG_TEXCOORD6
		&& tgen_src!=texcoord)) {
		texcoords |= 0x10;
		tgen_src -= GX_TG_TEXCOORD0;
		tgen_typ -= GX_TG_BUMP0;
		texcoords = (texcoords&~0x7000)|(_SHIFTL(tgen_src,12,3));
		texcoords = (texcoords&~0x38000)|(_SHIFTL(tgen_typ,15,3));
	} else if(tgen_typ==GX_TG_SRTG && (tgen_src==GX_TG_COLOR0 || tgen_src==GX_TG_COLOR1)) {
		if(tgen_src==GX_TG_COLOR0) texcoords |= 0x20;
		else if(tgen_src==GX_TG_COLOR1) texcoords |= 0x30;
	}

	GX_LOAD_XF_REG(texcoord+0x1040,texcoords);

	postmtx -= GX_DTTMTX0;
	dttexcoords = (dttexcoords&~0x3f)|(postmtx&0x3f);
	dttexcoords = (dttexcoords&~0x100)|(_SHIFTL(normalize,8,1));
	GX_LOAD_XF_REG(texcoord+0x1050,dttexcoords);

	__GX_SetMatrixIndex(texcoord+1);
}
 
void GX_SetZTexture(u8 op,u8 fmt,u32 bias)
{
	u32 val = 0;
	
	if(fmt==GX_TF_Z8) fmt = 0;
	else if(fmt==GX_TF_Z16) fmt = 1;
	else fmt = 2;
	
	val = (u32)(_SHIFTL(op,2,2))|(fmt&3);
	GX_LOAD_BP_REG(0xF4000000|(bias&0x00FFFFFF));
	GX_LOAD_BP_REG(0xF5000000|(val&0x00FFFFFF));
}

static void WriteMtxPS4x3(register Mtx mt,register void *wgpipe)
{
	__asm__ __volatile__
		("psq_l 0,0(%0),0,0\n\
		  psq_l 1,8(%0),0,0\n\
		  psq_l 2,16(%0),0,0\n\
		  psq_l 3,24(%0),0,0\n\
		  psq_l 4,32(%0),0,0\n\
		  psq_l 5,40(%0),0,0\n\
		  psq_st 0,0(%1),0,0\n\
		  psq_st 1,0(%1),0,0\n\
		  psq_st 2,0(%1),0,0\n\
		  psq_st 3,0(%1),0,0\n\
		  psq_st 4,0(%1),0,0\n\
		  psq_st 5,0(%1),0,0"
		  : : "r"(mt), "r"(wgpipe));
}

static void WriteMtxPS3x3from4x3(register Mtx mt,register void *wgpipe)
{
	__asm__ __volatile__
		("psq_l 0,0(%0),0,0\n\
		  lfs 1,8(%0)\n\
		  psq_l 2,16(%0),0,0\n\
		  lfs 3,24(%0)\n\
		  psq_l 4,32(%0),0,0\n\
		  lfs 5,40(%0)\n\
		  psq_st 0,0(%1),0,0\n\
		  stfs 1,0(%1)\n\
		  psq_st 2,0(%1),0,0\n\
		  stfs 3,0(%1)\n\
		  psq_st 4,0(%1),0,0\n\
		  stfs 5,0(%1)"
		  : : "r"(mt), "r"(wgpipe));
}

static void WriteMtxPS3x3(register Mtx33 mt,register void *wgpipe)
{
	__asm__ __volatile__
		("psq_l 0,0(%0),0,0\n\
		  psq_l 1,8(%0),0,0\n\
		  psq_l 2,16(%0),0,0\n\
		  psq_l 3,24(%0),0,0\n\
		  lfs 4,32(%0)\n\
		  psq_st 0,0(%1),0,0\n\
		  psq_st 1,0(%1),0,0\n\
		  psq_st 2,0(%1),0,0\n\
		  psq_st 3,0(%1),0,0\n\
		  stfs 4,0(%1)"
		  : : "r"(mt), "r"(wgpipe));
}

static void WriteMtxPS4x2(register Mtx mt,register void *wgpipe)
{
	__asm__ __volatile__
		("psq_l 0,0(%0),0,0\n\
		  psq_l 1,8(%0),0,0\n\
		  psq_l 2,16(%0),0,0\n\
		  psq_l 3,24(%0),0,0\n\
		  psq_st 0,0(%1),0,0\n\
		  psq_st 1,0(%1),0,0\n\
		  psq_st 2,0(%1),0,0\n\
		  psq_st 3,0(%1),0,0"
		  : : "r"(mt), "r"(wgpipe));
}

void GX_LoadPosMtxImm(Mtx mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0000|(_SHIFTL(pnidx,2,8))),12);
	WriteMtxPS4x3(mt,(void*)WGPIPE);
}

void GX_LoadPosMtxIdx(u16 mtxidx,u32 pnidx)
{
	FIFO_PUTU8(0x20);
	FIFO_PUTU32(((_SHIFTL(mtxidx,16,16))|0xb000|(_SHIFTL(pnidx,2,8))));
}

void GX_LoadNrmMtxImm(Mtx mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0400|(pnidx*3)),9);
	WriteMtxPS3x3from4x3(mt,(void*)WGPIPE);
}

void GX_LoadNrmMtxImm3x3(Mtx33 mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0400|(pnidx*3)),9);
	WriteMtxPS3x3(mt,(void*)WGPIPE);
}

void GX_LoadNrmMtxIdx3x3(u16 mtxidx,u32 pnidx)
{
	FIFO_PUTU8(0x28);
	FIFO_PUTU32(((_SHIFTL(mtxidx,16,16))|0x8000|(0x0400|(pnidx*3))));
}

void GX_LoadTexMtxImm(Mtx mt,u32 texidx,u8 type)
{
	u32 addr;
	u32 rows = (type==GX_MTX2x4)?2:3;

	if(texidx<GX_DTTMTX0) addr = 0x0000|(_SHIFTL(texidx,2,8));
	else addr = 0x0500|(_SHIFTL((texidx-GX_DTTMTX0),2,8));

	GX_LOAD_XF_REGS(addr,(rows*4));
	if(type==GX_MTX2x4)
		WriteMtxPS4x2(mt,(void*)WGPIPE);
	else
		WriteMtxPS4x3(mt,(void*)WGPIPE);
}

void GX_LoadTexMtxIdx(u16 mtxidx,u32 texidx,u8 type)
{
	u32 addr,size = (type==GX_MTX2x4)?7:11;

	if(texidx<GX_DTTMTX0) addr = 0x0000|(_SHIFTL(texidx,2,8));
	else addr = 0x0500|(_SHIFTL((texidx-GX_DTTMTX0),2,8));

	FIFO_PUTU8(0x30);
	FIFO_PUTU32(((_SHIFTL(mtxidx,16,16))|(_SHIFTL(size,12,4))|addr));
}

void GX_SetCurrentMtx(u32 mtx)
{
	_gx[0x03] = (_gx[0x03]&~0x3f)|(mtx&0x3f);
	__GX_SetMatrixIndex(0);
}

void GX_SetNumTexGens(u32 nr)
{
	_gx[0xac] = (_gx[0xac]&~0xf)|(nr&0xf);
	GX_LOAD_XF_REG(0x0000103f,(_gx[0xac]&0xf));
	_gx[0x09] |= 0x0004;
}

void GX_InvVtxCache()
{
	FIFO_PUTU8(0x48); // vertex cache weg
}

void GX_SetZMode(u8 enable,u8 func,u8 update_enable)
{
	_gx[0xb8] = (_gx[0xb8]&~0x1)|(enable&1);
	_gx[0xb8] = (_gx[0xb8]&~0xe)|(_SHIFTL(func,1,3));
	_gx[0xb8] = (_gx[0xb8]&~0x10)|(_SHIFTL(update_enable,4,1));
	GX_LOAD_BP_REG(_gx[0xb8]);
}

static void __GetTexTileShift(u32 fmt,u32 *xshift,u32 *yshift)
{
	switch(fmt) {
		case GX_TF_I4:
		case GX_TF_IA4:
		case GX_CTF_R4:
		case GX_CTF_RA4:
		case GX_CTF_Z4:
			*xshift = 3;
			*yshift = 3;
			break;
		case GX_TF_Z8:
		case GX_TF_I8:
		case GX_TF_IA8:
		case GX_CTF_RA8:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
			*xshift = 3;
			*yshift = 2;
			break;
		case GX_TF_Z16:
		case GX_TF_Z24X8:
		case GX_CTF_Z16L:
		case GX_TF_RGB565:
		case GX_TF_RGB5A3:
		case GX_TF_RGBA8:
			*xshift = 2;
			*yshift = 2;
			break;
		default:
			*xshift = 0;
			*yshift = 0;
			break;
	}
}

u8 GX_GetTexFmt(GXTexObj *obj)
{
	return obj->val[5];	
}

u32 GX_GetTexBufferSize(u16 wd,u16 ht,u32 fmt,u8 mipmap,u8 maxlod)
{
	u32 xshift,yshift,xtiles,ytiles,bitsize,size;
	
	switch(fmt) {
		case GX_TF_I4:
		case GX_TF_IA4:
		case GX_CTF_R4:
		case GX_CTF_RA4:
		case GX_CTF_Z4:
			xshift = 3;
			yshift = 3;
			break;
		case GX_TF_Z8:
		case GX_TF_I8:
		case GX_TF_IA8:
		case GX_CTF_RA8:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
			xshift = 3;
			yshift = 2;
			break;
		case GX_TF_Z16:
		case GX_TF_Z24X8:
		case GX_CTF_Z16L:
		case GX_TF_RGB565:
		case GX_TF_RGB5A3:
		case GX_TF_RGBA8:
			xshift = 2;
			yshift = 2;
			break;
		default:
			xshift = 0;
			yshift = 0;
			break;
	}

	bitsize = 32;
	if(fmt==GX_TF_RGBA8 || fmt==GX_TF_Z24X8) bitsize = 64;

	size = 0;
	if(mipmap) {
		u32 cnt = (maxlod&0xff);
		while(cnt) {
			u32 w = wd&0xffff;
			u32 h = ht&0xffff;
			xtiles = (w+((1<<xshift)-1))>>xshift;
			ytiles = (h+((1<<yshift)-1))>>yshift;
			if(cnt==0) return size;

			size += ((xtiles*ytiles)*bitsize);
			if(w==0x0001 && h==0x0001) return size;
			if(wd>0x0001) wd = (w>>1);
			else wd = 0x0001;
			if(ht>0x0001) ht = (h>>1);
			else ht = 0x0001;
		}
		return size;
	}

	wd &= 0xffff;
	xtiles = (wd+((1<<xshift)-1))>>xshift;
	
	ht &= 0xffff;
	ytiles = (ht+((1<<yshift)-1))>>yshift;

	size = ((xtiles*ytiles)*bitsize);

	return size;
}

void GX_InitTexCacheRegion(GXTexRegion *region,u8 is32bmipmap,u32 tmem_even,u8 size_even,u32 tmem_odd,u8 size_odd)
{
	u32 sze = 0;
	switch(size_even) {
		case GX_TEXCACHE_32K:
			sze = 3;
			break;
		case GX_TEXCACHE_128K:
			sze = 4;
			break;
		case GX_TEXCACHE_512K:
			sze = 5;
			break;
		default:
			sze = -1;
			return;
	}
	region->val[0] = 0;
	region->val[0] = (region->val[0]&~0x7fff)|(_SHIFTR(tmem_even,5,15));
	region->val[0] = (region->val[0]&~0x38000)|(_SHIFTL(sze,15,3));
	region->val[0] = (region->val[0]&~0x1C0000)|(_SHIFTL(sze,18,3));

	switch(size_odd) {
		case GX_TEXCACHE_NONE:
			sze = 0;
			break;
		case GX_TEXCACHE_32K:
			sze = 3;
			break;
		case GX_TEXCACHE_128K:
			sze = 4;
			break;
		case GX_TEXCACHE_512K:
			sze = 5;
			break;
		default:
			sze = -1;
			return;
	}
	region->val[1] = 0;
	region->val[1] = (region->val[1]&~0x7fff)|(_SHIFTR(tmem_odd,5,15));
	region->val[1] = (region->val[1]&~0x38000)|(_SHIFTL(sze,15,3));
	region->val[1] = (region->val[1]&~0x1C0000)|(_SHIFTL(sze,18,3));
	((u8*)region->val)[12] = is32bmipmap;
	((u8*)region->val)[13] = 1;
}

void GX_InitTexPreloadRegion(GXTexRegion *region,u32 tmem_even,u32 size_even,u32 tmem_odd,u32 size_odd)
{
	region->val[0] = 0;
	region->val[0] = (region->val[0]&~0x7FFF)|(_SHIFTR(tmem_even,5,15));
	region->val[0] = (region->val[0]&~0x38000);
	region->val[0] = (region->val[0]&~0x1C0000);
	region->val[0] = (region->val[0]&~0x200000)|0x200000;
	
	region->val[1] = 0;
	region->val[1] = (region->val[1]&~0x7FFF)|(_SHIFTR(tmem_odd,5,15));
	region->val[1] = (region->val[0]&~0x38000);
	region->val[1] = (region->val[0]&~0x1C0000);
	
	((u8*)region->val)[12] = 0;
	((u8*)region->val)[13] = 0;

	((u16*)region->val)[4] = _SHIFTR(size_even,5,16);
	((u16*)region->val)[5] = _SHIFTR(size_odd,5,16);
}

void GX_InitTlutRegion(GXTlutRegion *region,u32 tmem_addr,u8 tlut_sz)
{
	region->val[0] = 0;
	tmem_addr -= 0x80000;
	region->val[0] = (region->val[0]&~0x3ff)|(_SHIFTR(tmem_addr,9,10));
	region->val[0] = (region->val[0]&~0x1FFC00)|(_SHIFTL(tlut_sz,10,10));
	region->val[0] = (region->val[0]&~0xff000000)|(_SHIFTL(0x65,24,8));
}

void GX_InitTexObj(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap)
{
	u8 tmp1,tmp2;
	u32 nwd,nht,res;

	if(!obj) return;

	memset(obj,0,sizeof(GXTexObj));
	obj->val[0] = (obj->val[0]&~0x03)|(wrap_s&3);
	obj->val[0] = (obj->val[0]&~0x0c)|(_SHIFTL(wrap_t,2,2));
	obj->val[0] = (obj->val[0]&~0x10)|0x10;
	
	if(mipmap) {
		((u8*)obj->val)[31] |= 0x0001;
		if(fmt==0x0008 || fmt==0x0009 || fmt==0x000a) 
			obj->val[0] = (obj->val[0]&~0xe0)|0x00a0;
		else
			obj->val[0] = (obj->val[0]&~0xe0)|0x00c0;
	} else 
		obj->val[0]= (obj->val[0]&~0xE0)|0x0080;
	
	obj->val[5] = fmt;
	obj->val[2] = (obj->val[2]&~0x3ff)|((wd-1)&0x3ff);
	obj->val[2] = (obj->val[2]&~0xFFC00)|(_SHIFTL((ht-1),10,10));
	obj->val[2] = (obj->val[2]&~0xF00000)|(_SHIFTL(fmt,20,4));
	obj->val[3] = (obj->val[3]&~0x01ffffff)|(_SHIFTR((((u32)img_ptr)&~0xc0000000),5,24));

	tmp1 = 2;
	tmp2 = 2;
	((u8*)obj->val)[30] = 2;
	if(fmt<=GX_TF_CMPR) {
		tmp1 = 3;
		tmp2 = 3;
		((u8*)obj->val)[30] = 1;
	}

	nwd = ((1<<tmp1)-1)+wd;
	nwd >>= tmp1;
	nht = ((1<<tmp2)-1)+ht;
	nht >>= tmp2;
	res = nwd*nht;
	((u16*)obj->val)[14] = res&0x7fff;

	((u8*)obj->val)[31] |= 0x0002;
}

void GX_InitTexObjCI(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap,u32 tlut_name)
{
	u8 flag;

	GX_InitTexObj(obj,img_ptr,wd,ht,fmt,wrap_s,wrap_t,mipmap);
	flag = ((u8*)obj->val)[31];
	((u8*)obj->val)[31] = flag&~0x2;
	obj->val[6] = tlut_name;
}

void GX_InitTexObjTlut(GXTexObj *obj,u32 tlut_name)
{
	obj->val[6] = tlut_name;
}

void GX_InitTexObjLOD(GXTexObj *obj,u8 minfilt,u8 magfilt,f32 minlod,f32 maxlod,f32 lodbias,u8 biasclamp,u8 edgelod,u8 maxaniso)
{
	static u8 GX2HWFiltConv[] = {0x00,0x04,0x01,0x05,0x02,0x06,0x00,0x00};
	//static u8 HW2GXFiltConv[] = {0x00,0x02,0x04,0x00,0x01,0x03,0x05,0x00};
	
	if(lodbias<-4.0f) lodbias = -4.0f;
	else if(lodbias==4.0f) lodbias = 3.99f;
	
	obj->val[0] = (obj->val[0]&~0x1fe00)|(_SHIFTL(((u32)(32.0f*lodbias)),9,8));
	obj->val[0] = (obj->val[0]&~0x10)|(_SHIFTL((magfilt==GX_LINEAR?1:0),4,1));
	obj->val[0] = (obj->val[0]&~0xe0)|(_SHIFTL(GX2HWFiltConv[minfilt],5,3));
	obj->val[0] = (obj->val[0]&~0x100)|(_SHIFTL(!(edgelod&0xff),8,1));
	obj->val[0] = (obj->val[0]&~0x180000)|(_SHIFTL(maxaniso,19,2));
	obj->val[0] = (obj->val[0]&~0x200000)|(_SHIFTL(biasclamp,21,1));
	
	if(minlod<0.0f) minlod = 0.0f;
	else if(minlod>10.0f) minlod = 10.0f;

	if(maxlod<0.0f) maxlod = 0.0f;
	else if(maxlod>10.0f) maxlod = 10.0f;

	obj->val[1] = (obj->val[1]&~0xff)|(((u32)(16.0f*minlod))&0xff);
	obj->val[1] = (obj->val[1]&~0xff00)|(_SHIFTL(((u32)(16.0f*maxlod)),8,8));
}

void GX_InitTlutObj(GXTlutObj *obj,void *lut,u8 fmt,u16 entries)
{
	memset(obj,0,sizeof(GXTlutObj));
	obj->val[0] = (obj->val[0]&~0xC00)|(_SHIFTL(fmt,10,2));
	obj->val[1] = (obj->val[1]&~0x01ffffff)|(_SHIFTR((((u32)lut)&~0xc0000000),5,24));
	obj->val[1] = (obj->val[1]&~0xff000000)|(_SHIFTL(0x64,24,8));
	((u16*)obj->val)[4] = entries;
}

void GX_LoadTexObj(GXTexObj *obj,u8 mapid)
{
	GXTexRegion *region = NULL;

	if(regionCB)
		region = regionCB(obj,mapid);
	
	GX_LoadTexObjPreloaded(obj,region,mapid);
}

void GX_LoadTexObjPreloaded(GXTexObj *obj,GXTexRegion *region,u8 mapid)
{
	u8 type;
	GXTlutRegion *tlut = NULL;

	obj->val[0] = (obj->val[0]&~0xff000000)|(_SHIFTL(_gxtexmode0ids[mapid],24,8));
	obj->val[1] = (obj->val[1]&~0xff000000)|(_SHIFTL(_gxtexmode1ids[mapid],24,8));
	obj->val[2] = (obj->val[2]&~0xff000000)|(_SHIFTL(_gxteximg0ids[mapid],24,8));
	obj->val[3] = (obj->val[3]&~0xff000000)|(_SHIFTL(_gxteximg3ids[mapid],24,8));
	
	region->val[0] = (region->val[0]&~0xff000000)|(_SHIFTL(_gxteximg1ids[mapid],24,8));
	region->val[1] = (region->val[1]&~0xff000000)|(_SHIFTL(_gxteximg2ids[mapid],24,8));

	GX_LOAD_BP_REG(obj->val[0]);
	GX_LOAD_BP_REG(obj->val[1]);
	GX_LOAD_BP_REG(obj->val[2]);

	GX_LOAD_BP_REG(region->val[0]);
	GX_LOAD_BP_REG(region->val[1]);

	GX_LOAD_BP_REG(obj->val[3]);

	type = ((u8*)obj->val)[31];
	if(!(type&0x0002)) {
		if(tlut_regionCB)
			tlut = tlut_regionCB(obj->val[6]);
		tlut->val[1] = (tlut->val[1]&~0xff000000)|(_SHIFTL(_gxtextlutids[mapid],24,8));
		GX_LOAD_BP_REG(tlut->val[1]);
	}
	
	_gx[0x40+mapid] = obj->val[2];
	_gx[0x50+mapid] = obj->val[0];
	
	_gx[0x09] |= 0x0001;
}

void GX_PreloadEntireTex(GXTexObj *obj,GXTexRegion *region)
{
}

void GX_InvalidateTexAll()
{
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x66001000);
	GX_LOAD_BP_REG(0x66001100);
	__GX_FlushTextureState();
}

void GX_InvalidateTexRegion(GXTexRegion *region)
{
	u8 ismipmap;
	s32 cw_e,ch_e,cw_o,ch_o;
	u32 size,tmp,regvalA = 0,regvalB = 0;

	cw_e = (_SHIFTR(region->val[0],15,3))-1;
	ch_e = (_SHIFTR(region->val[0],18,3))-1;

	cw_o = (_SHIFTR(region->val[1],15,3))-1;
	ch_o = (_SHIFTR(region->val[1],18,3))-1;

	if(cw_e<0) cw_e = 0;
	if(ch_e<0) ch_e = 0;
	if(cw_o<0) cw_o = 0;
	if(ch_o<0) ch_o = 0;
	
	ismipmap = ((u8*)region->val)[12];
	
	tmp = size = cw_e+ch_e;
	if(ismipmap) size = tmp+(cw_o+ch_o-2);
	regvalA = _SHIFTR((region->val[0]&0x7fff),6,24)|(_SHIFTL(size,9,24))|(_SHIFTL(0x66,24,8));

	if(cw_o!=0) {
		size = cw_o+ch_o;
		if(ismipmap) size += (tmp-2);
		regvalB = _SHIFTR((region->val[1]&0x7fff),6,24)|(_SHIFTL(size,9,24))|(_SHIFTL(0x66,24,8));
	}
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(regvalA);
	if(ismipmap) GX_LOAD_BP_REG(regvalB);
	__GX_FlushTextureState();
}

void GX_LoadTlut(GXTlutObj *obj,u32 tlut_name)
{
	GXTlutRegion *region = NULL;

	if(tlut_regionCB)
		region = tlut_regionCB(tlut_name);

	__GX_FlushTextureState();
	GX_LOAD_BP_REG(obj->val[1]);
	GX_LOAD_BP_REG(region->val[0]);
	__GX_FlushTextureState();

	obj->val[0] = (region->val[0]&~0xFFFFFC00)|(obj->val[0]&0xFFFFFC00);
	region->val[1] = obj->val[0];
	region->val[2] = obj->val[1];
	region->val[3] = obj->val[2];
}

void GX_SetTexCoorScaleManually(u8 texcoord,u8 enable,u16 ss,u16 ts)
{
	u32 regA,regB;

	_gx[0x05] = (_gx[0x05]&~(_SHIFTL(1,texcoord,1)))|(_SHIFTL(enable,texcoord,1));
	if(!enable) return;

	regA = 0xa0+(texcoord&0x7);
	regB = 0xb0+(texcoord&0x7);

	_gx[regA] = (_gx[regA]&~0xffff)|((ss-1)&0xffff);
	_gx[regB] = (_gx[regB]&~0xffff)|((ts-1)&0xffff);

	GX_LOAD_BP_REG(_gx[regA]);
	GX_LOAD_BP_REG(_gx[regB]);
}

void GX_SetTexCoordCylWrap(u8 texcoord,u8 s_enable,u8 t_enable)
{
	u32 regA,regB;
	
	regA = 0xa0+(texcoord&0x7);
	regB = 0xb0+(texcoord&0x7);

	_gx[regA] = (_gx[regA]&~0x20000)|(_SHIFTL(s_enable,17,1));
	_gx[regB] = (_gx[regB]&~0x20000)|(_SHIFTL(t_enable,17,1));

	if(!(_gx[0x05]&(_SHIFTL(1,texcoord,1)))) return;
	
	GX_LOAD_BP_REG(_gx[regA]);
	GX_LOAD_BP_REG(_gx[regB]);
}

void GX_SetTexCoordBias(u8 texcoord,u8 s_enable,u8 t_enable)
{
	u32 regA,regB;
	
	regA = 0xa0+(texcoord&0x7);
	regB = 0xb0+(texcoord&0x7);

	_gx[regA] = (_gx[regA]&~0x10000)|(_SHIFTL(s_enable,16,1));
	_gx[regB] = (_gx[regB]&~0x10000)|(_SHIFTL(t_enable,16,1));

	if(!(_gx[0x05]&(_SHIFTL(1,texcoord,1)))) return;
	
	GX_LOAD_BP_REG(_gx[regA]);
	GX_LOAD_BP_REG(_gx[regB]);
}

GXTexRegionCallback GX_SetTexRegionCallback(GXTexRegionCallback cb)
{
	u32 level;
	GXTexRegionCallback ret;

	_CPU_ISR_Disable(level);
	ret = regionCB;
	regionCB = cb;
	_CPU_ISR_Restore(level);

	return ret;
}

GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback cb)
{
	u32 level;
	GXTlutRegionCallback ret;

	_CPU_ISR_Disable(level);
	ret = tlut_regionCB;
	tlut_regionCB = cb;
	_CPU_ISR_Restore(level);

	return ret;
}

void GX_SetBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op)
{
	_gx[0xb9] = (_gx[0xb9]&~0x1);
	if(type==GX_BM_BLEND || type==GX_BM_SUBSTRACT) _gx[0xb9] |= 0x1;
	
	_gx[0xb9] = (_gx[0xb9]&~0x800);
	if(type==GX_BM_SUBSTRACT) _gx[0xb9] |= 0x800;

	_gx[0xb9] = (_gx[0xb9]&~0x2);
	if(type==GX_BM_LOGIC) _gx[0xb9] |= 0x2;
	
	_gx[0xb9] = (_gx[0xb9]&~0xF000)|(_SHIFTL(op,12,4));
	_gx[0xb9] = (_gx[0xb9]&~0xE0)|(_SHIFTL(dst_fact,5,3));
	_gx[0xb9] = (_gx[0xb9]&~0x700)|(_SHIFTL(src_fact,8,3));

	GX_LOAD_BP_REG(_gx[0xb9]);
}

void GX_ClearVtxDesc()
{
	_gx[0x08] = 0;
	_gx[0x00] = ((_gx[0x00]&~0x0600)|0x0200);
	_gx[0x06] = _gx[0x07] = 0;
	_gx[0x09] |= 0x0008;
}

void GX_SetLineWidth(u8 width,u8 fmt)
{
	_gx[0xaa] = (_gx[0xaa]&~0xff)|(width&0xff);
	_gx[0xaa] = (_gx[0xaa]&~0x70000)|(_SHIFTL(fmt,16,3));
	GX_LOAD_BP_REG(_gx[0xaa]);
}

void GX_SetPointSize(u8 width,u8 fmt)
{
	_gx[0xaa] = (_gx[0xaa]&~0xFF00)|(_SHIFTL(width,8,8));
	_gx[0xaa] = (_gx[0xaa]&~0x380000)|(_SHIFTL(fmt,19,3));
	GX_LOAD_BP_REG(_gx[0xaa]);
}

void GX_SetTevColor(u8 tev_regid,GXColor color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_regid<<1)),24,8)|(_SHIFTL(color.a,12,8))|(color.r&0xff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_regid<<1)),24,8)|(_SHIFTL(color.g,12,8))|(color.b&0xff));
	GX_LOAD_BP_REG(reg);
	
	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevColorS10(u8 tev_regid,GXColor color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_regid<<1)),24,8)|(_SHIFTL(color.a,12,11))|(color.r&0x7ff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_regid<<1)),24,8)|(_SHIFTL(color.g,12,11))|(color.b&0x7ff));
	GX_LOAD_BP_REG(reg);
	
	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevKColor(u8 tev_regid,GXColor color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_regid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.a,12,8))|(color.r&0xff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_regid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.g,12,8))|(color.b&0xff));
	GX_LOAD_BP_REG(reg);
	
	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevKColorS10(u8 tev_regid,GXColor color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_regid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.a,12,11))|(color.r&0x7ff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_regid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.g,12,11))|(color.b&0x7ff));
	GX_LOAD_BP_REG(reg);
	
	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevOp(u8 tevstage,u8 mode)
{
	u8 defcolor = GX_CC_RASC;
	u8 defalpha = GX_CA_RASA;

	if(tevstage!=GX_TEVSTAGE0) {
		defcolor = GX_CC_CPREV;
		defalpha = GX_CA_APREV;
	}
	
	switch(mode) {
		case GX_MODULATE:
			GX_SetTevColorIn(tevstage,GX_CC_ZERO,GX_CC_TEXC,defcolor,GX_CC_ZERO);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_TEXA,defalpha,GX_CA_ZERO);
			break;
		case GX_DECAL:
			GX_SetTevColorIn(tevstage,defcolor,GX_CC_TEXC,GX_CC_TEXA,GX_CC_ZERO);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,defalpha);
			break;
		case GX_BLEND:
			GX_SetTevColorIn(tevstage,defcolor,GX_CC_ONE,GX_CC_TEXC,GX_CC_ZERO);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_TEXA,defalpha,GX_CA_RASA);
			break;
		case GX_REPLACE:
			GX_SetTevColorIn(tevstage,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_TEXC);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_TEXA);
			break;
		case GX_PASSCLR:
			GX_SetTevColorIn(tevstage,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,defcolor);
			GX_SetTevAlphaIn(tevstage,GX_CC_A2,GX_CC_A2,GX_CC_A2,defalpha);
			break;
	}
	GX_SetTevColorOp(tevstage,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
	GX_SetTevAlphaOp(tevstage,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
}

void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
{
	u32 reg = 0x80+(tevstage&0xf);
	_gx[reg] = (_gx[reg]&~0xF000)|(_SHIFTL(a,12,4));
	_gx[reg] = (_gx[reg]&~0xF00)|(_SHIFTL(b,8,4));
	_gx[reg] = (_gx[reg]&~0xF0)|(_SHIFTL(c,4,4));
	_gx[reg] = (_gx[reg]&~0xf)|(d&0xf);

	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
{
	u32 reg = 0x90+(tevstage&0xf);
	_gx[reg] = (_gx[reg]&~0xE000)|(_SHIFTL(a,13,3));
	_gx[reg] = (_gx[reg]&~0x1C00)|(_SHIFTL(b,10,3));
	_gx[reg] = (_gx[reg]&~0x380)|(_SHIFTL(c,7,3));
	_gx[reg] = (_gx[reg]&~0x70)|(_SHIFTL(d,4,3));

	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
{
	/* set tev op add/sub*/
	u32 reg = 0x80+(tevstage&0xf);
	_gx[reg] = (_gx[reg]&~0x40000)|(_SHIFTL(tevop,18,1));
	if(tevop<=GX_TEV_SUB) {
		_gx[reg] = (_gx[reg]&~0x300000)|(_SHIFTL(tevscale,20,2));
		_gx[reg] = (_gx[reg]&~0x30000)|(_SHIFTL(tevbias,16,2));
	} else {
		_gx[reg] = (_gx[reg]&~0x300000)|((_SHIFTL(tevop,19,4))&0x300000);
		_gx[reg] = (_gx[reg]&~0x30000)|0x30000;
	}
	_gx[reg] = (_gx[reg]&~0x80000)|(_SHIFTL(clamp,19,1));
	_gx[reg] = (_gx[reg]&~0xC00000)|(_SHIFTL(tevregid,22,2));
	
	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
{
	/* set tev op add/sub*/
	u32 reg = 0x90+(tevstage&0xf);
	_gx[reg] = (_gx[reg]&~0x40000)|(_SHIFTL(tevop,18,1));
	if(tevop<=GX_TEV_SUB) {
		_gx[reg] = (_gx[reg]&~0x300000)|(_SHIFTL(tevscale,20,2));
		_gx[reg] = (_gx[reg]&~0x30000)|(_SHIFTL(tevbias,16,2));
	} else {
		_gx[reg] = (_gx[reg]&~0x300000)|((_SHIFTL(tevop,19,4))&0x300000);
		_gx[reg] = (_gx[reg]&~0x30000)|0x30000;
	}
	_gx[reg] = (_gx[reg]&~0x80000)|(_SHIFTL(clamp,19,1));
	_gx[reg] = (_gx[reg]&~0xC00000)|(_SHIFTL(tevregid,22,2));
	
	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetCullMode(u8 mode)
{
    static u8 cm2hw[] = { 0, 2, 1, 3 };
	
	_gx[0xac] = (_gx[0xac]&~0xC000)|(_SHIFTL(cm2hw[mode],14,2));
	_gx[0x09] |= 0x0004;
}

void GX_SetCoPlanar(u8 enable)
{
	_gx[0xac] = (_gx[0xac]&~0x80000)|(_SHIFTL(enable,19,1));
	GX_LOAD_BP_REG(0xFE080000);
	GX_LOAD_BP_REG(_gx[0xac]);
}

void GX_EnableTexOffsets(u8 coord,u8 line_enable,u8 point_enable)
{
	u32 reg = 0xa0+(coord&0x7);
	_gx[reg] = (_gx[reg]&~0x40000)|(_SHIFTL(line_enable,18,1));
	_gx[reg] = (_gx[reg]&~0x80000)|(_SHIFTL(point_enable,19,1));
	GX_LOAD_BP_REG(_gx[reg]);
}
 
void GX_SetClipMode(u8 mode)
{
	GX_LOAD_XF_REG(0x1005,(mode&1));
}

void GX_SetScissor(u32 xOrigin,u32 yOrigin,u32 wd,u32 ht)
{
	u32 xo = xOrigin+0x156;
	u32 yo = yOrigin+0x156;
	u32 nwd = xo+(wd-1);
	u32 nht = yo+(ht-1);
	
	_gx[0xa8] = (_gx[0xa8]&~0x7ff)|(yo&0x7ff);
	_gx[0xa8] = (_gx[0xa8]&~0x7FF000)|(_SHIFTL(xo,12,11));

	_gx[0xa9] = (_gx[0xa9]&~0x7ff)|(nht&0xfff);
	_gx[0xa9] = (_gx[0xa9]&~0x7FF000)|(_SHIFTL(nwd,12,11));

	GX_LOAD_BP_REG(_gx[0xa8]);
	GX_LOAD_BP_REG(_gx[0xa9]);
}
 
void GX_SetScissorBoxOffset(s32 xoffset,s32 yoffset)
{
	s32 xoff = _SHIFTR((xoffset+0x156),1,24);
	s32 yoff = _SHIFTR((yoffset+0x156),1,24);

	GX_LOAD_BP_REG((0x59000000|(_SHIFTL(yoff,10,10))|(xoff&0x3ff)));
}

void GX_SetNumChans(u8 num)
{
	_gx[0xac] = (_gx[0xac]&~0x70)|(_SHIFTL(num,4,3));
	GX_LOAD_XF_REG(0x1009,(num&3));
	_gx[0x09] |= 0x0004;
}

void GX_SetTevOrder(u8 tevstage,u8 texcoord,u32 texmap,u8 color)
{
	u8 colid;
	u32 texm,texc,tmp;
	u32 reg = 0xc3+(_SHIFTR(tevstage,1,3));
	
	_gx[0x30+tevstage] = texmap;

	texm = (texmap&~0x100);
	if(texm>=GX_MAX_TEXMAP) texm = 0;
	if(texcoord>=GX_MAXCOORD) {
		texc = 0;
		_gx[0x0a] &= ~(_SHIFTL(1,tevstage,1));
	} else {
		texc = texcoord;
		_gx[0x0a] |= (_SHIFTL(1,tevstage,1));
	}

	if(tevstage&1) {
		_gx[reg] = (_gx[reg]&~0x7000)|(_SHIFTL(texm,12,3));
		_gx[reg] = (_gx[reg]&~0x38000)|(_SHIFTL(texc,15,3));
		
		colid = GX_BUMP;
		if(color!=GX_COLORNULL) colid = _gxtevcolid[color];
		_gx[reg] = (_gx[reg]&~0x380000)|(_SHIFTL(colid,19,3));

		tmp = 1;
		if(texmap==GX_TEXMAP_NULL || texmap&0x100) tmp = 0;
		_gx[reg] = (_gx[reg]&~0x40000)|(_SHIFTL(tmp,18,1));
	} else {
		_gx[reg] = (_gx[reg]&~0x7)|(texm&0x7);
		_gx[reg] = (_gx[reg]&~0x38)|(_SHIFTL(texc,3,3));
		
		colid = GX_BUMP;
		if(color!=GX_COLORNULL) colid = _gxtevcolid[color];
		_gx[reg] = (_gx[reg]&~0x380)|(_SHIFTL(colid,7,3));

		tmp = 1;
		if(texmap==GX_TEXMAP_NULL || texmap&0x100) tmp = 0;
		_gx[reg] = (_gx[reg]&~0x40)|(_SHIFTL(tmp,6,1));
	}
	GX_LOAD_BP_REG(_gx[reg]);
	_gx[0x09] |= 0x0001;
}

void GX_SetNumTevStages(u8 num)
{
	_gx[0xac] = (_gx[0xac]&~0x3C00)|(_SHIFTL((num-1),10,4));
	_gx[0x09] |= 0x0004;
}

void GX_SetAlphaCompare(u8 comp0,u8 ref0,u8 aop,u8 comp1,u8 ref1)
{
	u32 val = 0;
	val = (_SHIFTL(aop,22,2))|(_SHIFTL(comp1,19,3))|(_SHIFTL(comp0,16,3))|(_SHIFTL(ref1,8,8))|(ref0&0xff);
	GX_LOAD_BP_REG(0xf3000000|val);
}

void GX_SetTevKColorSel(u8 tevstage,u8 sel)
{
	u32 reg = 0xd0+(_SHIFTR(tevstage,1,3));

	if(tevstage&1)
		_gx[reg] = (_gx[reg]&~0x7C000)|(_SHIFTL(sel,14,5));
	else
		_gx[reg] = (_gx[reg]&~0x1F0)|(_SHIFTL(sel,4,5));
	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetTevKAlphaSel(u8 tevstage,u8 sel)
{
	u32 reg = 0xd0+(_SHIFTR(tevstage,1,3));

	if(tevstage&1)
		_gx[reg] = (_gx[reg]&~0xF80000)|(_SHIFTL(sel,19,5));
	else
		_gx[reg] = (_gx[reg]&~0x3E00)|(_SHIFTL(sel,9,5));
	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel)
{
	u32 reg = 0x90+(tevstage&0xf);
	_gx[reg] = (_gx[reg]&~0x3)|(ras_sel&0x3);
	_gx[reg] = (_gx[reg]&~0xC)|(_SHIFTL(tex_sel,2,2));
	GX_LOAD_BP_REG(_gx[reg]);
}

void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a)
{
	u32 regA = 0xd0+(_SHIFTL(swapid,1,3));
	u32 regB = 0xd1+(_SHIFTL(swapid,1,3));

	_gx[regA] = (_gx[regA]&~0x3)|(r&0x3);
	_gx[regA] = (_gx[regA]&~0xC)|(_SHIFTL(g,2,2));
	GX_LOAD_BP_REG(_gx[regA]);

	_gx[regB] = (_gx[regB]&~0x3)|(b&0x3);
	_gx[regB] = (_gx[regB]&~0xC)|(_SHIFTL(a,2,2));
	GX_LOAD_BP_REG(_gx[regB]);
}

void GX_SetTevIndirect(u8 tevstage,u8 indtexid,u8 format,u8 bias,u8 mtxid,u8 wrap_s,u8 wrap_t,u8 addprev,u8 utclod,u8 a)
{
	u32 val = (0x10000000|(_SHIFTL(tevstage,24,4)))|(indtexid&3)|(_SHIFTL(format,2,2))|(_SHIFTL(bias,4,3))|(_SHIFTL(a,7,2))|(_SHIFTL(mtxid,9,4))|(_SHIFTL(wrap_s,13,3))|(_SHIFTL(wrap_t,16,3))|(_SHIFTL(utclod,19,1))|(_SHIFTL(addprev,20,1));
	GX_LOAD_BP_REG(val);
}

void GX_SetTevDirect(u8 tevstage)
{
	GX_SetTevIndirect(tevstage,GX_INDTEXSTAGE0,GX_ITF_8,GX_ITB_NONE,GX_ITM_OFF,GX_ITW_OFF,GX_ITW_OFF,GX_FALSE,GX_FALSE,GX_ITBA_OFF);
}

void GX_SetNumIndStages(u8 nstages)
{
	_gx[0xac] = (_gx[0xac]&~0x70000)|(_SHIFTL(nstages,16,3));
	_gx[0x09] |= 0x0006;
}

void GX_SetIndTexCoordScale(u8 indtexid,u8 scale_s,u8 scale_t)
{
	switch(indtexid) {
		case GX_INDTEXSTAGE0:
			_gx[0xc0] = (_gx[0xc0]&~0x0f)|(scale_s&0x0f);
			_gx[0xc0] = (_gx[0xc0]&~0xF0)|(_SHIFTL(scale_t,4,4));
			GX_LOAD_BP_REG(_gx[0xc0]);
			break;
		case GX_INDTEXSTAGE1:
			_gx[0xc0] = (_gx[0xc0]&~0xF00)|(_SHIFTL(scale_s,8,4));
			_gx[0xc0] = (_gx[0xc0]&~0xF000)|(_SHIFTL(scale_t,12,4));
			GX_LOAD_BP_REG(_gx[0xc0]);
			break;
		case GX_INDTEXSTAGE2:
			_gx[0xc1] = (_gx[0xc1]&~0x0f)|(scale_s&0x0f);
			_gx[0xc1] = (_gx[0xc1]&~0xF0)|(_SHIFTL(scale_t,4,4));
			GX_LOAD_BP_REG(_gx[0xc1]);
			break;
		case GX_INDTEXSTAGE3:
			_gx[0xc1] = (_gx[0xc1]&~0xF00)|(_SHIFTL(scale_s,8,4));
			_gx[0xc1] = (_gx[0xc1]&~0xF000)|(_SHIFTL(scale_t,12,4));
			GX_LOAD_BP_REG(_gx[0xc1]);
			break;
	}
}

void GX_SetFog(u8 type,f32 startz,f32 endz,f32 nearz,f32 farz,GXColor col)
{
    f32 A, B, B_mant, C, A_f;
    u32 b_expn, b_m, a_hex, c_hex,proj = 0;


    // Calculate constants a, b, and c (TEV HW requirements).
    if((farz==nearz) || (endz==startz)) {
        // take care of the odd-ball case.
        A = 0.0f;
        B = 0.5f;
        C = 0.0f;
    } else {
        A = (farz*nearz)/((farz-nearz)*(endz-startz));
        B = farz/(farz-nearz);
        C = startz/(endz-startz);
    }

    B_mant = B;
    b_expn = 1;
    while(B_mant>1.0) {
        B_mant /= 2;
        b_expn++;
    }

    while((B_mant>0) && (B_mant<0.5)) {
        B_mant *= 2;
        b_expn--;
    }

    A_f   = A/(1<<(b_expn));
    b_m   = (u32)(B_mant * 8388638);
    a_hex = (*(u32*)((void*)&A_f));
    c_hex = (*(u32*)((void*)&C));

	GX_LOAD_BP_REG(0xee000000|(a_hex>>12));
	GX_LOAD_BP_REG(0xef000000|b_m);
	GX_LOAD_BP_REG(0xf0000000|b_expn);
	GX_LOAD_BP_REG(0xf1000000|((type<<21)|(proj<<20)|(c_hex>>12)));
	GX_LOAD_BP_REG(0xf2000000|(_SHIFTL(col.r,16,8)|_SHIFTL(col.g,8,8)|(col.b&0xff)));
}

void GX_SetFogRangeAdj(u8 enable,u16 center,GXFogAdjTbl *table)
{
	GX_LOAD_BP_REG(0xe8000156);
}

void GX_SetColorUpdate(u8 enable)
{
	_gx[0xb9] = (_gx[0xb9]&~0x8)|(_SHIFTL(enable,3,1));
	GX_LOAD_BP_REG(_gx[0xb9]);
}

void GX_SetAlphaUpdate(u8 enable)
{
	_gx[0xb9] = (_gx[0xb9]&~0x10)|(_SHIFTL(enable,4,1));
	GX_LOAD_BP_REG(_gx[0xb9]);
}

void GX_SetZCompLoc(u8 before_tex)
{
	_gx[0xbb] = (_gx[0xbb]&~0x40)|(_SHIFTL(before_tex,6,1));
	GX_LOAD_BP_REG(_gx[0xbb]);
}

void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt)
{
	u8 ms_en = 0;
	u32 realfmt[8] = {0,1,2,3,4,4,4,5};
	
	_gx[0xbb] = (_gx[0xbb]&~0x7)|(realfmt[pix_fmt]&0x7);
	_gx[0xbb] = (_gx[0xbb]&~0x38)|(_SHIFTL(z_fmt,3,3));
	GX_LOAD_BP_REG(_gx[0xbb]);
	_gx[0x09] |= 0x0004;

	if(pix_fmt==GX_PF_RGB565_Z16) ms_en = 1;
	_gx[0xac] = (_gx[0xac]&~0x200)|(_SHIFTL(ms_en,9,1));

	if(realfmt[pix_fmt]==GX_PF_Y8) {
		pix_fmt -= GX_PF_Y8;
		_gx[0xba] = (_gx[0xba]&~0xC00)|(_SHIFTL(pix_fmt,10,2));
		GX_LOAD_BP_REG(_gx[0xba]);
	}
}

void GX_SetDither(u8 dither)
{
	_gx[0xb9] = (_gx[0xb9]&~0x4)|(_SHIFTL(dither,2,1));
	GX_LOAD_BP_REG(_gx[0xb9]);
}

void GX_SetDstAlpha(u8 enable,u8 a)
{
	_gx[0xba] = (_gx[0xba]&~0xff)|(a&0xff);
	_gx[0xba] = (_gx[0xba]&~0x100)|(_SHIFTL(enable,8,1));
	GX_LOAD_BP_REG(_gx[0xba]);
}

void GX_SetFieldMask(u8 even_mask,u8 odd_mask)
{
	u32 val = 0;
	
	val = (_SHIFTL(even_mask,1,1))|(odd_mask&1);
	GX_LOAD_BP_REG(0x44000000|val);	
}

void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
{
	_gx[0xaa] = (_gx[0xaa]&~0x400000)|(_SHIFTL(half_aspect_ratio,22,1));
	GX_LOAD_BP_REG(_gx[0xaa]);
	
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x68000000|(field_mode&1));
	__GX_FlushTextureState();
}

void GX_PokeAlphaMode(u8 func,u8 threshold)
{
	_peReg[3] = (_SHIFTL(func,8,8))|(threshold&0xFF);
}

void GX_PokeAlphaRead(u8 mode)
{
	_peReg[4] = (mode&~0x4)|0x4;
}

void GX_PokeDstAlpha(u8 enable,u8 a)
{
	_peReg[2] = (_SHIFTL(enable,8,1))|(a&0xff);
}

void GX_PokeAlphaUpdate(u8 update_enable)
{
	_peReg[1] = (_peReg[1]&~0x10)|(_SHIFTL(update_enable,4,1));
}

void GX_PokeColorUpdate(u8 update_enable)
{
	_peReg[1] = (_peReg[1]&~0x8)|(_SHIFTL(update_enable,3,1));
}

void GX_PokeDither(u8 dither)
{
	_peReg[1] = (_peReg[1]&~0x4)|(_SHIFTL(dither,2,1));
}

void GX_PokeBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op)
{
	u32 regval = _peReg[1];

	regval = (regval&~0x1);
	if(type==GX_BM_BLEND || type==GX_BM_SUBSTRACT) regval |= 0x1;
	
	regval = (regval&~0x800);
	if(type==GX_BM_SUBSTRACT) regval |= 0x800;

	regval = (regval&~0x2);
	if(type==GX_BM_LOGIC) regval |= 0x2;
	
	regval = (regval&~0xF000)|(_SHIFTL(op,12,4));
	regval = (regval&~0xE0)|(_SHIFTL(dst_fact,5,3));
	regval = (regval&~0x700)|(_SHIFTL(src_fact,8,3));

	regval |= 0x41000000;
	_peReg[1] = (u16)regval;
}

void GX_PokeARGB(u16 x,u16 y,GXColor color)
{
	u32 regval;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	*(u32*)regval = _SHIFTL(color.a,24,8)|_SHIFTL(color.r,16,8)|_SHIFTL(color.g,8,8)|(color.b&0xff);
}

void GX_PokeZ(u16 x,u16 y,u32 z)
{
	u32 regval;
	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	regval = (regval&~0xC00000)|0x400000;
	*(u32*)regval = z;
}

void GX_PokeZMode(u8 comp_enable,u8 func,u8 update_enable)
{
	u16 regval;
	regval = comp_enable&0x1;
	regval = (regval&~0xE)|(_SHIFTL(func,1,3));
	regval = (regval&0x10)|(_SHIFTL(update_enable,4,1));
	_peReg[0] = regval;
}

void GX_SetIndTexOrder(u8 indtexstage,u8 texcoord,u8 texmap)
{
	switch(indtexstage) {
		case GX_INDTEXSTAGE0:
			_gx[0xc2] = (_gx[0xc2]&~0x7)|(texmap&0x7);
			_gx[0xc2] = (_gx[0xc2]&~0x38)|(_SHIFTL(texcoord,3,3));
			break;
		case GX_INDTEXSTAGE1:
			_gx[0xc2] = (_gx[0xc2]&~0x1C0)|(_SHIFTL(texmap,6,3));
			_gx[0xc2] = (_gx[0xc2]&~0xE00)|(_SHIFTL(texcoord,9,3));
			break;
		case GX_INDTEXSTAGE2:
			_gx[0xc2] = (_gx[0xc2]&~0x7000)|(_SHIFTL(texmap,12,3));
			_gx[0xc2] = (_gx[0xc2]&~0x38000)|(_SHIFTL(texcoord,15,3));
			break;
		case GX_INDTEXSTAGE3:
			_gx[0xc2] = (_gx[0xc2]&~0x1C0000)|(_SHIFTL(texmap,18,3));
			_gx[0xc2] = (_gx[0xc2]&~0xE00000)|(_SHIFTL(texcoord,21,3));
			break;
	}
	GX_LOAD_BP_REG(_gx[0xc2]);
	_gx[0x09] |= 0x0003;
}

void GX_InitLightPos(GXLightObj *lit_obj,f32 x,f32 y,f32 z)
{
	((f32*)lit_obj->val)[10] = x;
	((f32*)lit_obj->val)[11] = y;
	((f32*)lit_obj->val)[12] = z;
}

void GX_InitLightColor(GXLightObj *lit_obj,GXColor col)
{
	((u32*)lit_obj->val)[3] = ((_SHIFTL(col.r,24,8))|(_SHIFTL(col.g,16,8))|(_SHIFTL(col.b,8,8))|(col.a&0xff));
}

void GX_LoadLightObj(GXLightObj *lit_obj,u8 lit_id)
{
	u32 id;
	u16 reg;

	switch(lit_id) {
		case GX_LIGHT0:
			id = 0;
			break;
		case GX_LIGHT1:
			id = 1;
			break;
		case GX_LIGHT2:
			id = 2;
			break;
		case GX_LIGHT3:
			id = 3;
			break;
		case GX_LIGHT4:
			id = 4;
			break;
		case GX_LIGHT5:
			id = 5;
			break;
		case GX_LIGHT6:
			id = 6;
			break;
		case GX_LIGHT7:
			id = 7;
			break;
		default:
			id = 0;
			break;
	}
	
	reg = 0x600|(_SHIFTL(id,4,8));
	GX_LOAD_XF_REGS(reg,16);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(0);
	FIFO_PUTU32(((u32*)lit_obj->val)[3]);
	FIFO_PUTF32(((f32*)lit_obj->val)[4]);
	FIFO_PUTF32(((f32*)lit_obj->val)[5]);
	FIFO_PUTF32(((f32*)lit_obj->val)[6]);
	FIFO_PUTF32(((f32*)lit_obj->val)[7]);
	FIFO_PUTF32(((f32*)lit_obj->val)[8]);
	FIFO_PUTF32(((f32*)lit_obj->val)[9]);
	FIFO_PUTF32(((f32*)lit_obj->val)[10]);
	FIFO_PUTF32(((f32*)lit_obj->val)[11]);
	FIFO_PUTF32(((f32*)lit_obj->val)[12]);
	FIFO_PUTF32(((f32*)lit_obj->val)[13]);
	FIFO_PUTF32(((f32*)lit_obj->val)[14]);
	FIFO_PUTF32(((f32*)lit_obj->val)[15]);
}

void GX_LoadLightObjIdx(u32 litobjidx,u8 litid)
{
	u32 reg;
	u32 idx = 0;

	switch(litid) {
		case GX_LIGHT0:
			idx = 0;
			break;
		case GX_LIGHT1:
			idx = 1;
			break;
		case GX_LIGHT2:
			idx = 2;
			break;
		case GX_LIGHT3:
			idx = 3;
			break;
		case GX_LIGHT4:
			idx = 4;
			break;
		case GX_LIGHT5:
			idx = 5;
			break;
		case GX_LIGHT6:
			idx = 6;
			break;
		case GX_LIGHT7:
			idx = 7;
			break;
		default:
			idx = 0;
			break;

	}

	reg = 0x600|(_SHIFTL(idx,4,8));
	reg = (reg&~0xf000)|0xf000;
	reg = (reg&~0xffff0000)|(_SHIFTL(litobjidx,16,16));

	FIFO_PUTU8(0x38);
	FIFO_PUTU32(reg);
}

void GX_InitLightDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz)
{
	((f32*)lit_obj->val)[13] = -(nx);
	((f32*)lit_obj->val)[14] = -(ny);
	((f32*)lit_obj->val)[15] = -(nz);
}

void GX_InitLightDistAttn(GXLightObj *lit_obj,f32 ref_dist,f32 ref_brite,u8 dist_fn)
{
	f32 k0,k1,k2;

	if(ref_dist<0.0f ||
		ref_brite<0.0f || ref_brite>=1.0f) dist_fn = GX_DA_OFF;
	
	switch(dist_fn) {
		case GX_DA_GENTLE:
			k0 = 1.0f;
			k1 = (1.0f-ref_brite)/(ref_brite*ref_dist);
			k2 = 0.0f;
			break;
		case GX_DA_MEDIUM:
			k0 = 1.0f;
			k1 = 0.5f*(1.0f-ref_brite)/(ref_brite*ref_dist);
			k2 = 0.5f*(1.0f-ref_brite)/(ref_brite*ref_dist*ref_dist);
			break;
		case GX_DA_STEEP:
			k0 = 1.0f;
			k1 = 0.0f;
			k2 = (1.0f-ref_brite)/(ref_brite*ref_dist*ref_dist);
			break;
		case GX_DA_OFF:
		default:
			k0 = 1.0f;
			k1 = 0.0f;
			k2 = 0.0f;
			break;
	}
	((f32*)lit_obj->val)[7] = k0;	
	((f32*)lit_obj->val)[8] = k1;	
	((f32*)lit_obj->val)[9] = k2;	
}

void GX_InitLightAttn(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2,f32 k0,f32 k1,f32 k2)
{
	((f32*)lit_obj->val)[4] = a0;	
	((f32*)lit_obj->val)[5] = a1;	
	((f32*)lit_obj->val)[6] = a2;	
	((f32*)lit_obj->val)[7] = k0;	
	((f32*)lit_obj->val)[8] = k1;	
	((f32*)lit_obj->val)[9] = k2;	
}

void GX_InitLightAttnA(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2)
{
	((f32*)lit_obj->val)[4] = a0;	
	((f32*)lit_obj->val)[5] = a1;	
	((f32*)lit_obj->val)[6] = a2;	
}

void GX_InitLightAttnK(GXLightObj *lit_obj,f32 k0,f32 k1,f32 k2)
{
	((f32*)lit_obj->val)[7] = k0;	
	((f32*)lit_obj->val)[8] = k1;	
	((f32*)lit_obj->val)[9] = k2;	
}

void GX_InitSpecularDirHA(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz,f32 hx,f32 hy,f32 hz)
{
    f32 px, py, pz;

    px = -nx * BIG_NUMBER;
    py = -ny * BIG_NUMBER;
    pz = -nz * BIG_NUMBER;

	((f32*)lit_obj)[10] = px;
	((f32*)lit_obj)[11] = py;
	((f32*)lit_obj)[12] = pz;
	((f32*)lit_obj)[13] = hx;
	((f32*)lit_obj)[14] = hy;
	((f32*)lit_obj)[15] = hz;
}

void GX_InitSpecularDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz)
{
    f32 px, py, pz;
    f32 hx, hy, hz, mag;

    // Compute half-angle vector
    hx  = -nx;
    hy  = -ny;
    hz  = (-nz + 1.0f);
    mag = 1.0f / sqrtf((hx * hx) + (hy * hy) + (hz * hz));
    hx *= mag;
    hy *= mag;
    hz *= mag;

    px  = -nx * BIG_NUMBER;
    py  = -ny * BIG_NUMBER;
    pz  = -nz * BIG_NUMBER;
	
	((f32*)lit_obj)[10] = px;
	((f32*)lit_obj)[11] = py;
	((f32*)lit_obj)[12] = pz;
	((f32*)lit_obj)[13] = hx;
	((f32*)lit_obj)[14] = hy;
	((f32*)lit_obj)[15] = hz;
}

void GX_InitLightSpot(GXLightObj *lit_obj,f32 cut_off,u8 spotfn)
{
	f32 r,d,cr,a0,a1,a2;

	if(cut_off<0.0f ||	cut_off>90.0f) spotfn = GX_SP_OFF;
	
	r = (cut_off*M_PI)/180.0f;
	cr = cosf(r);

	switch(spotfn) {
		case GX_SP_FLAT:
			a0 = -1000.0f*cr;
			a1 = 1000.0f;
			a2 = 0.0f;
			break;
		case GX_SP_COS:
			a0 = -cr/(1.0f-cr);
			a1 = 1.0f/(1.0f-cr);
			a2 = 0.0f;
			break;
		case GX_SP_COS2:
			a0 = 0.0f;
			a1 = -cr/(1.0f-cr);
			a2 = 1.0f/(1.0f-cr);
			break;
		case GX_SP_SHARP:
			d = (1.0f-cr)*(1.0f-cr);
			a0 = cr*(cr-2.0f);
			a1 = 2.0f/d;
			a2 = -1.0/d;
			break;
		case GX_SP_RING1:
			d = (1.0f-cr)*(1.0f-cr);
			a0 = -4.0f*cr/d;
			a1 = 4.0f*(1.0f+cr)/d;
			a2 = -4.0f/d;
			break;
		case GX_SP_RING2:
			d = (1.0f-cr)*(1.0f-cr);
			a0 = 1.0f-2.0f*cr*cr/d;
			a1 = 4.0f*cr/d;
			a2 = -2.0f/d;
			break;
		case GX_SP_OFF:
		default:
			a0 = 1.0f;
			a1 = 0.0f;
			a2 = 0.0f;
			break;
	}
	((f32*)lit_obj)[4] = a0;
	((f32*)lit_obj)[5] = a1;
	((f32*)lit_obj)[6] = a2;
}

void GX_SetGPMetric(u32 perf0,u32 perf1)
{
	// check last setted perf0 counters
	if(_gx[0x0b]>=GX_PERF0_TRIANGLES && _gx[0x0b]<GX_PERF0_QUAD_0CVG)
		GX_LOAD_BP_REG(0x23000000);
	else if(_gx[0x0b]>=GX_PERF0_QUAD_0CVG && _gx[0x0b]<GX_PERF0_CLOCKS)
		GX_LOAD_BP_REG(0x24000000);
	else if(_gx[0x0b]>=GX_PERF0_VERTICES && _gx[0x0b]<=GX_PERF0_CLOCKS)
		GX_LOAD_XF_REG(0x1006,0);

	// check last setted perf1 counters
	if(_gx[0x0c]>=GX_PERF1_VC_ELEMQ_FULL && _gx[0x0c]<GX_PERF1_FIFO_REQ) {
		_gx[0x0d] = (_gx[0x0d]&~0xf0);
		GX_LOAD_CP_REG(0x20,_gx[0x0d]);
	} else if(_gx[0x0c]>=GX_PERF1_FIFO_REQ && _gx[0x0c]<GX_PERF1_CLOCKS) {
		_cpReg[3] = 0;
	} else if(_gx[0x0c]>=GX_PERF1_TEXELS && _gx[0x0c]<=GX_PERF1_CLOCKS) {
		GX_LOAD_BP_REG(0x67000000);
	}

	_gx[0x0b] = perf0;
	switch(_gx[0x0b]) {
		case GX_PERF0_CLOCKS:
			GX_LOAD_XF_REG(0x1006,0x00000273);
			break;
		case GX_PERF0_VERTICES:
			GX_LOAD_XF_REG(0x1006,0x0000014a);
			break;
		case GX_PERF0_CLIP_VTX:
			GX_LOAD_XF_REG(0x1006,0x0000016b);
			break;
		case GX_PERF0_CLIP_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000084);
			break;
		case GX_PERF0_XF_WAIT_IN:
			GX_LOAD_XF_REG(0x1006,0x000000c6);
			break;
		case GX_PERF0_XF_WAIT_OUT:
			GX_LOAD_XF_REG(0x1006,0x00000210);
			break;
		case GX_PERF0_XF_XFRM_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000252);
			break;
		case GX_PERF0_XF_LIT_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000231);
			break;
		case GX_PERF0_XF_BOT_CLKS:
			GX_LOAD_XF_REG(0x1006,0x000001ad);
			break;
		case GX_PERF0_XF_REGLD_CLKS:
			GX_LOAD_XF_REG(0x1006,0x000001ce);
			break;
		case GX_PERF0_XF_REGRD_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000021);
			break;
		case GX_PERF0_CLIP_RATIO:
			GX_LOAD_XF_REG(0x1006,0x00000153);
			break;
		case GX_PERF0_TRIANGLES:
			GX_LOAD_BP_REG(0x2300AE7F);
			break;
		case GX_PERF0_TRIANGLES_CULLED:
			GX_LOAD_BP_REG(0x23008E7F);
			break;
		case GX_PERF0_TRIANGLES_PASSED:
			GX_LOAD_BP_REG(0x23009E7F);
			break;
		case GX_PERF0_TRIANGLES_SCISSORED:
			GX_LOAD_BP_REG(0x23001E7F);
			break;
		case GX_PERF0_TRIANGLES_0TEX:
			GX_LOAD_BP_REG(0x2300AC3F);
			break;
		case GX_PERF0_TRIANGLES_1TEX:
			GX_LOAD_BP_REG(0x2300AC7F);
			break;
		case GX_PERF0_TRIANGLES_2TEX:
			GX_LOAD_BP_REG(0x2300ACBF);
			break;
		case GX_PERF0_TRIANGLES_3TEX:
			GX_LOAD_BP_REG(0x2300ACFF);
			break;
		case GX_PERF0_TRIANGLES_4TEX:
			GX_LOAD_BP_REG(0x2300AD3F);
			break;
		case GX_PERF0_TRIANGLES_5TEX:
			GX_LOAD_BP_REG(0x2300AD7F);
			break;
		case GX_PERF0_TRIANGLES_6TEX:
			GX_LOAD_BP_REG(0x2300ADBF);
			break;
		case GX_PERF0_TRIANGLES_7TEX:
			GX_LOAD_BP_REG(0x2300ADFF);
			break;
		case GX_PERF0_TRIANGLES_8TEX:
			GX_LOAD_BP_REG(0x2300AE3F);
			break;
		case GX_PERF0_TRIANGLES_0CLR:
			GX_LOAD_BP_REG(0x2300A27F);
			break;
		case GX_PERF0_TRIANGLES_1CLR:
			GX_LOAD_BP_REG(0x2300A67F);
			break;
		case GX_PERF0_TRIANGLES_2CLR:
			GX_LOAD_BP_REG(0x2300AA7F);
			break;
		case GX_PERF0_QUAD_0CVG:
			GX_LOAD_BP_REG(0x2402C0C6);
			break;
		case GX_PERF0_QUAD_NON0CVG:
			GX_LOAD_BP_REG(0x2402C16B);
			break;
		case GX_PERF0_QUAD_1CVG:
			GX_LOAD_BP_REG(0x2402C0E7);
			break;
		case GX_PERF0_QUAD_2CVG:
			GX_LOAD_BP_REG(0x2402C108);
			break;
		case GX_PERF0_QUAD_3CVG:
			GX_LOAD_BP_REG(0x2402C129);
			break;
		case GX_PERF0_QUAD_4CVG:
			GX_LOAD_BP_REG(0x2402C14A);
			break;
		case GX_PERF0_AVG_QUAD_CNT:
			GX_LOAD_BP_REG(0x2402C1AD);
			break;
		case GX_PERF0_NONE:
			break;
	}

	_gx[0x0c] = perf1;
	switch(_gx[0x0c]) {
		case GX_PERF1_CLOCKS:
			GX_LOAD_BP_REG(0x67000042);
			break;			
		case GX_PERF1_TEXELS:
			GX_LOAD_BP_REG(0x67000084);
			break;			
		case GX_PERF1_TX_IDLE:
			GX_LOAD_BP_REG(0x67000063);
			break;			
		case GX_PERF1_TX_REGS:
			GX_LOAD_BP_REG(0x67000129);
			break;			
		case GX_PERF1_TX_MEMSTALL:
			GX_LOAD_BP_REG(0x67000252);
			break;			
		case GX_PERF1_TC_CHECK1_2:
			GX_LOAD_BP_REG(0x67000021);
			break;			
		case GX_PERF1_TC_CHECK3_4:
			GX_LOAD_BP_REG(0x6700014b);
			break;			
		case GX_PERF1_TC_CHECK5_6:
			GX_LOAD_BP_REG(0x6700018d);
			break;			
		case GX_PERF1_TC_CHECK7_8:
			GX_LOAD_BP_REG(0x670001cf);
			break;			
		case GX_PERF1_TC_MISS:
			GX_LOAD_BP_REG(0x67000211);
			break;			
		case GX_PERF1_VC_ELEMQ_FULL:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x20;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VC_MISSQ_FULL:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x30;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VC_MEMREQ_FULL:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x40;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VC_STATUS7:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x50;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VC_MISSREP_FULL:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x60;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VC_STREAMBUF_LOW:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x70;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VC_ALL_STALLS:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x90;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_VERTICES:
			_gx[0x0d] = (_gx[0x0d]&~0xf0)|0x80;
			GX_LOAD_CP_REG(0x20,_gx[0x0d]);
			break;			
		case GX_PERF1_FIFO_REQ:
			_cpReg[3] = 2;
			break;			
		case GX_PERF1_CALL_REQ:
			_cpReg[3] = 3;
			break;			
		case GX_PERF1_VC_MISS_REQ:
			_cpReg[3] = 4;
			break;			
		case GX_PERF1_CP_ALL_REQ:
			_cpReg[3] = 5;
			break;			
		case GX_PERF1_NONE:
			break;			
	}
	
}

void GX_ClearGPMetric()
{
	_cpReg[2] = 4;
}

void GX_InitXfRasMetric()
{
	GX_LOAD_BP_REG(0x2402C022);
	GX_LOAD_XF_REG(0x1006,0x31000);
}

void GX_ReadXfRasMetric(u32 *xfwaitin,u32 *xfwaitout,u32 *rasbusy,u32 *clks)
{
	*rasbusy = _SHIFTL(_cpReg[33],16,16)|(_cpReg[32]&0xffff);
	*clks = _SHIFTL(_cpReg[35],16,16)|(_cpReg[34]&0xffff);
	*xfwaitin = _SHIFTL(_cpReg[37],16,16)|(_cpReg[36]&0xffff);
	*xfwaitout = _SHIFTL(_cpReg[39],16,16)|(_cpReg[38]&0xffff);
}

u32 GX_ReadClksPerVtx()
{
	GX_DrawDone();
	_cpReg[49] = 0x1007;
	_cpReg[48] = 0x1007;
	return (_cpReg[50]<<8);
}

void GX_ClearVCacheMetric()
{
	GX_LOAD_CP_REG(0,0);
}

void GX_ReadVCacheMetric(u32 *check,u32 *miss,u32 *stall)
{
	*check = _SHIFTL(_cpReg[41],16,16)|(_cpReg[40]&0xffff);
	*miss = _SHIFTL(_cpReg[43],16,16)|(_cpReg[42]&0xffff);
	*stall = _SHIFTL(_cpReg[45],16,16)|(_cpReg[44]&0xffff);
}

void GX_SetVCacheMetric(u32 attr)
{
}

void GX_GetGPStatus(u8 *overhi,u8 *underlow,u8 *readIdle,u8 *cmdIdle,u8 *brkpt)
{
	_gxgpstatus = _cpReg[0];
	*overhi = !!(_gxgpstatus&1);
	*underlow = !!(_gxgpstatus&2);
	*readIdle = !!(_gxgpstatus&4);
	*cmdIdle = !!(_gxgpstatus&8);
	*brkpt = !!(_gxgpstatus&16);	
}

void GX_ReadGPMetric(u32 *cnt0,u32 *cnt1)
{
	u32 tmp,reg1,reg2,reg3,reg4;
	
	reg1 = (_SHIFTL(_cpReg[33],16,16))|(_cpReg[32]&0xffff);
	reg2 = (_SHIFTL(_cpReg[35],16,16))|(_cpReg[34]&0xffff);
	reg3 = (_SHIFTL(_cpReg[37],16,16))|(_cpReg[36]&0xffff);
	reg4 = (_SHIFTL(_cpReg[39],16,16))|(_cpReg[38]&0xffff);

	*cnt0 = 0;
	if(_gx[0x0b]==GX_PERF0_CLIP_RATIO) {
		tmp = reg2*1000;
		*cnt0 = tmp/reg1;
	} else if(_gx[0x0b]>=GX_PERF0_VERTICES && _gx[0x0b]<GX_PERF0_NONE) *cnt0 = reg1;

	//further implementation needed.....
	// cnt1 fails....
}

void GX_AdjustForOverscan(GXRModeObj *rmin,GXRModeObj *rmout,u16 hor,u16 ver)
{
	if(rmin!=rmin) memcpy(rmout,rmin,sizeof(GXRModeObj));
	rmout->fbWidth -= ((hor<<1)&0xfffe);
	rmout->efbHeight -= ((((ver<<1)&0xfffe)*rmin->efbHeight)/rmin->xfbHeight);
	if(rmin->xfbMode==VI_XFBMODE_SF && !(rmin->viTVMode&VI_PROGRESSIVE)) rmout->xfbHeight -= ver;
	else rmout->xfbHeight -= ((ver<<1)&0xfffe);

	rmout->viWidth -= ((hor<<1)&0xfffe);
	rmout->viHeight -= ((ver<<1)&0xfffe);

	rmout->viXOrigin += hor;
	rmout->viYOrigin += ver;
}

f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight)
{
	u32 yScale,xfblines,cnt;
	f32 yscale;

	yscale = (f32)efbHeight/(f32)xfbHeight;
	yScale = (u32)((f32)256.0/yscale)&0x1ff;

	cnt = xfbHeight;
	xfblines = __GX_GetNumXfbLines(efbHeight,yScale);
	while(xfblines>=xfbHeight) {
		yscale = (f32)(cnt--)/(f32)efbHeight;
		yScale = (u32)((f32)256.0/yscale)&0x1ff;
		xfblines = __GX_GetNumXfbLines(efbHeight,yScale);
	}

	while(xfblines<xfbHeight) {
		yscale = (f32)(cnt++)/(f32)efbHeight;
		yScale = (u32)((f32)256.0/yscale)&0x1ff;
		xfblines = __GX_GetNumXfbLines(efbHeight,yScale);
	}
	return yscale;
}

#undef BIG_NUMBER
