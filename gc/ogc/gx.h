#ifndef __GX_H__
#define __GX_H__

#include <gctypes.h>
#include "gu.h"

#define GX_FALSE			0
#define GX_TRUE				1
#define GX_DISABLE			0
#define GX_ENABLE			1
#define GX_CLIP_DISABLE		0
#define GX_CLIP_ENABLE		1

#define GX_FIFO_MINSIZE		64*1024
#define GX_FIFO_HIWATERMARK	16*1024
#define GX_FIFO_OBJSIZE		128

#define GX_PERSPECTIVE		0
#define GX_ORTHOGRAPHIC		1

/* chan id */
#define GX_COLOR0			0
#define GX_COLOR1			1
#define GX_ALPHA0			2
#define GX_ALPHA1			3
#define GX_COLOR0A0			4
#define GX_COLOR1A1			5
#define GX_COLORZERO		6
#define GX_BUMP				7
#define GX_BUMPN			8
#define GX_COLORNULL		0xff

/* Matrix Type */
#define GX_MTX2x4			0
#define GX_MTX3x4			1

/* Vertex Format */
#define GX_VTXFMT0			0
#define GX_VTXFMT1			1
#define GX_VTXFMT2			2
#define GX_VTXFMT3			3
#define GX_VTXFMT4			4
#define GX_VTXFMT5			5
#define GX_VTXFMT6			6
#define GX_VTXFMT7			7
#define GX_MAXVTXFMT		8

/* Attribute Type */
#define GX_NONE				0
#define GX_DIRECT			1
#define GX_INDEX8			2
#define GX_INDEX16			3

/* CompType */
#define GX_U8				0
#define GX_S8				1
#define GX_U16				2
#define GX_S16				3
#define GX_F32				4
#define GX_RGB565			0
#define GX_RGB8				1
#define GX_RGBX8			2
#define GX_RGBA4			3
#define GX_RGBA6			4
#define GX_RGBA8			5

/* CompCount */
#define GX_POS_XY			0
#define GX_POS_XYZ			1
#define GX_NRM_XYZ			0
#define GX_NRM_NBT			1
#define GX_NRM_NBT3			2
#define GX_CLR_RGB			0
#define GX_CLR_RGBA			1
#define GX_TEX_S			0
#define GX_TEX_ST			1

/* Attribute */
#define GX_VA_PTNMTXIDX			0
#define GX_VA_TEX0MTXIDX		1
#define GX_VA_TEX1MTXIDX		2
#define GX_VA_TEX2MTXIDX		3
#define GX_VA_TEX3MTXIDX		4
#define GX_VA_TEX4MTXIDX		5
#define GX_VA_TEX5MTXIDX		6
#define GX_VA_TEX6MTXIDX		7
#define GX_VA_TEX7MTXIDX		8
#define GX_VA_POS				9
#define GX_VA_NRM				10
#define GX_VA_CLR0				11
#define GX_VA_CLR1				12
#define GX_VA_TEX0				13
#define GX_VA_TEX1				14
#define GX_VA_TEX2				15
#define GX_VA_TEX3				16
#define GX_VA_TEX4				17
#define GX_VA_TEX5				18
#define GX_VA_TEX6				19
#define GX_VA_TEX7				20
#define GX_POSMTXARRAY			21
#define GX_NRMMTXARRAY			22
#define GX_TEXMTXARRAY			23
#define GX_LITMTXARRAY			24
#define GX_VA_NBT				25
#define GX_VA_MAXATTR			26
#define GX_VA_NULL				0xff

#define GX_POINTS				0xB8
#define GX_LINES				0xA8
#define GX_LINESTRIP			0xB0
#define GX_TRIANGLES			0x90
#define GX_TRIANGLSTRIP			0x98
#define GX_TRIANGLEFAN			0xA0
#define GX_QUADS				0x80

#define GX_SRC_REG			0
#define GX_SRC_VTX			1

#define GX_LIGHT0			0x001
#define GX_LIGHT1			0x002
#define GX_LIGHT2			0x004
#define GX_LIGHT3			0x008
#define GX_LIGHT4			0x010
#define GX_LIGHT5			0x020
#define GX_LIGHT6			0x040
#define GX_LIGHT7			0x080
#define GX_MAXLIGHT			0x100
#define GX_LIGHTNULL		0x000

#define GX_DF_NONE			0
#define GX_DF_SIGNED		1
#define GX_DF_CLAMP			2

#define GX_AF_SPEC			0
#define GX_AF_SPOT			1
#define GX_AF_NONE			2

