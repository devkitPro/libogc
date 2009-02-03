#ifndef __GX_H__
#define __GX_H__

/*! 
 * \file gx.h 
 * \brief GX subsystem
 *
 */ 

#include <gctypes.h>
#include "lwp.h"
#include "gx_struct.h"
#include "gu.h"

#define GX_FALSE			0
#define GX_TRUE				1
#define GX_DISABLE			0
#define GX_ENABLE			1
#define GX_CLIP_DISABLE		0
#define GX_CLIP_ENABLE		1

#define GX_FIFO_MINSIZE		(64*1024)
#define GX_FIFO_HIWATERMARK	(16*1024)
#define GX_FIFO_OBJSIZE		128

#define GX_PERSPECTIVE		0
#define GX_ORTHOGRAPHIC		1

#define GX_MT_NULL			0
#define GX_MT_XF_FLUSH		1
#define GX_MT_DL_SAVE_CTX	2

#define GX_XF_FLUSH_NONE	0
#define GX_XF_FLUSH_SAFE	1

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

/*! \addtogroup vtxfmt Vertex format index
 * @{
 */
#define GX_VTXFMT0			0
#define GX_VTXFMT1			1
#define GX_VTXFMT2			2
#define GX_VTXFMT3			3
#define GX_VTXFMT4			4
#define GX_VTXFMT5			5
#define GX_VTXFMT6			6
#define GX_VTXFMT7			7
#define GX_MAXVTXFMT		8

/*! @} */


/*! \addtogroup vtxattrin Vertex data input type
 * @{
 */
#define GX_NONE				0			/*!< Input data is not used */
#define GX_DIRECT			1			/*!< Input data is set direct */
#define GX_INDEX8			2			/*!< Input data is set by a 8bit index */
#define GX_INDEX16			3			/*!< Input data is set by a 16bit index */

/*! @} */


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
#define GX_TRIANGLESTRIP		0x98
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

/* tex coord id 
   used by: XF: 0x1040,0x1050
            BP: 0x30
*/
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

/*! \addtogroup texfmt Texture format
 * @{
 */

#define GX_TF_I4			0x0
#define GX_TF_I8			0x1
#define GX_TF_IA4			0x2
#define GX_TF_IA8			0x3
#define GX_TF_RGB565		0x4
#define GX_TF_RGB5A3		0x5
#define GX_TF_RGBA8			0x6
#define GX_TF_CI4			0x8
#define GX_TF_CI8			0x9
#define GX_TF_CI14			0xa
#define GX_TF_CMPR			0xE

#define GX_TL_IA8			0x00
#define GX_TL_RGB565		0x01
#define GX_TL_RGB5A3		0x02

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

/*! \addtogroup ztexfmt Z Texture format
 * @{
 */

#define GX_TF_Z8			(0x1|_GX_TF_ZTF)
#define GX_TF_Z16			(0x3|_GX_TF_ZTF)
#define GX_TF_Z24X8			(0x6|_GX_TF_ZTF)

/*! @} */

/*! @} */

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

/*! \addtogroup ztexop Z Texture operator
 * @{
 */

#define GX_ZT_DISABLE		0
#define GX_ZT_ADD			1
#define GX_ZT_REPLACE		2
#define GX_MAX_ZTEXOP		3

/*! @} */


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

/* point/line fmt */
#define GX_TO_ZERO			0
#define GX_TO_SIXTEENTH		1
#define GX_TO_EIGHTH		2
#define GX_TO_FOURTH		3
#define GX_TO_HALF			4
#define GX_TO_ONE			5
#define GX_MAX_TEXOFFSET	6

/*! \addtogroup tevdefmode TEV combiner operation
 * Color/Alpha combiner modes for GX_SetTevOp().
 *
 * @{
 */

#define GX_MODULATE			0
#define GX_DECAL			1
#define GX_BLEND			2
#define GX_REPLACE			3
#define GX_PASSCLR			4

/*! @} */


/*! \addtogroup tevcolorarg TEV color combiner input
 * @{
 */

#define GX_CC_CPREV			0				/*!< Use the color value from previous TEV stage */
#define GX_CC_APREV			1				/*!< Use the alpha value from previous TEV stage */
#define GX_CC_C0			2				/*!< Use the color value from the color/output register 0 */
#define GX_CC_A0			3				/*!< Use the alpha value from the color/output register 0 */
#define GX_CC_C1			4				/*!< Use the color value from the color/output register 1 */
#define GX_CC_A1			5				/*!< Use the alpha value from the color/output register 1 */
#define GX_CC_C2			6				/*!< Use the color value from the color/output register 2 */
#define GX_CC_A2			7				/*!< Use the alpha value from the color/output register 2 */
#define GX_CC_TEXC			8				/*!< Use the color value from texture */
#define GX_CC_TEXA			9				/*!< Use the alpha value from texture */
#define GX_CC_RASC			10				/*!< Use the color value from rasterizer */
#define GX_CC_RASA			11				/*!< Use the alpha value from rasterizer */
#define GX_CC_ONE			12
#define GX_CC_HALF			13
#define GX_CC_KONST			14
#define GX_CC_ZERO			15				/*!< Use to pass zero value */

