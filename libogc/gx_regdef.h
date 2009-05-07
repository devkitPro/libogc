#ifndef __GX_REGDEF_H__
#define __GX_REGDEF_H__

#include <gctypes.h>

#define STRUCT_REGDEF_SIZE		1440

struct __gx_regdef
{
	vu16 cpSRreg;
	vu16 cpCRreg;
	vu16 cpCLreg;
	vu16 xfFlush;
	vu16 xfFlushExp;
	vu16 xfFlushSafe;
	vu32 gxFifoInited;
	vu32 vcdClear;
	vu32 VATTable;
	vu32 mtxIdxLo;
	vu32 mtxIdxHi;
	vu32 texCoordManually;
	vu32 vcdLo;
	vu32 vcdHi;
	vu32 vcdNrms;
	vu32 dirtyState;
	vu32 perf0Mode;
	vu32 perf1Mode;
	vu32 cpPerfMode;
	vu32 VAT0reg[8];
	vu32 VAT1reg[8];
	vu32 VAT2reg[8];
	vu32 texMapSize[8];
	vu32 texMapWrap[8];
	vu32 sciTLcorner;
	vu32 sciBRcorner;
	vu32 lpWidth;
	vu32 genMode;
	vu32 suSsize[8];
	vu32 suTsize[8];
	vu32 tevTexMap[16];
	vu32 tevColorEnv[16];
	vu32 tevAlphaEnv[16];
	vu32 tevSwapModeTable[8];
	vu32 tevRasOrder[11];
	vu32 tevTexCoordEnable;
	vu32 tevIndMask;
	vu32 texCoordGen[8];
	vu32 texCoordGen2[8];
	vu32 dispCopyCntrl;
	vu32 dispCopyDst;
	vu32 dispCopyTL;
	vu32 dispCopyWH;
	vu32 texCopyCntrl;
	vu32 texCopyDst;
	vu32 texCopyTL;
	vu32 texCopyWH;
	vu32 peZMode;
	vu32 peCMode0;
	vu32 peCMode1;
	vu32 peCntrl;
	vu32 chnAmbColor[2];
	vu32 chnMatColor[2];
	vu32 chnCntrl[4];
	GXTexRegion texRegion[24];
	GXTlutRegion tlutRegion[20];
	vu8 saveDLctx;
	vu8 gxFifoUnlinked;
	vu8 texCopyZTex;
	vu8 _pad;
} __attribute__((packed));

struct __gxfifo {
	vu32 buf_start;
	vu32 buf_end;
	vu32 size;
	vu32 hi_mark;
	vu32 lo_mark;
	vu32 rd_ptr;
	vu32 wt_ptr;
	vu32 rdwt_dst;
	vu8 fifo_wrap;
	vu8 cpufifo_ready;
	vu8 gpfifo_ready;
	u8 _pad[93];
} __attribute__((packed));

struct __gx_litobj
{
	u32 _pad[3];
	u32 col;
	f32 a0;
	f32 a1;
	f32 a2;
	f32 k0;
	f32 k1;
	f32 k2;
	f32 px;
	f32 py;
	f32 pz;
	f32 nx;
	f32 ny;
	f32 nz;
} __attribute__((packed));

struct __gx_texobj
{
	u32 tex_filt;
	u32 tex_lod;
	u32 tex_size;
	u32 tex_maddr;
	u32 _pad;
	u32 tex_fmt;
	u32 tex_tlut;
	u16 tex_tile_cnt;
	u8 tex_tile_type;
	u8 tex_flag;
} __attribute__((packed));

struct __gx_tlutobj
{
	u32 tlut_fmt;
	u32 tlut_maddr;
	u16 tlut_nentries;
	u8 _pad[2];
} __attribute__((packed));

struct __gx_texregion
{
	u32 tmem_even;
	u32 tmem_odd;
	u16 size_even;
	u16 size_odd;
	u8 ismipmap;
	u8 iscached;
	u8 _pad[2];
} __attribute__((packed));

struct __gx_tlutregion
{
	u32 tmem_addr_conf;
	u32 tmem_addr_base;
	u32 tlut_maddr;
	u16 tlut_nentries;
	u8 _pad[2];
} __attribute__((packed));

#endif
