#ifndef __TDF_H__
#define __TDF_H__

#include <gctypes.h>
#include <gx.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

// texture header
typedef struct
{
	u16 height;
	u16 width;
	u32 fmt;
	u32* data;
	u32 wraps;
	u32 wrapt;
	u32 minfilter;
	u32 magfilter;
	float lodbias;
	u8 edgelod;
	u8 minlod;
	u8 maxlod;
	u8 unpacked;
} TexHeader;

// texture palette header
typedef struct
{
	u16 nitems;
	u8 unpacked;
	u8 pad;
	u32 fmt;
	u32* data;
} TexPalHeader;

// texture descriptor
typedef struct
{
	TexHeader *texhead;
	TexPalHeader *palhead;
} TexDesc;

// tdf file
typedef struct
{
	int ntextures;
	TexDesc *texdesc;
} TDF;

int LoadTDFHeader(TDF* tdf, const char* file_name);
void GetTexFromTDF( TDF* tdf, GXTexObj *tex, u32 id);
void GetTexFromTDFCI( TDF* tdf, GXTexObj *tex, GXTlutObj *tlo, u32 tluts, u32 id);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