/*! @} */


/*! \addtogroup tevalphaarg TEV alpha combiner input
 * @{
 */

#define GX_CA_APREV			0				/*!< Use the alpha value from previous TEV stage */
#define GX_CA_A0			1				/*!< Use the alpha value from the color/output register 0 */
#define GX_CA_A1			2				/*!< Use the alpha value from the color/output register 1 */			
#define GX_CA_A2			3				/*!< Use the alpha value from the color/output register 2 */
#define GX_CA_TEXA			4				/*!< Use the alpha value from texture */
#define GX_CA_RASA			5				/*!< Use the alpha value from rasterizer */
#define GX_CA_KONST			6
#define GX_CA_ZERO			7				/*!< Use to pass zero value */

/*! @} */


/*! \addtogroup tevstage TEV stage
 * @{
 */

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

/*! @} */


/*! \addtogroup tevop TEV combiner operator
 * @{
 */

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

/*! @} */


/*! \addtogroup tevbias TEV bias value
 * @{
 */

#define GX_TB_ZERO				0
#define GX_TB_ADDHALF			1
#define GX_TB_SUBHALF			2
#define GX_MAX_TEVBIAS			3

/*! @} */


/*! \addtogroup tevbias TEV clamp mode value
 * @{
 */

#define GX_TC_LINEAR			0
#define GX_TC_GE				1
#define GX_TC_EQ				2
#define GX_TC_LE				3
#define GX_MAX_TEVCLAMPMODE		4

/*! @} */


/*! \addtogroup tevscale TEV scale value
 * @{
 */

#define GX_CS_SCALE_1			0
#define GX_CS_SCALE_2			1
#define GX_CS_SCALE_4			2
#define GX_CS_DIVIDE_2			3
#define GX_MAX_TEVSCALE			4

/*! @} */


/*! \addtogroup tevcoloutreg TEV color/output register
 * @{
 */

#define GX_TEVPREV				0
#define GX_TEVREG0				1
#define GX_TEVREG1				2
#define GX_TEVREG2				3
#define GX_MAX_TEVREG			4

/*! @} */


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

/* tev const color reg id */
#define GX_KCOLOR0				0
#define GX_KCOLOR1				1
#define GX_KCOLOR2				2
#define GX_KCOLOR3				3
#define GX_KCOLOR_MAX			4

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


/*! \addtogroup tevswapsel TEV color swap table entry
 * @{
 */

#define GX_TEV_SWAP0					0
#define GX_TEV_SWAP1					1
#define GX_TEV_SWAP2					2
#define GX_TEV_SWAP3					3
#define GX_MAX_TEVSWAP					4

/*! @} */


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

#define GX_FOG_PERSP_LIN				2
#define GX_FOG_PERSP_EXP				4
#define GX_FOG_PERSP_EXP2				5
#define GX_FOG_PERSP_REVEXP				6
#define GX_FOG_PERSP_REVEXP2			7

#define GX_FOG_ORTHO_LIN				10
#define GX_FOG_ORTHO_EXP				12
#define GX_FOG_ORTHO_EXP2				13
#define GX_FOG_ORTHO_REVEXP				14
#define GX_FOG_ORTHO_REVEXP2			15

#define GX_FOG_LIN						GX_FOG_PERSP_LIN
#define GX_FOG_EXP						GX_FOG_PERSP_EXP
#define GX_FOG_EXP2						GX_FOG_PERSP_EXP2
#define GX_FOG_REVEXP  					GX_FOG_PERSP_REVEXP
#define GX_FOG_REVEXP2 					GX_FOG_PERSP_REVEXP2


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

/*! \addtogroup xfbclamp XFB clamp modes
 * @{
 */

#define GX_CLAMP_NONE					0
#define GX_CLAMP_TOP					1
#define GX_CLAMP_BOTTOM					2

/*! @} */


/*! \addtogroup gammamode Gamma values
 * @{
 */

#define GX_GM_1_0						0
#define GX_GM_1_7						1
#define GX_GM_2_2						2

/*! @} */


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

#define GX_MAX_VTXDESC					GX_VA_MAXATTR
#define GX_MAX_VTXDESC_LISTSIZE			(GX_VA_MAXATTR+1)

#define GX_MAX_VTXATTRFMT				GX_VA_MAXATTR
#define GX_MAX_VTXATTRFMT_LISTSIZE		(GX_VA_MAXATTR+1)

#define GX_MAX_Z24						0x00ffffff

#define WGPIPE			(0xCC008000)

#define FIFO_PUTU8(x)	*(vu8*)WGPIPE = (u8)(x)
#define FIFO_PUTS8(x)	*(vs8*)WGPIPE = (s8)(x)
#define FIFO_PUTU16(x)	*(vu16*)WGPIPE = (u16)(x)
#define FIFO_PUTS16(x)	*(vs16*)WGPIPE = (s16)(x)
#define FIFO_PUTU32(x)	*(vu32*)WGPIPE = (u32)(x)
#define FIFO_PUTS32(x)	*(vs32*)WGPIPE = (s32)(x)
#define FIFO_PUTF32(x)	*(vf32*)WGPIPE = (f32)(x)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _gx_color {
 	u8 r;
 	u8 g;
 	u8 b;
	u8 a;
} GXColor;