/* pos,nrm,tex,dtt matrix */
#define GX_PNMTX0			0
#define GX_PNMTX1			3
#define GX_PNMTX2			6
#define GX_PNMTX3			9
#define GX_PNMTX4			12
#define GX_PNMTX5			15
#define GX_PNMTX6			18
#define GX_PNMTX7			21
#define GX_PNMTX8			24
#define GX_PNMTX9			27
#define GX_TEXMTX0			30
#define GX_TEXMTX1			33
#define GX_TEXMTX2			36
#define GX_TEXMTX3			39
#define GX_TEXMTX4			42
#define GX_TEXMTX5			45
#define GX_TEXMTX6			48
#define GX_TEXMTX7			51
#define GX_TEXMTX8			54
#define GX_TEXMTX9			57
#define GX_IDENTITY			60	
#define GX_DTTMTX0			64
#define GX_DTTMTX1			67
#define GX_DTTMTX2			70
#define GX_DTTMTX3			73
#define GX_DTTMTX4			76
#define GX_DTTMTX5			79
#define GX_DTTMTX6			82
#define GX_DTTMTX7			85
#define GX_DTTMTX8			88
#define GX_DTTMTX9			91
#define GX_DTTMTX10			94
#define GX_DTTMTX11			97
#define GX_DTTMTX12			100
#define GX_DTTMTX13			103
#define GX_DTTMTX14			106
#define GX_DTTMTX15			109
#define GX_DTTMTX16			112
#define GX_DTTMTX17			115
#define GX_DTTMTX18			118
#define GX_DTTMTX19			121
#define GX_DTTIDENTITY		125

/* tex coord id */
#define GX_TEXCOORD0		0x0
#define GX_TEXCOORD1		0x1
#define GX_TEXCOORD2		0x2
#define GX_TEXCOORD3		0x3
#define GX_TEXCOORD4		0x4
#define GX_TEXCOORD5		0x5
#define GX_TEXCOORD6		0x6
#define GX_TEXCOORD7		0x7
#define GX_MAXCOORD			0x8
#define GX_TEXCOORDNULL		0xff

/* tex format */
#define _GX_TF_ZTF			0x10
#define _GX_TF_CTF			0x20

#define GX_TF_I4			0x0
#define GX_TF_I8			0x1
#define GX_TF_IA4			0x2
#define GX_TF_IA8			0x3
#define GX_TF_RGB565		0x4
#define GX_TF_RGB5A3		0x5
#define GX_TF_RGBA8			0x6
#define GX_TF_CMPR			0xE

#define GX_CTF_R4			(0x0|_GX_TF_CTF)
#define GX_CTF_RA4			(0x2|_GX_TF_CTF)
#define GX_CTF_RA8			(0x3|_GX_TF_CTF)
#define GX_CTF_YUVA8		(0x6|_GX_TF_CTF)
#define GX_CTF_A8			(0x7|_GX_TF_CTF)
#define GX_CTF_R8			(0x8|_GX_TF_CTF)
#define GX_CTF_G8			(0x9|_GX_TF_CTF)
#define GX_CTF_B8			(0xA|_GX_TF_CTF)
#define GX_CTF_RG8			(0xB|_GX_TF_CTF)
#define GX_CTF_GB8			(0xC|_GX_TF_CTF)

#define GX_TF_Z8			(0x1|_GX_TF_ZTF)
#define GX_TF_Z16			(0x3|_GX_TF_ZTF)
#define GX_TF_Z24X8			(0x6|_GX_TF_ZTF)

#define GX_CTF_Z4			(0x0|_GX_TF_ZTF|_GX_TF_CTF)
#define GX_CTF_Z8M			(0x9|_GX_TF_ZTF|_GX_TF_CTF)
#define GX_CTF_Z8L			(0xA|_GX_TF_ZTF|_GX_TF_CTF)
#define GX_CTF_Z16L			(0xC|_GX_TF_ZTF|_GX_TF_CTF)

#define GX_TF_A8			GX_CTF_A8

/* gx tlut size */
#define GX_TLUT_16			1	// number of 16 entry blocks.
#define GX_TLUT_32			2
#define GX_TLUT_64			4
#define GX_TLUT_128			8
#define GX_TLUT_256			16
#define GX_TLUT_512			32
#define GX_TLUT_1K			64
#define GX_TLUT_2K			128
#define GX_TLUT_4K			256
#define GX_TLUT_8K			512
#define GX_TLUT_16K			1024

/* Z tex op */
#define GX_ZT_DISABLE		0
#define GX_ZT_ADD			1
#define GX_ZT_REPLACE		2
#define GX_MAX_ZTEXOP		3

/* TexGenType */
#define GX_TG_MTX3x4		0
#define GX_TG_MTX2x4		1
#define GX_TG_BUMP0			2
#define GX_TG_BUMP1			3
#define GX_TG_BUMP2			4
#define GX_TG_BUMP3			5
#define GX_TG_BUMP4			6
#define GX_TG_BUMP5			7
#define GX_TG_BUMP6			8
#define GX_TG_BUMP7			9
#define GX_TG_SRTG			10

/* TexGenSrc */
#define GX_TG_POS			0
#define GX_TG_NRM			1	
#define GX_TG_BINRM			2
#define GX_TG_TANGENT		3
#define GX_TG_TEX0			4
#define GX_TG_TEX1			5
#define GX_TG_TEX2			6
#define GX_TG_TEX3			7
#define GX_TG_TEX4			8
#define GX_TG_TEX5			9
#define GX_TG_TEX6			10
#define GX_TG_TEX7			11
#define GX_TG_TEXCOORD0		12
#define GX_TG_TEXCOORD1		13
#define GX_TG_TEXCOORD2		14
#define GX_TG_TEXCOORD3		15
#define GX_TG_TEXCOORD4		16
#define GX_TG_TEXCOORD5		17
#define GX_TG_TEXCOORD6		18
#define GX_TG_COLOR0		19
#define GX_TG_COLOR1		20

