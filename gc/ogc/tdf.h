#ifndef __TDF_H__
#define __TDF_H__

#include <gctypes.h>
#include <gx.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _tdpalette {
	u16 nitems;
	u32 fmt;
	void *data;
} TDPalette;

typedef struct _tdimage {
	u16 height;
	u16 width;
	u32 fmt;
	void *data;
} TDImage;

typedef struct _tdtexture {
	u32 wraps;
	u32 wrapt;
	u32 minfilter;
	u32 magfilter;
	f32 lodbias;
	u8 edgelod;
	u8 minlod;
	u8 maxlod;
	TDImage *image;
	TDPalette *palette;
} TDTexture;
// tdf file
typedef struct _tdfile {
	int ntextures;
	void *texdesc;
	char *tdf_name;
} TDFile;

u32 TDF_LoadHeader(TDFile* tdf, const char* file_name);
u32 TDF_LoadTexture(TDFile *tdf,u32 id,TDTexture **tex);
//void GetTexFromTDF( TDF* tdf, GXTexObj *tex, u32 id);
//void GetTexFromTDFCI( TDF* tdf, GXTexObj *tex, GXTlutObj *tlo, u32 tluts, u32 id);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