typedef struct _gx_colors10 {
 	s16 r;
 	s16 g;
 	s16 b;
	s16 a;
} GXColorS10;

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
	u8 attr;
	u8 type;
} GXVtxDesc;

typedef struct {
	u32 vtxattr;
	u32 comptype;
	u32 compsize;
	u32 frac;
} GXVtxAttrFmt;

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

/*! \typedef void (*GXDrawSyncCallback)(u16 token)
\brief function pointer typedef for the drawsync-token callback
\param[out] token tokenvalue most recently encountered.
*/
typedef void (*GXDrawSyncCallback)(u16 token);

/*! \typedef GXTexRegion* (*GXTexRegionCallback)(GXTexObj *obj,u8 mapid)
\brief function pointer typedef for the texture region callback
\param[out] token tokenvalue most recently encountered.
*/
typedef GXTexRegion* (*GXTexRegionCallback)(GXTexObj *obj,u8 mapid);

/*! \typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 tlut_name)
\brief function pointer typedef for the TLUT region callback
\param[out] token tokenvalue most recently encountered.
*/
typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 tlut_name);

/*! 
 * \fn GXFifoObj* GX_Init(void *base,u32 size)
 * \brief Initializes the graphics processor to its initial state and should be called before any other GX functions.
 *
 * \param[in] base pointer to the GX fifo buffer base address. Must be aligned on a 32 Byte boundery.
 * \param[in] size size of buffer. Must be a multiple of 32.
 *
 * \return pointer to the intialized GXFifoObj object.
 */
GXFifoObj* GX_Init(void *base,u32 size);

void GX_InitFifoBase(GXFifoObj *fifo,void *base,u32 size);
void GX_InitFifoLimits(GXFifoObj *fifo,u32 hiwatermark,u32 lowatermark);
void GX_InitFifoPtrs(GXFifoObj *fifo,void *rd_ptr,void *wt_ptr);
void GX_SetCPUFifo(GXFifoObj *fifo);
void GX_SetGPFifo(GXFifoObj *fifo);
void GX_GetCPUFifo(GXFifoObj *fifo);
void GX_GetGPFifo(GXFifoObj *fifo);
GXDrawDoneCallback GX_SetDrawDoneCallback(GXDrawDoneCallback cb);
GXBreakPtCallback GX_SetBreakPtCallback(GXBreakPtCallback cb);

void GX_AbortFrame();
void GX_Flush();
void GX_SetMisc(u32 token,u32 value);
void GX_SetDrawDone();
void GX_WaitDrawDone();

/*! 
 * \fn u16 GX_GetDrawSync()
 * \brief Returns the value of the token register, which is written using the GX_SetDrawSync() function.
 *
 * \return the value of the token register, u16.
 */
u16 GX_GetDrawSync();

/*! 
 * \fn void GX_SetDrawSync(u16 token)
 * \brief This function sends a token into the command stream.
 *        When the token register is set, an interrupt will also be received by the CPU. You can install a callback on this interrupt
 *        with GX_SetDrawSyncCallback(). Draw syncs can be used to notify the CPU that the graphics processor is finished using a shared
 *        resource (a vertex array for instance).
 *
 * \param[in] token 16-bit value to write to the token register.
 *
 * \return none
 */
void GX_SetDrawSync(u16 token);

/*! 
 * \fn GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb)
 * \brief Installs a callback that is invoked whenever a DrawSync token is encountered by the graphics pipeline. The callbacks argument is 
 *        is the value of the token most recently encountered. Since it is possible to miss tokens(graphics processing does not stop while
 *        the callback is running), your code should be capable of deducing if any tokens have been missed.
 *
 * \param[in] cb callback to be invoked when the DrawSnyc tokens are encountered in the graphics pipeline.
 *
 * \return pointer to the previously set callback function.
 */
GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb);

void GX_DisableBreakPt();
void GX_EnableBreakPt(void *break_pt);
void GX_DrawDone();
void GX_TexModeSync();
void GX_InvVtxCache();
void GX_ClearVtxDesc();
void GX_LoadProjectionMtx(Mtx44 mt,u8 type);
void GX_SetViewport(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ);
void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field);
void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn);
void GX_SetChanAmbColor(s32 channel,GXColor color);
void GX_SetChanMatColor(s32 channel,GXColor color);
void GX_SetArray(u32 attr,void *ptr,u8 stride);