/* Compare */
#define GX_NEVER			0
#define GX_LESS				1
#define GX_EQUAL			2
#define GX_LEQUAL			3
#define GX_GREATER			4
#define GX_NEQUAL			5
#define GX_GEQUAL			6
#define GX_ALWAYS			7

/* Text Wrap Mode */
#define GX_CLAMP			0
#define GX_REPEAT			1
#define GX_MIRROR			2
#define GX_MAXTEXWRAPMODE	3

/* Blend mode */
#define GX_BM_NONE			0
#define GX_BM_BLEND			1
#define GX_BM_LOGIC			2
#define GX_BM_SUBSTRACT		3
#define GX_MAX_BLENDMODE	4

/* blend factor */
#define GX_BL_ZERO			0
#define GX_BL_ONE			1
#define GX_BL_SRCCLR		2
#define GX_BL_INVSRCCLR		3
#define GX_BL_SRCALPHA		4
#define GX_BL_INVSRCALPHA	5
#define GX_BL_DSTALPHA		6
#define GX_BL_INVDSTALPHA	7
#define GX_BL_DSTCLR		GX_BL_SRCCLR
#define GX_BL_INVDSTCLR		GX_BL_INVSRCCLR

/* logic op */
#define GX_LO_CLEAR			0
#define GX_LO_AND			1
#define GX_LO_REVAND		2
#define GX_LO_COPY			3
#define GX_LO_INVAND		4
#define GX_LO_NOOP			5
#define GX_LO_XOR			6
#define GX_LO_OR			7
#define GX_LO_NOR			8
#define GX_LO_EQUIV			9
#define GX_LO_INV			10
#define GX_LO_REVOR			11
#define GX_LO_INVCOPY		12
#define GX_LO_INVOR			13
#define GX_LO_NAND			14
#define GX_LO_SET			15

/* piont/line fmt */
#define GX_TO_ZERO			0
#define GX_TO_SIXTEENTH		1
#define GX_TO_EIGHTH		2
#define GX_TO_FOURTH		3
#define GX_TO_HALF			4
#define GX_TO_ONE			5
#define GX_MAX_TEXOFFSET	6

/* tev mode */
#define GX_MODULATE			0
#define GX_DECAL			1
#define GX_BLEND			2
#define GX_REPLACE			3
#define GX_PASSCLR			4

/* tev color arg */
#define GX_CC_CPREV			0
#define GX_CC_APREV			1
#define GX_CC_C0			2
#define GX_CC_A0			3
#define GX_CC_C1			4
#define GX_CC_A1			5
#define GX_CC_C2			6
#define GX_CC_A2			7
#define GX_CC_TEXC			8
#define GX_CC_TEXA			9
#define GX_CC_RASC			10
#define GX_CC_RASA			11
#define GX_CC_ONE			12
#define GX_CC_HALF			13
#define GX_CC_KONST			14
#define GX_CC_ZERO			15

/* tev alpha arg */
#define GX_CA_APREV			0
#define GX_CA_A0			1
#define GX_CA_A1			2
#define GX_CA_A2			3
#define GX_CA_TEXA			4
#define GX_CA_RASA			5
#define GX_CA_KONST			6
#define GX_CA_ZERO			7

/* tev stage */
#define GX_TEVSTAGE0		0
#define GX_TEVSTAGE1		1
#define GX_TEVSTAGE2		2
#define GX_TEVSTAGE3		3
#define GX_TEVSTAGE4		4
#define GX_TEVSTAGE5		5
#define GX_TEVSTAGE6		6
#define GX_TEVSTAGE7		7
#define GX_TEVSTAGE8		8
#define GX_TEVSTAGE9		9
#define GX_TEVSTAGE10		10
#define GX_TEVSTAGE11		11
#define GX_TEVSTAGE12		12 
#define GX_TEVSTAGE13		13
#define GX_TEVSTAGE14		14
#define GX_TEVSTAGE15		15
#define GX_MAX_TEVSTAGE		16


/* tev op */
#define GX_TEV_ADD				0
#define GX_TEV_SUB				1
#define GX_TEV_COMP_R8_GT		8
#define GX_TEV_COMP_R8_EQ		9
#define GX_TEV_COMP_GR16_GT		10
#define GX_TEV_COMP_GR16_EQ		11
#define GX_TEV_COMP_BGR24_GT	12
#define GX_TEV_COMP_BGR24_EQ	13
#define GX_TEV_COMP_RGB8_GT		14
#define GX_TEV_COMP_RGB8_EQ		15
#define GX_TEV_COMP_A8_GT		GX_TEV_COMP_RGB8_GT	 // for alpha channel
#define GX_TEV_COMP_A8_EQ		GX_TEV_COMP_RGB8_EQ  // for alpha channel

/* tev bias */
#define GX_TB_ZERO				0
#define GX_TB_ADDHALF			1
#define GX_TB_SUBHALF			2
#define GX_MAX_TEVBIAS			3

/* tev scale */
#define GX_CS_SCALE_1			0
#define GX_CS_SCALE_2			1
#define GX_CS_SCALE_4			2
#define GX_CS_DIVIDE_2			3
#define GX_MAX_TEVSCALE			4

/* tev dst reg */
#define GX_TEVPREV				0
#define GX_TEVREG0				1
#define GX_TEVREG1				2
#define GX_TEVREG2				3
#define GX_MAX_TEVREG			4