/*! 
 * \fn void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac)
 * \brief Installs a callback that is invoked whenever a DrawSync token is encountered by the graphics pipeline. The callbacks argument is 
 *        is the value of the token most recently encountered. Since it is possible to miss tokens(graphics processing does not stop while
 *        the callback is running), your code should be capable of deducing if any tokens have been missed.
 *
 * \param[in] vtxfmt \ref vtxfmt
 * \param[in] vtxattr to be invoked when the DrawSnyc tokens are encountered in the graphics pipeline.
 * \param[in] comptype to be invoked when the DrawSnyc tokens are encountered in the graphics pipeline.
 * \param[in] compsize to be invoked when the DrawSnyc tokens are encountered in the graphics pipeline.
 * \param[in] frac to be invoked when the DrawSnyc tokens are encountered in the graphics pipeline.
 *
 * \return none
 */
void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac);
void GX_SetVtxAttrFmtv(u8 vtxfmt,GXVtxAttrFmt *attr_list);
void GX_SetVtxDesc(u8 attr,u8 type);
void GX_SetVtxDescv(GXVtxDesc *attr_list);

u32 GX_EndDispList();
void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt);
void GX_BeginDispList(void *list,u32 size);
void GX_CallDispList(void *list,u32 nbytes);

static inline void GX_End()
{
}

static inline void GX_Position3f32(f32 x,f32 y,f32 z)
{
	FIFO_PUTF32(x);
	FIFO_PUTF32(y);
	FIFO_PUTF32(z);
}

static inline void GX_Position3u16(u16 x,u16 y,u16 z)
{
	FIFO_PUTU16(x);
	FIFO_PUTU16(y);
	FIFO_PUTU16(z);
}

static inline void GX_Position3s16(s16 x,s16 y,s16 z)
{
	FIFO_PUTS16(x);
	FIFO_PUTS16(y);
	FIFO_PUTS16(z);
}

static inline void GX_Position3u8(u8 x,u8 y,u8 z)
{
	FIFO_PUTU8(x);
	FIFO_PUTU8(y);
	FIFO_PUTU8(z);
}

static inline void GX_Position3s8(s8 x,s8 y,s8 z)
{
	FIFO_PUTS8(x);
	FIFO_PUTS8(y);
	FIFO_PUTS8(z);
}

static inline void GX_Position2f32(f32 x,f32 y)
{
	FIFO_PUTF32(x);
	FIFO_PUTF32(y);
}

static inline void GX_Position2u16(u16 x,u16 y)
{
	FIFO_PUTU16(x);
	FIFO_PUTU16(y);
}

static inline void GX_Position2s16(s16 x,s16 y)
{
	FIFO_PUTS16(x);
	FIFO_PUTS16(y);
}

static inline void GX_Position2u8(u8 x,u8 y)
{
	FIFO_PUTU8(x);
	FIFO_PUTU8(y);
}

static inline void GX_Position2s8(s8 x,s8 y)
{
	FIFO_PUTS8(x);
	FIFO_PUTS8(y);
}

static inline void GX_Position1x8(u8 index)
{
	FIFO_PUTU8(index);
}

static inline void GX_Position1x16(u16 index)
{
	FIFO_PUTU16(index);
}

static inline void GX_Normal3f32(f32 nx,f32 ny,f32 nz)
{
	FIFO_PUTF32(nx);
	FIFO_PUTF32(ny);
	FIFO_PUTF32(nz);
}

static inline void GX_Normal3s16(s16 nx,s16 ny,s16 nz)
{
	FIFO_PUTS16(nx);
	FIFO_PUTS16(ny);
	FIFO_PUTS16(nz);
}

static inline void GX_Normal3s8(s8 nx,s8 ny,s8 nz)
{
	FIFO_PUTS8(nx);
	FIFO_PUTS8(ny);
	FIFO_PUTS8(nz);
}

static inline void GX_Normal1x8(u8 index)
{
	FIFO_PUTU8(index);
}

static inline void GX_Normal1x16(u16 index)
{
	FIFO_PUTU16(index);
}

static inline void GX_Color4u8(u8 r,u8 g,u8 b,u8 a)
{
	FIFO_PUTU8(r);
	FIFO_PUTU8(g);
	FIFO_PUTU8(b);
	FIFO_PUTU8(a);
}

static inline void GX_Color3u8(u8 r,u8 g,u8 b)
{
	FIFO_PUTU8(r);
	FIFO_PUTU8(g);
	FIFO_PUTU8(b);
}

static inline void GX_Color3f32(f32 r, f32 g, f32 b)
{

	FIFO_PUTU8((u8)(r * 255.0));
	FIFO_PUTU8((u8)(g * 255.0));
	FIFO_PUTU8((u8)(b * 255.0));

}


static inline void GX_Color1u32(u32 clr)
{
	FIFO_PUTU32(clr);
}

static inline void GX_Color1u16(u16 clr)
{
	FIFO_PUTU16(clr);
}

static inline void GX_Color1x8(u8 index)
{
	FIFO_PUTU8(index);
}

static inline void GX_Color1x16(u16 index)
{
	FIFO_PUTU16(index);
}

static inline void GX_TexCoord2f32(f32 s,f32 t)
{
	FIFO_PUTF32(s);
	FIFO_PUTF32(t);
}

static inline void GX_TexCoord2u16(u16 s,u16 t)
{
	FIFO_PUTU16(s);
	FIFO_PUTU16(t);
}