/* cull mode */
#define GX_CULL_NONE			0
#define GX_CULL_FRONT			1
#define GX_CULL_BACK			2
#define GX_CULL_ALL				3

/* tex map */
#define GX_TEXMAP0				0
#define GX_TEXMAP1				1
#define GX_TEXMAP2				2
#define GX_TEXMAP3				3
#define GX_TEXMAP4				4
#define GX_TEXMAP5				5
#define GX_TEXMAP6				6
#define GX_TEXMAP7				7
#define GX_MAX_TEXMAP			8
#define GX_TEXMAP_NULL			0xff
#define GX_TEXMAP_DISABLE		0x100

/* alpha op */
#define GX_AOP_AND				0
#define GX_AOP_OR				1
#define GX_AOP_XOR				2
#define GX_AOP_XNOR				3
#define GX_MAX_ALPHAOP			4

/* tev const color sel */
#define GX_TEV_KCSEL_1					0x00
#define GX_TEV_KCSEL_7_8				0x01
#define GX_TEV_KCSEL_3_4				0x02
#define GX_TEV_KCSEL_5_8				0x03
#define GX_TEV_KCSEL_1_2				0x04
#define GX_TEV_KCSEL_3_8				0x05
#define GX_TEV_KCSEL_1_4				0x06
#define GX_TEV_KCSEL_1_8				0x07
#define GX_TEV_KCSEL_K0					0x0C
#define GX_TEV_KCSEL_K1					0x0D
#define GX_TEV_KCSEL_K2					0x0E
#define GX_TEV_KCSEL_K3					0x0F
#define GX_TEV_KCSEL_K0_R				0x10
#define GX_TEV_KCSEL_K1_R				0x11
#define GX_TEV_KCSEL_K2_R				0x12
#define GX_TEV_KCSEL_K3_R				0x13
#define GX_TEV_KCSEL_K0_G				0x14
#define GX_TEV_KCSEL_K1_G				0x15
#define GX_TEV_KCSEL_K2_G				0x16
#define GX_TEV_KCSEL_K3_G				0x17
#define GX_TEV_KCSEL_K0_B				0x18
#define GX_TEV_KCSEL_K1_B				0x19
#define GX_TEV_KCSEL_K2_B				0x1A
#define GX_TEV_KCSEL_K3_B				0x1B
#define GX_TEV_KCSEL_K0_A				0x1C
#define GX_TEV_KCSEL_K1_A				0x1D
#define GX_TEV_KCSEL_K2_A				0x1E
#define GX_TEV_KCSEL_K3_A				0x1F

/* tev const alpha sel */
#define GX_TEV_KASEL_1					0x00
#define GX_TEV_KASEL_7_8				0x01
#define GX_TEV_KASEL_3_4				0x02
#define GX_TEV_KASEL_5_8				0x03
#define GX_TEV_KASEL_1_2				0x04
#define GX_TEV_KASEL_3_8				0x05
#define GX_TEV_KASEL_1_4				0x06
#define GX_TEV_KASEL_1_8				0x07
#define GX_TEV_KASEL_K0_R				0x10
#define GX_TEV_KASEL_K1_R				0x11
#define GX_TEV_KASEL_K2_R				0x12
#define GX_TEV_KASEL_K3_R				0x13
#define GX_TEV_KASEL_K0_G				0x14
#define GX_TEV_KASEL_K1_G				0x15
#define GX_TEV_KASEL_K2_G				0x16
#define GX_TEV_KASEL_K3_G				0x17
#define GX_TEV_KASEL_K0_B				0x18
#define GX_TEV_KASEL_K1_B				0x19
#define GX_TEV_KASEL_K2_B				0x1A
#define GX_TEV_KASEL_K3_B				0x1B
#define GX_TEV_KASEL_K0_A				0x1C
#define GX_TEV_KASEL_K1_A				0x1D
#define GX_TEV_KASEL_K2_A				0x1E
#define GX_TEV_KASEL_K3_A				0x1F

/* tev swap mode */
#define GX_TEV_SWAP0					0
#define GX_TEV_SWAP1					1
#define GX_TEV_SWAP2					2
#define GX_TEV_SWAP3					3
#define GX_MAX_TEVSWAP					4

/* tev color chan */
#define GX_CH_RED						0
#define GX_CH_GREEN						1
#define GX_CH_BLUE						2
#define GX_CH_ALPHA						3

/* ind tex stage id */
#define GX_INDTEXSTAGE0					0
#define GX_INDTEXSTAGE1					1
#define GX_INDTEXSTAGE2					2
#define GX_INDTEXSTAGE3					3
#define GX_MAX_INDTEXSTAGE				4

/* ind tex format */
#define GX_ITF_8						0
#define GX_ITF_5						1
#define GX_ITF_4						2 
#define GX_ITF_3						3
#define GX_MAX_ITFORMAT					4

/* ind tex bias sel */
#define GX_ITB_NONE						0	
#define GX_ITB_S						1
#define GX_ITB_T						2
#define GX_ITB_ST						3
#define GX_ITB_U						4
#define GX_ITB_SU						5
#define GX_ITB_TU						6
#define GX_ITB_STU						7
#define GX_MAX_ITBIAS					8