static inline void GX_TexCoord2s16(s16 s,s16 t)
{
	FIFO_PUTS16(s);
	FIFO_PUTS16(t);
}

static inline void GX_TexCoord2u8(u8 s,u8 t)
{
	FIFO_PUTU8(s);
	FIFO_PUTU8(t);
}

static inline void GX_TexCoord2s8(s8 s,s8 t)
{
	FIFO_PUTS8(s);
	FIFO_PUTS8(t);
}

static inline void GX_TexCoord1f32(f32 s)
{
	FIFO_PUTF32(s);
}

static inline void GX_TexCoord1u16(u16 s)
{
	FIFO_PUTU16(s);
}

static inline void GX_TexCoord1s16(s16 s)
{
	FIFO_PUTS16(s);
}

static inline void GX_TexCoord1u8(u8 s)
{
	FIFO_PUTU8(s);
}

static inline void GX_TexCoord1s8(s8 s)
{
	FIFO_PUTS8(s);
}

static inline void GX_TexCoord1x8(u8 index)
{
	FIFO_PUTU8(index);
}

static inline void GX_TexCoord1x16(u16 index)
{
	FIFO_PUTU16(index);
}

static inline void GX_MatrixIndex1x8(u8 index)
{
	FIFO_PUTU8(index);
}


void GX_AdjustForOverscan(GXRModeObj *rmin,GXRModeObj *rmout,u16 hor,u16 ver);
void GX_LoadPosMtxImm(Mtx mt,u32 pnidx);
void GX_LoadPosMtxIdx(u16 mtxidx,u32 pnidx);
void GX_LoadNrmMtxImm(Mtx mt,u32 pnidx);
void GX_LoadNrmMtxIdx3x3(u16 mtxidx,u32 pnidx);
void GX_LoadTexMtxImm(Mtx mt,u32 texidx,u8 type);
void GX_LoadTexMtxIdx(u16 mtxidx,u32 texidx,u8 type);
void GX_SetCurrentMtx(u32 mtx);

/*! 
 * \fn void GX_SetTevOp(u8 tevstage,u8 mode)
 * \brief Simplified function to set GX_SetTevColorIn(), GX_SetTevAlphaIn(), GX_SetTevColorOp() and GX_SetTevAlphaOp() for 
 *        this <b><i>tevstage</i></b> based on a predefined combiner <b><i>mode</i></b>.
 *
 * \param[in] tevstage \ref tevstage.
 * \param[in] mode \ref tevdefmode
 *
 * \return none
 */
void GX_SetTevOp(u8 tevstage,u8 mode);

/*! 
 * \fn void GX_SetTevColor(u8 tev_regid,GXColor color)
 * \brief Sets the vertical clamping mode to use during the EFB to XFB or texture copy.
 *
 * \param[in] tev_regid \ref tevcoloutreg.
 * \param[in] color Constant color value.
 *
 * \return none
 */
void GX_SetTevColor(u8 tev_regid,GXColor color);

/*! 
 * \fn void GX_SetTevColorS10(u8 tev_regid,GXColor color)
 * \brief Sets the vertical clamping mode to use during the EFB to XFB or texture copy.
 *
 * \param[in] tev_regid \ref tevcoloutreg.
 * \param[in] color Constant color value in S10 format.
 *
 * \return none
 */
void GX_SetTevColorS10(u8 tev_regid,GXColorS10 color);

/*! 
 * \fn void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
 * \brief Sets the color input sources for one <b><i>tevstage</i></b> of the Texture Environment (TEV) color combiner.
 *	      This includes constant (register) colors and alphas, texture color/alpha, rasterized color/alpha (the result
 *        of per-vertex lighting), and a few useful constants.<br><br>
 *        <b><i>Note:</i></b> The input controls are independent for each TEV stage.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] a \ref tevcolorarg
 * \param[in] b \ref tevcolorarg
 * \param[in] c \ref tevcolorarg
 * \param[in] d \ref tevcolorarg
 *
 * \return none
 */
void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d);

/*! 
 * \fn void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
 * \brief Sets the alpha input sources for one <b><i>tevstage</i></b> of the Texture Environment (TEV) alpha combiner.
 *        There are fewer alpha inputs than color inputs, and there are no color channels available in the alpha combiner.<br><br>
 *        <b><i>Note:</i></b> The input controls are independent for each TEV stage.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] a \ref tevalphaarg
 * \param[in] b \ref tevalphaarg
 * \param[in] c \ref tevalphaarg
 * \param[in] d \ref tevalphaarg
 *
 * \return none
 */
void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d);