/* ind tex mtx idx */
#define GX_ITM_OFF						0
#define GX_ITM_0						1
#define GX_ITM_1						2
#define GX_ITM_2						3
#define GX_ITM_S0						5
#define GX_ITM_S1						6
#define GX_ITM_S2						7
#define GX_ITM_T0						9
#define GX_ITM_T1						10
#define GX_ITM_T2						11

/* ind tex warp */
#define GX_ITW_OFF						0
#define GX_ITW_256						1
#define GX_ITW_128						2
#define GX_ITW_64						3
#define GX_ITW_32						4
#define GX_ITW_16						5
#define GX_ITW_0						6
#define GX_MAX_ITWRAP					7

/* ind tex alpha sel */
#define GX_ITBA_OFF						0
#define GX_ITBA_S						1
#define GX_ITBA_T						2
#define GX_ITBA_U						3
#define GX_MAX_ITBALPHA					4

/* ind tex coord scale */
#define GX_ITS_1						0 
#define GX_ITS_2						1
#define GX_ITS_4						2
#define GX_ITS_8						3
#define GX_ITS_16						4
#define GX_ITS_32						5
#define GX_ITS_64						6
#define GX_ITS_128						7
#define GX_ITS_256						8
#define GX_MAX_ITSCALE					9

/* fog type */
#define GX_FOG_NONE						0
#define GX_FOG_LIN						2
#define GX_FOG_EXP						4
#define GX_FOG_EXP2						5
#define GX_FOG_REVEXP  					6
#define GX_FOG_REVEXP2 					7

/* pixel format */
#define GX_PF_RGB8_Z24					0
#define GX_PF_RGBA6_Z24					1
#define GX_PF_RGB565_Z16				2
#define GX_PF_Z24						3
#define GX_PF_Y8						4
#define GX_PF_U8						5
#define GX_PF_V8						6
#define GX_PF_YUV420					7

/* z pixel format */
#define GX_ZC_LINEAR					0
#define GX_ZC_NEAR						1
#define GX_ZC_MID						2
#define GX_ZC_FAR						3

/* clamp */
#define GX_CLAMP_NONE					0
#define GX_CLAMP_TOP					1
#define GX_CLAMP_BOTTOM					2

/* gamma */
#define GX_GM_1_0						0
#define GX_GM_1_7						1
#define GX_GM_2_2						2

/* copy mode */
#define GX_COPY_PROGRESSIVE				0
#define GX_COPY_INTLC_EVEN				2
#define GX_COPY_INTLC_ODD				3

/* alpha read mode */
#define GX_READ_00						0
#define GX_READ_FF						1
#define GX_READ_NONE					2

/* tex cache size */
#define GX_TEXCACHE_32K					0
#define GX_TEXCACHE_128K				1
#define GX_TEXCACHE_512K				2
#define GX_TEXCACHE_NONE				3

/* dist attn fn */
#define GX_DA_OFF						0
#define GX_DA_GENTLE					1
#define GX_DA_MEDIUM					2
#define GX_DA_STEEP						3

/* spot fn */
#define GX_SP_OFF						0
#define GX_SP_FLAT						1
#define GX_SP_COS						2
#define GX_SP_COS2						3
#define GX_SP_SHARP						4
#define GX_SP_RING1						5
#define GX_SP_RING2						6

/* tex filter */
#define GX_NEAR							0
#define GX_LINEAR						1
#define GX_NEAR_MIP_NEAR				2
#define GX_LIN_MIP_NEAR					3
#define GX_NEAR_MIP_LIN					4
#define GX_LIN_MIP_LIN					5

/* anisotropy */
#define GX_ANISO_1						0
#define GX_ANISO_2						1
#define GX_ANISO_4						2
#define GX_MAX_ANISOTROPY				3

/* vcache metric settings */
#define GX_VC_POS						0
#define GX_VC_NRM						1
#define GX_VC_CLR0						2
#define GX_VC_CLR1						3
#define GX_VC_TEX0						4
#define GX_VC_TEX1						5
#define GX_VC_TEX2						6
#define GX_VC_TEX3						7
#define GX_VC_TEX4						8
#define GX_VC_TEX5						9
#define GX_VC_TEX6						10
#define GX_VC_TEX7						11
#define GX_VC_ALL						15

/* perf0 counter sets */
#define GX_PERF0_VERTICES				0
#define GX_PERF0_CLIP_VTX				1
#define GX_PERF0_CLIP_CLKS				2
#define GX_PERF0_XF_WAIT_IN				3
#define GX_PERF0_XF_WAIT_OUT			4
#define GX_PERF0_XF_XFRM_CLKS			5
#define GX_PERF0_XF_LIT_CLKS			6
#define GX_PERF0_XF_BOT_CLKS			7
#define GX_PERF0_XF_REGLD_CLKS			8
#define GX_PERF0_XF_REGRD_CLKS			9
#define GX_PERF0_CLIP_RATIO				10
#define GX_PERF0_TRIANGLES				11
#define GX_PERF0_TRIANGLES_CULLED		12
#define GX_PERF0_TRIANGLES_PASSED		13
#define GX_PERF0_TRIANGLES_SCISSORED	14
#define GX_PERF0_TRIANGLES_0TEX			15
#define GX_PERF0_TRIANGLES_1TEX			16
#define GX_PERF0_TRIANGLES_2TEX			17
#define GX_PERF0_TRIANGLES_3TEX			18
#define GX_PERF0_TRIANGLES_4TEX			19
#define GX_PERF0_TRIANGLES_5TEX			20
#define GX_PERF0_TRIANGLES_6TEX			21
#define GX_PERF0_TRIANGLES_7TEX			22
#define GX_PERF0_TRIANGLES_8TEX			23
#define GX_PERF0_TRIANGLES_0CLR			24
#define GX_PERF0_TRIANGLES_1CLR			25
#define GX_PERF0_TRIANGLES_2CLR			26
#define GX_PERF0_QUAD_0CVG				27
#define GX_PERF0_QUAD_NON0CVG			28
#define GX_PERF0_QUAD_1CVG				29
#define GX_PERF0_QUAD_2CVG				30
#define GX_PERF0_QUAD_3CVG				31
#define GX_PERF0_QUAD_4CVG				32
#define GX_PERF0_AVG_QUAD_CNT			33
#define GX_PERF0_CLOCKS					34
#define GX_PERF0_NONE					35