/*! 
 * \fn void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
 * \brief Sets the <b><i>tevop</i></b>, <b><i>tevbias</i></b>, <b><i>tevscale</i></b> and <b><i>clamp</i></b>-mode operation for the color combiner 
 *        for this <b><i>tevstage</i></b> of the Texture Environment (TEV) unit. This function also specifies the register, <b><i>tevregid</i></b>, that
 *        will contain the result of the color combiner function. The color combiner function is:<br><br>
 *		  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<b><i>tevregid</i></b> = (d (<b><i>tevop</i></b>) ((1.0 - c)*a + c*b) + <b><i>tevbias</i></b>) * <b><i>tevscale</i></b>;<br><br>
 *        The input sources a,b,c and d are set using GX_SetTevColorIn().
 *
 * \param[in] tevstage \ref tevstage.
 * \param[in] tevop \ref tevop
 * \param[in] tevbias \ref tevbias.
 * \param[in] tevscale \ref tevscale.
 * \param[in] clamp Clamp results when GX_TRUE.
 * \param[in] tevregid \ref tevcoloutreg
 *
 * \return none
 */
void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid);


/*! 
 * \fn void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
 * \brief Sets the <b><i>tevop</i></b>, <b><i>tevbias</i></b>, <b><i>tevscale</i></b> and <b><i>clamp</i></b>-mode operation for the alpha combiner 
 *        for this <b><i>tevstage</i></b> of the Texture Environment (TEV) unit. This function also specifies the register, <b><i>tevregid</i></b>, that
 *        will contain the result of the alpha combiner function. The alpha combiner function is:<br><br>
 *        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<b><i>tevregid</i></b> = (d (<b><i>tevop</i></b>) ((1.0 - c)*a + c*b) + <b><i>tevbias</i></b>) * <b><i>tevscale</i></b>;<br><br>
 *        The input sources a,b,c and d are set using GX_SetTevAlphaIn().
 *
 * \param[in] tevstage \ref tevstage.
 * \param[in] tevop \ref tevop
 * \param[in] tevbias \ref tevbias.
 * \param[in] tevscale \ref tevscale.
 * \param[in] clamp Clamp results when GX_TRUE.
 * \param[in] tevregid \ref tevcoloutreg
 *
 * \return none
 */
void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid);
void GX_SetNumTexGens(u32 nr);
void GX_SetTexCoordGen(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc);
void GX_SetTexCoordGen2(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc,u32 normalize,u32 postmtx);

/*! 
 * \fn void GX_SetZTexture(u8 op,u8 fmt,u32 bias)
 * \brief Sets the vertical clamping mode to use during the EFB to XFB or texture copy.
 *
 * \param[in] op \ref ztexop
 * \param[in] fmt \ref ztexfmt
 * \param[in] bias Bias value. Format is 24bit unsigned.
 *
 * \return none
 */
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
void GX_SetTevKColor(u8 sel, GXColor col);
void GX_SetTevKColorSel(u8 tevstage,u8 sel);
void GX_SetTevKAlphaSel(u8 tevstage,u8 sel);
void GX_SetTevKColorS10(u8 sel, GXColorS10 col);

/*! 
 * \fn void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel)
 * \brief Selects a set of swap modes for the rasterized color and texture color for a given TEV stage. This allows the
 *        color components of these inputs to be rearranged or duplicated.
 *        There are four different swap mode table entries and each entry in the table specifies how the RGBA inputs map
 *        to the RGBA outputs. See GX_SetTevSwapModeTable() for more information.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] ras_sel selects a swap mode for the rasterized color input.
 * \param[in] tex_sel selects a swap mode for the texture color input.
 *
 * \return none
 */
void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel);

/*! 
 * \fn void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a)
 * \brief Sets up the TEV color swap table. The swap table allows the rasterized color and texture color to be swapped component-wise.
 *        An entry in the table specifies how the input color components map to the output color components.
 *
 * \param[in] swapid \ref tevswapsel
 * \param[in] r input color component that should be mapped to the red output component.
 * \param[in] g input color component that should be mapped to the green output component.
 * \param[in] b input color component that should be mapped to the blue output component.
 * \param[in] a input color component that should be mapped to the alpha output component.
 *
 * \return none
 */
void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a);
void GX_SetTevIndirect(u8 tevstage,u8 indtexid,u8 format,u8 bias,u8 mtxid,u8 wrap_s,u8 wrap_t,u8 addprev,u8 utclod,u8 a);
void GX_SetTevDirect(u8 tevstage);
void GX_SetNumIndStages(u8 nstages);
void GX_SetIndTexOrder(u8 indtexstage,u8 texcoord,u8 texmap);
void GX_SetIndTexCoordScale(u8 indtexid,u8 scale_s,u8 scale_t);
void GX_SetFog(u8 type,f32 startz,f32 endz,f32 nearz,f32 farz,GXColor col);
void GX_SetFogRangeAdj(u8 enable,u16 center,GXFogAdjTbl *table);
void GX_SetIndTexMatrix(u8 indtexmtx,f32 offset_mtx[2][3],s8 scale_exp);
void GX_SetTevIndBumpST(u8 tevstage,u8 indstage,u8 mtx_sel);
void GX_SetTevIndBumpXYZ(u8 tevstage,u8 indstage,u8 mtx_sel);
void GX_SetTevIndTile(u8 tevstage,u8 indtexid,u16 tilesize_x,u16 tilesize_y,u16 tilespacing_x,u16 tilespacing_y,u8 indtexfmt,u8 indtexmtx,u8 bias_sel,u8 alpha_sel);

void GX_SetColorUpdate(u8 enable);
void GX_SetAlphaUpdate(u8 enable);
void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt);
void GX_SetDither(u8 dither);
void GX_SetDstAlpha(u8 enable,u8 a);
void GX_SetFieldMask(u8 even_mask,u8 odd_mask);
void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio);

/*! 
 * \fn f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight)
 * \brief Calculates an appropriate Y scale factor value for GX_SetDispCopyYScale based on the height of the EFB and
 *        the height of the XFB.
 *
 * \param[in] efbHeight Height of embedded framebuffer. Range from 2 to 528. Should be a multiple of 2.
 * \param[in] xfbHeight Height of external framebuffer. Range from 2 to 1024. Should be equal or greater than efbHeight.
 *
 * \return Y scale factor which can be used as argument of GX_SetDispCopyYScale().
 */
f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight);

/*! 
 * \fn u32 GX_SetDispCopyYScale(f32 yscale)
 * \brief Sets the vertical scale factor for the EFB to XFB copy operation. The number of actual lines copied is returned, based on the current EFB height.
 *        You can use this number to allocate the proper XFB size. You have to call GX_SetDispCopySrc() prior to this function call if you want to get the 
 *        number of lines by using this function. There is also another way (GX_GetNumXfbLines) to calculate this number.
 *
 * \param[in] yscale Vertical scale value. Range from 1.0 to 256.0.
 *
 * \return Number of lines that will be copied.
 */
u32 GX_SetDispCopyYScale(f32 yscale);

/*! 
 * \fn void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht)
 * \brief Sets the source parameters for the EFB to XFB copy operation.
 *
 * \param[in] left left most source pixel to copy. Must be a multiple of 2 pixels.
 * \param[in] top top most source line to copy. Must be a multiple of 2 lines.
 * \param[in] wd width in pixels to copy. Must be a multiple of 2 pixels.
 * \param[in] ht height in lines to copy. Must be a multiple of 2 lines.
 *
 * \return none
 */
void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht);

/*! 
 * \fn void GX_SetDispCopyDst(u16 wd,u16 ht)
 * \brief Sets the witdth and height of the display buffer in pixels. The application typical renders an image into the EFB(source) and
 *        then copies it into the XFB(destination) in main memory. The <b><i>wd</i></b> specifies the number of pixels between adjacent lines in the 
 *        destination buffer and can be different than the width of the EFB.
 *
 * \param[in] wd Distance between successive lines in the XFB, in pixels. Must be a multiple of 16.
 * \param[in] ht Height of the XFB in lines.
 *
 * \return none
 */
void GX_SetDispCopyDst(u16 wd,u16 ht);

/*! 
 * \fn void GX_SetCopyClamp(u8 clamp)
 * \brief Sets the vertical clamping mode to use during the EFB to XFB or texture copy.
 *
 * \param[in] clamp clamp mode. bit-wise OR of \ref xfbclamp. Use GX_CLAMP_NONE for no clamping.
 *
 * \return none
 */
void GX_SetCopyClamp(u8 clamp);

/*! 
 * \fn void GX_SetDispCopyGamma(u8 gamma)
 * \brief Sets the gamma correction applied to pixels during EFB to XFB copy operation.
 *
 * \param[in] gamma \ref gammamode
 *
 * \return none
 */
void GX_SetDispCopyGamma(u8 gamma);

void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7]);
void GX_SetDispCopyFrame2Field(u8 mode);

/*! 
 * \fn void GX_SetCopyClear(GXColor color,u32 zvalue)
 * \brief Sets color and Z value to clear the EFB to, during copy operations. These values are used during both display copies and texture copies.
 *
 * \param[in] color RGBA color (8-bit/component) to use during clear operation.
 * \param[in] zvalue 24-bit Z value to use during clear operation. Use the constant GX_MAX_Z24 to specify the maximum depth value.
 *
 * \return none
 */
void GX_SetCopyClear(GXColor color,u32 zvalue);

/*! 
 * \fn void GX_CopyDisp(void *dest,u8 clear)
 * \brief Copies the embedded framebuffer(XFB) to the external framebuffer(XFB) in main memory. The stride of
 *        the XFB is set using GX_SetDispCopyDst(). The source image in the EFB is described using GX_SetDispCopySrc().
 *
 *        The graphics processor will stall all graphics commands util the copy is complete.
 *
 *        If the <b><i>clear</i></b> flag is true, the color and Z buffers will be cleared during the copy. They will be 
 *        cleared to the constant values set using GX_SetCopyClear().
 *
 * \param[in] dest pointer to the external framebuffer. <b><i>dest</i></b> should be 32B aligned.
 * \param[in] clear flag that indicates framebuffer should be cleared if GX_TRUE.
 *
 * \return none
 */
void GX_CopyDisp(void *dest,u8 clear);

void GX_SetTexCopySrc(u16 left,u16 top,u16 wd,u16 ht);