/* perf1 counter sets */
#define GX_PERF1_TEXELS					0
#define GX_PERF1_TX_IDLE				1
#define GX_PERF1_TX_REGS				2
#define GX_PERF1_TX_MEMSTALL			3
#define GX_PERF1_TC_CHECK1_2			4
#define GX_PERF1_TC_CHECK3_4			5
#define GX_PERF1_TC_CHECK5_6			6
#define GX_PERF1_TC_CHECK7_8			7
#define GX_PERF1_TC_MISS				8
#define GX_PERF1_VC_ELEMQ_FULL			9
#define GX_PERF1_VC_MISSQ_FULL			10
#define GX_PERF1_VC_MEMREQ_FULL			11
#define GX_PERF1_VC_STATUS7				12
#define GX_PERF1_VC_MISSREP_FULL		13
#define GX_PERF1_VC_STREAMBUF_LOW		14
#define GX_PERF1_VC_ALL_STALLS			15
#define GX_PERF1_VERTICES				16
#define GX_PERF1_FIFO_REQ				17
#define GX_PERF1_CALL_REQ				18
#define GX_PERF1_VC_MISS_REQ			19
#define GX_PERF1_CP_ALL_REQ				20
#define GX_PERF1_CLOCKS					21
#define GX_PERF1_NONE					22

/* tlut */
#define GX_TLUT0						 0
#define GX_TLUT1						 1
#define GX_TLUT2						 2
#define GX_TLUT3						 3
#define GX_TLUT4						 4
#define GX_TLUT5						 5
#define GX_TLUT6						 6 
#define GX_TLUT7						 7
#define GX_TLUT8						 8
#define GX_TLUT9						 9
#define GX_TLUT10						10
#define GX_TLUT11						11
#define GX_TLUT12						12
#define GX_TLUT13						13
#define GX_TLUT14						14
#define GX_TLUT15						15
#define GX_BIGTLUT0						16
#define GX_BIGTLUT1						17
#define GX_BIGTLUT2						18
#define GX_BIGTLUT3						19

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _gx_color {
 	u8 r;
 	u8 g;
 	u8 b;
	u8 a;
} GXColor;

typedef struct _gx_texobj {
	u32 val[8];
} GXTexObj;

typedef struct _gx_tlutobj {
	u32 val[3];
} GXTlutObj;

typedef struct _gx_texreg {
	u32 val[4];
} GXTexRegion;

typedef struct _gx_tlutreg {
	u32 val[4];
} GXTlutRegion;

typedef struct _gx_litobj {
	u32 val[16];
} GXLightObj;

typedef struct _vtx {
	f32 x,y,z;
	u16 s,t;
	u32 rgba;
} Vtx;

typedef struct {
	u8 pad[GX_FIFO_OBJSIZE];
} GXFifoObj;

typedef struct {
	u8 dummy[4];
} GXTexReg;

typedef struct {
	u16 r[10];
} GXFogAdjTbl;

typedef void (*GXBreakPtCallback)(void);
typedef void (*GXDrawDoneCallback)(void);
typedef void (*GXDrawSyncCallback)(u16 token);

typedef GXTexRegion* (*GXTexRegionCallback)(GXTexObj *obj,u8 mapid);
typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 tlut_name);

GXFifoObj* GX_Init(void *base,u32 size);
void GX_InitFifoBase(GXFifoObj *fifo,void *base,u32 size);
void GX_InitFifoLimits(GXFifoObj *fifo,u32 hiwatermark,u32 lowatermark);
void GX_InitFifoPtrs(GXFifoObj *fifo,void *rd_ptr,void *wt_ptr);
void GX_SetCPUFifo(GXFifoObj *fifo);
void GX_SetGPFifo(GXFifoObj *fifo);
GXDrawDoneCallback GX_SetDrawDoneCallback(GXDrawDoneCallback cb);
GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb);
GXBreakPtCallback GX_SetBreakPtCallback(GXBreakPtCallback cb);

void GX_Flush();
void GX_SetDrawDone();
void GX_WaitDrawDone();
void GX_SetDrawSync(u16 token);
void GX_DisableBreakPt();
void GX_EnableBreakPt(void *break_pt);
void GX_DrawDone();
void GX_PixModeSync();
void GX_TexModeSync();
void GX_InvVtxCache();
void GX_ClearVtxDesc();
void GX_LoadProjectionMtx(Mtx44 mt,u8 type);
void GX_SetViewport(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ);
void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field);
void GX_SetCopyClear(GXColor color,u32 zvalue);
void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn);
void GX_SetChanAmbColor(s32 channel,GXColor color);
void GX_SetChanMatColor(s32 channel,GXColor color);
void GX_SetArray(u32 attr,void *ptr,u8 stride);
void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac);
void GX_SetVtxDesc(u8 attr,u8 type);