/*! 
 * \fn void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap)
 * \brief Copies the embedded framebuffer(XFB) to the texture image buffer <b><i>dest</i></b> in main memory. This is useful
 *        when creating textures using the Graphics Processor(CP).
 *        If the <b><i>clear</i></b> flag is set to GX_TRUE, the EFB will be cleared to the current color(see GX_SetCopyClear()) 
 *        during the copy operation.
 *
 * \param[in] wd pointer to the image buffer in main memory. <b><i>dest</i></b> should be 32B aligned.
 * \param[in] ht flag that indicates framebuffer should be cleared if GX_TRUE.
 * \param[in] fmt \ref texfmt
 * \param[in] mipmap 
 *
 * \return none
 */
void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap);

/*! 
 * \fn void GX_CopyTex(void *dest,u8 clear)
 * \brief Copies the embedded framebuffer(XFB) to the texture image buffer <b><i>dest</i></b> in main memory. This is useful
 *        when creating textures using the Graphics Processor(CP).
 *        If the <b><i>clear</i></b> flag is set to GX_TRUE, the EFB will be cleared to the current color(see GX_SetCopyClear()) 
 *        during the copy operation.
 *
 * \param[in] dest pointer to the image buffer in main memory. <b><i>dest</i></b> should be 32B aligned.
 * \param[in] clear flag that indicates framebuffer should be cleared if GX_TRUE.
 *
 * \return none
 */
void GX_CopyTex(void *dest,u8 clear);

/*! 
 * \fn void GX_PixModeSync()
 * \brief Inserts a synchronization command into the graphics FIFO. When the GPU sees this command it will allow the
 *        rest of the pipe to flush before continuing. This command is useful in certain situation such as after a GX_CopyTex()
 *        command and before a primitive that uses the copied texture.
 *
 * \return none
 */
void GX_PixModeSync();

/*! 
 * \fn void GX_ClearBoundingBox()
 * \brief Clears the bounding box values before a new image is drawn.
 *        The graphics hardware keeps track of a bounding box of pixel coordinates that are drawn in the EFB.
 *
 * \return none
 */
void GX_ClearBoundingBox();
void GX_PokeAlphaMode(u8 func,u8 threshold);
void GX_PokeAlphaUpdate(u8 update_enable);
void GX_PokeColorUpdate(u8 update_enable);
void GX_PokeDither(u8 dither);
void GX_PokeBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op);
void GX_PokeAlphaRead(u8 mode);
void GX_PokeDstAlpha(u8 enable,u8 a);
void GX_PokeARGB(u16 x,u16 y,GXColor color);
void GX_PokeZ(u16 x,u16 y,u32 z);
void GX_PokeZMode(u8 comp_enable,u8 func,u8 update_enable);

u32 GX_GetTexObjFmt(GXTexObj *obj);
u32 GX_GetTexObjMipMap(GXTexObj *obj);

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

u32 GX_ReadClksPerVtx();
u32 GX_GetOverflowCount();
u32 GX_ResetOverflowCount();
lwp_t GX_GetCurrentGXThread();
lwp_t GX_SetCurrentGXThread();
void GX_RestoreWriteGatherPipe();
void GX_SetGPMetric(u32 perf0,u32 perf1);
void GX_ClearGPMetric();
void GX_InitXfRasMetric();
void GX_ReadXfRasMetric(u32 *xfwaitin,u32 *xfwaitout,u32 *rasbusy,u32 *clks);
void GX_ClearVCacheMetric();
void GX_ReadVCacheMetric(u32 *check,u32 *miss,u32 *stall);
void GX_SetVCacheMetric(u32 attr);
void GX_GetGPStatus(u8 *overhi,u8 *underlow,u8 *readIdle,u8 *cmdIdle,u8 *brkpt);
void GX_ReadGPMetric(u32 *cnt0,u32 *cnt1);
void GX_GetFifoPtrs(GXFifoObj *fifo,void **rd_ptr,void **wt_ptr);
volatile void* GX_RedirectWriteGatherPipe(void *ptr);

#define GX_InitLightPosv(lo,vec) \
    (GX_InitLightPos((lo), *(f32*)(vec), *((f32*)(vec)+1), *((f32*)(vec)+2)))

#define GX_InitLightDirv(lo,vec) \
    (GX_InitLightDir((lo), *(f32*)(vec), *((f32*)(vec)+1), *((f32*)(vec)+2)))

#define GX_InitSpecularDirv(lo,vec) \
    (GX_InitSpecularDir((lo), *(f32*)(vec), *((f32*)(vec)+1), *((f32*)(vec)+2)))

#define GX_InitSpecularDirHAv(lo,vec0,vec1) \
    (GX_InitSpecularDirHA((lo), \
    *(f32*)(vec0), *((f32*)(vec0)+1), *((f32*)(vec0)+2), \
    *(f32*)(vec1), *((f32*)(vec1)+1), *((f32*)(vec1)+2)))

#define GX_InitLightShininess(lobj, shininess) \
    (GX_InitLightAttn(lobj, 0.0F, 0.0F, 1.0F,  \
                    (shininess)/2.0F, 0.0F,   \
                    1.0F-(shininess)/2.0F ))

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