void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt);
void GX_End();

void GX_MatrixIndex1x8(u8 index);

void GX_Position3f32(f32 x,f32 y,f32 z);
void GX_Position3u16(u16 x,u16 y,u16 z);
void GX_Position3s16(s16 x,s16 y,s16 z);
void GX_Position3u8(u8 x,u8 y,u8 z);
void GX_Position3s8(s8 x,s8 y,s8 z);
void GX_Position2f32(f32 x,f32 y);
void GX_Position2u16(u16 x,u16 y);
void GX_Position2s16(s16 x,s16 y);
void GX_Position2u8(u8 x,u8 y);
void GX_Position2s8(s8 x,s8 y);
void GX_Position1x8(u8 index);
void GX_Position1x16(u16 index);

void GX_Normal3f32(f32 nx,f32 ny,f32 nz);
void GX_Normal3s16(s16 nx,s16 ny,s16 nz);
void GX_Normal3s8(s8 nx,s8 ny,s8 nz);
void GX_Normal1x8(u8 index);
void GX_Normal1x16(u16 index);

void GX_Color4u8(u8 r,u8 g,u8 b,u8 a);
void GX_Color3u8(u8 r,u8 g,u8 b);
void GX_Color1u32(u32 clr);
void GX_Color1u16(u16 clr);
void GX_Color1x8(u8 index);
void GX_Color1x16(u16 index);

void GX_TexCoord2f32(f32 s,f32 t);
void GX_TexCoord2u16(u16 s,u16 t);
void GX_TexCoord2s16(s16 s,s16 t);
void GX_TexCoord2u8(u8 s,u8 t);
void GX_TexCoord2s8(s8 s,s8 t);
void GX_TexCoord1f32(f32 s);
void GX_TexCoord1u16(u16 s);
void GX_TexCoord1s16(s16 s);
void GX_TexCoord1u8(u8 s);
void GX_TexCoord1s8(s8 s);
void GX_TexCoord1x8(u8 index);
void GX_TexCoord1x16(u16 index);

void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht);
void GX_CopyDisp(void *dest,u8 clear);
void GX_LoadPosMtxImm(Mtx mt,u32 pnidx);
void GX_LoadPosMtxIdx(u16 mtxidx,u32 pnidx);
void GX_LoadNrmMtxImm(Mtx mt,u32 pnidx);
void GX_LoadNrmMtxIdx3x3(u16 mtxidx,u32 pnidx);
void GX_LoadTexMtxImm(Mtx mt,u32 texidx,u8 type);
void GX_LoadTexMtxIdx(u16 mtxidx,u32 texidx,u8 type);
void GX_SetCurrentMtx(u32 mtx);

void GX_SetTevOp(u8 tevstage,u8 mode);
void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d);
void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d);
void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid);
void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid);
void GX_SetNumTexGens(u32 nr);
void GX_SetTexCoordGen(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc);
void GX_SetTexCoordGen2(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc,u32 normalize,u32 postmtx);
void GX_SetZTexture(u8 op,u8 fmt,u32 bias);
void GX_SetZMode(u8 enable,u8 func,u8 update_enable);
void GX_SetZCompLoc(u8 before_tex);
void GX_SetLineWidth(u8 width,u8 fmt);
void GX_SetPointSize(u8 width,u8 fmt);

void GX_SetBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op);
void GX_SetCullMode(u8 mode);
void GX_SetCoPlanar(u8 enable);
void GX_EnableTexOffsets(u8 coord,u8 line_enable,u8 point_enable);
void GX_SetClipMode(u8 mode);
void GX_SetScissor(u32 xOrigin,u32 yOrigin,u32 wd,u32 ht);
void GX_SetScissorBoxOffset(s32 xoffset,s32 yoffset);
void GX_SetNumChans(u8 num);

void GX_SetTevOrder(u8 tevstage,u8 texcoord,u32 texmap,u8 color);
void GX_SetNumTevStages(u8 num);

void GX_SetAlphaCompare(u8 comp0,u8 ref0,u8 aop,u8 comp1,u8 ref1);
void GX_SetTevKColorSel(u8 tevstage,u8 sel);
void GX_SetTevKAlphaSel(u8 tevstage,u8 sel);
void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel);
void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a);
void GX_SetTevIndirect(u8 tevstage,u8 indtexid,u8 format,u8 bias,u8 mtxid,u8 wrap_s,u8 wrap_t,u8 addprev,u8 utclod,u8 a);
void GX_SetTevDirect(u8 tevstage);
void GX_SetNumIndStages(u8 nstages);
void GX_SetIndTexOrder(u8 indtexstage,u8 texcoord,u8 texmap);
void GX_SetIndTexCoordScale(u8 indtexid,u8 scale_s,u8 scale_t);
void GX_SetFog(u8 type,f32 startz,f32 endz,f32 nearz,f32 farz,GXColor col);
void GX_SetFogRangeAdj(u8 enable,u16 center,GXFogAdjTbl *table);

void GX_SetColorUpdate(u8 enable);
void GX_SetAlphaUpdate(u8 enable);
void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt);
void GX_SetDither(u8 dither);
void GX_SetDstAlpha(u8 enable,u8 a);
void GX_SetFieldMask(u8 even_mask,u8 odd_mask);
void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio);

void GX_SetDispCopyDst(u16 wd,u16 ht);
void GX_SetDispCopyYScale(f32 yscale);
void GX_SetCopyClamp(u8 clamp);
void GX_SetDispCopyGamma(u8 gamma);
void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7]);
void GX_SetDispCopyFrame2Field(u8 mode);
void GX_SetTexCopySrc(u16 left,u16 top,u16 wd,u16 ht);
void GX_CopyTex(void *dest,u8 clear);
void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap);

void GX_ClearBoundingBox();
void GX_PokeAlphaMode(u8 func,u8 threshold);
void GX_PokeAlphaUpdate(u8 update_enable);
void GX_PokeColorUpdate(u8 update_enable);
void GX_PokeDither(u8 dither);
void GX_PokeBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op);
void GX_PokeAlphaRead(u8 mode);
void GX_PokeDstAlpha(u8 enable,u8 a);
void GX_PokeZ(u16 x,u16 y,u32 z);
void GX_PokeZMode(u8 comp_enable,u8 func,u8 update_enable);

u8 GX_GetTexFmt(GXTexObj *obj);
u32 GX_GetTexBufferSize(u16 wd,u16 ht,u32 fmt,u8 mipmap,u8 maxlod);
void GX_InvalidateTexAll();
void GX_InvalidateTexRegion(GXTexRegion *region);
void GX_InitTexCacheRegion(GXTexRegion *region,u8 is32bmipmap,u32 tmem_even,u8 size_even,u32 tmem_odd,u8 size_odd);
void GX_InitTexPreloadRegion(GXTexRegion *region,u32 tmem_even,u32 size_even,u32 tmem_odd,u32 size_odd);
void GX_InitTexObj(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap);
void GX_InitTexObjCI(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap,u32 tlut_name);
void GX_InitTexObjTlut(GXTexObj *obj,u32 tlut_name);	
void GX_LoadTexObj(GXTexObj *obj,u8 mapid);
void GX_LoadTlut(GXTlutObj *obj,u32 tlut_name);
void GX_LoadTexObjPreloaded(GXTexObj *obj,GXTexRegion *region,u8 mapid);
void GX_PreloadEntireTex(GXTexObj *obj,GXTexRegion *region);
void GX_InitTlutObj(GXTlutObj *obj,void *lut,u8 fmt,u16 entries);
void GX_InitTlutRegion(GXTlutRegion *region,u32 tmem_addr,u8 tlut_sz);
void GX_InitTexObjLOD(GXTexObj *obj,u8 minfilt,u8 magfilt,f32 minlod,f32 maxlod,f32 lodbias,u8 biasclamp,u8 edgelod,u8 maxaniso);
void GX_SetTexCoorScaleManually(u8 texcoord,u8 enable,u16 ss,u16 ts);
void GX_SetTexCoordBias(u8 texcoord,u8 s_enable,u8 t_enable);
GXTexRegionCallback GX_SetTexRegionCallback(GXTexRegionCallback cb);
GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback cb);

void GX_InitLightPos(GXLightObj *lit_obj,f32 x,f32 y,f32 z);
void GX_InitLightColor(GXLightObj *lit_obj,GXColor col);
void GX_InitLightDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz);
void GX_LoadLightObj(GXLightObj *lit_obj,u8 lit_id);
void GX_LoadLightObjIdx(u32 litobjidx,u8 litid);
void GX_InitLightDistAttn(GXLightObj *lit_obj,f32 ref_dist,f32 ref_brite,u8 dist_fn);
void GX_InitLightAttn(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2,f32 k0,f32 k1,f32 k2);
void GX_InitLightAttnA(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2);
void GX_InitLightAttnK(GXLightObj *lit_obj,f32 k0,f32 k1,f32 k2);
void GX_InitSpecularDirHA(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz,f32 hx,f32 hy,f32 hz);
void GX_InitSpecularDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz);
void GX_InitLightSpot(GXLightObj *lit_obj,f32 cut_off,u8 spotfn);
#define GX_InitLightShininess(lobj, shininess)   \
        GX_InitLightAttn(lobj, 0.0F, 0.0F, 1.0F, (shininess)/2.0F, 0.0F, 1.0F-(shininess)/2.0F )

void GX_SetGPMetric(u32 perf0,u32 perf1);
void GX_ClearGPMetric();
void GX_InitXfRasMetric();
void GX_ReadXfRasMetric(u32 *xfwaitin,u32 *xfwaitout,u32 *rasbusy,u32 *clks);
u32 GX_ReadClksPerVtx();
void GX_ClearVCacheMetric();
void GX_ReadVCacheMetric(u32 *check,u32 *miss,u32 *stall);
void GX_SetVCacheMetric(u32 attr);
void GX_GetGPStatus(u8 *overhi,u8 *underlow,u8 *readIdle,u8 *cmdIdle,u8 *brkpt);
u32 GX_GetOverflowCount();
void GX_ReadGPMetric(u32 *cnt0,u32 *cnt1);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
