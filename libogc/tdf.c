#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "tdf.h"

#define LOWORD(l)           ((u16)(l))
#define HIWORD(l)           ((u16)(((u32)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)           ((u8)(w))
#define HIBYTE(w)           ((u8)(((u16)(w) >> 8) & 0xFF))

#define SwapInt(n)			(LOBYTE(LOWORD(n))<<24) + (HIBYTE(LOWORD(n))<<16) + (LOBYTE(HIWORD(n))<<8) + HIBYTE(HIWORD(n))
#define SwapShort(n)		(LOBYTE(n)<<8) + HIBYTE(n);

// texture header
typedef struct _tdimgheader {
	u16 height;
	u16 width;
	u32 fmt;
	u32 data_offset;
	u32 wraps;
	u32 wrapt;
	u32 minfilter;
	u32 magfilter;
	f32 lodbias;
	u8 edgelod;
	u8 minlod;
	u8 maxlod;
	u8 unpacked;
} TDImgHeader;

// texture palette header
typedef struct _tdpalheader {
	u16 nitems;
	u8 unpacked;
	u8 pad;
	u32 fmt;
	u32 data_offset;
} TDPalHeader;

// texture descriptor
typedef struct _tddesc {
	TDImgHeader *imghead;
	TDPalHeader *palhead;
} TDDescHeader;

static u32 TDF_GetTextureSize(u32 width,u32 height,u32 fmt)
{
	u32 size = 0;

	switch(fmt) {
			case GX_TF_I4:
			case GX_TF_CI4:
			case GX_TF_CMPR:
				size = ((width+7)>>3)*((height+7)>>3)*32;
				break;
			case GX_TF_I8:
			case GX_TF_IA4:
			case GX_TF_CI8:
				size = ((width+7)>>3)*((height+7)>>2)*32;
				break;
			case GX_TF_IA8:
			case GX_TF_CI14:
			case GX_TF_RGB565:
			case GX_TF_RGB5A3:
				size = ((width+3)>>2)*((height+3)>>2)*32;
				break;
			case GX_TF_RGBA8:
				size = ((width+3)>>2)*((height+3)>>2)*32*2;
				break;
			default:
				break;
	}
	return size;
}

u32 TDF_OpenTDFile(TDFile* tdf, const char* file_name)
{
	u32 c;
	TDDescHeader *deschead;
	TDImgHeader *imghead;
	TDPalHeader *palhead;
	FILE *tdffile = NULL;

	if(!file_name) return 0;
	tdf->tdf_name = malloc(strlen(file_name)+1);
	if(tdf->tdf_name) strcpy(tdf->tdf_name,file_name);
	
	//open file
	tdffile = fopen(file_name, "rb");
	if(!tdffile) return 0;

	//seek/read num textures
	fseek(tdffile, 4, SEEK_SET);
	fread(&tdf->ntextures, sizeof(int), 1, tdffile);
#ifndef BIGENDIAN
	tdf->ntextures = SwapInt(tdf->ntextures);
#endif
	//seek/read texture descriptors
	fseek(tdffile, 12, 0);
	deschead = malloc(tdf->ntextures * sizeof(TDDescHeader));
	if(deschead) {
		fread(deschead, sizeof(TDDescHeader), tdf->ntextures, tdffile);

		// read in texture and pal data
		for(c=0; c<tdf->ntextures; c++)
		{
#ifndef BIGENDIAN
			deschead[c].imghead = SwapInt(deschead[c].imghead);
			deschead[c].palhead = SwapInt(deschead[c].palhead);
#endif
			imghead = deschead[c].imghead;
			palhead = deschead[c].palhead;

			//if we've a palette read in all values.
			if(palhead) {
				fseek(tdffile,(long)palhead,SEEK_SET);
				palhead = malloc(sizeof(TDPalHeader));
				fread(palhead,1,sizeof(TDPalHeader),tdffile);
#ifndef BIGENDIAN
				palhead->fmt = SwapInt(palhead->fmt);
				palhead->nitems = SwapShort(palhead->nitems);
				palhead->data_offset = SwapInt(palhead->data_offset);
#endif
				deschead[c].palhead = palhead;
			}

			//now read in the image data.
			fseek(tdffile,(long)imghead,SEEK_SET);
			imghead = malloc(sizeof(TDImgHeader));
			fread(imghead,1,sizeof(TDImgHeader),tdffile);
#ifndef BIGENDIAN
			imghead->fmt = SwapInt(imghead->fmt);
			imghead->width = SwapShort(imghead->width);
			imghead->height = SwapShort(imghead->height);
			imghead->data_offset = SwapInt(imghead->data_offset);
			imghead->minfilter = SwapInt(imghead->minfilter);
			imghead->magfilter = SwapInt(imghead->magfilter);
			imghead->lodbias = (f32)SwapInt((int)imghead->lodbias);
			imghead->wraps = SwapInt(imghead->wraps);
			imghead->wrapt = SwapInt(imghead->wrapt);
#endif
			deschead[c].imghead = imghead;
		}
		tdf->texdesc = deschead;
	}
	fclose(tdffile);
	return 1;
}

u32 TDF_GetTextureFromFile(TDFile *tdf,u32 id,TDTexture **tex)
{
	u32 nRet,size;
	TDDescHeader *deschead;
	TDImage *image = NULL;
	TDPalette *palette = NULL;
	TDTexture *texture = NULL;
	FILE *tdffile = NULL;

	if(!tdf) return 0;
	if(!tex) return 0;
	if(!tdf->tdf_name) return 0;

	tdffile = fopen(tdf->tdf_name,"rb");
	if(!tdffile) return 0;

	nRet = 0;
	*tex = NULL;
	deschead = (TDDescHeader*)tdf->texdesc;
	if(deschead && deschead[id].imghead) {
		texture = malloc(sizeof(TDTexture));
		if(texture) {
			texture->minlod = deschead[id].imghead->minlod;
			texture->maxlod = deschead[id].imghead->maxlod;
			texture->minfilter = deschead[id].imghead->minfilter;
			texture->magfilter = deschead[id].imghead->magfilter;
			texture->lodbias = deschead[id].imghead->lodbias;
			texture->edgelod = deschead[id].imghead->edgelod;
			texture->wraps = deschead[id].imghead->wraps;
			texture->wrapt = deschead[id].imghead->wrapt;

			fseek(tdffile,(long)deschead[id].imghead->data_offset,SEEK_SET);
			image = malloc(sizeof(TDImage));
			if(image) {
				image->width = deschead[id].imghead->width;
				image->height = deschead[id].imghead->height;
				image->fmt = deschead[id].imghead->fmt;

				size = TDF_GetTextureSize(image->width,image->height,image->fmt);
				image->data = memalign(size,32);
				if(image->data) fread(image->data,1,size,tdffile);
			}

			if(deschead[id].palhead) {
				palette = malloc(sizeof(TDPalette));
				if(palette) {
					palette->fmt = deschead[id].palhead->fmt;
					palette->nitems = deschead[id].palhead->nitems;

					size = palette->nitems;
					fseek(tdffile,(long)deschead[id].palhead->data_offset,SEEK_SET);
					palette->data = memalign(size*sizeof(u16),32);
					if(palette->data) fread(palette->data,sizeof(u16),size,tdffile);
				}
			}

			texture->palette = palette;
			texture->image = image;
			if(image) nRet = 1;			//only if image header is present, then return 1
		}
	}
	fclose(tdffile);
	if(nRet) *tex = texture;

	return nRet;
}

u32 TDF_InitTextureFromFile(TDFile *tdf,u32 id,TDTexture **tex)
{
	u32 nRet;
	u8 biasclamp = GX_DISABLE;
	boolean bMipMap = false;
	TDTexture *texture = NULL;

	nRet = TDF_GetTextureFromFile(tdf,id,tex);
	if(nRet) {
		// sanity check to avoid the try to set a wrong texture format
		texture = *tex;
		switch(texture->image->fmt) {
			case GX_TF_CI4:
			case GX_TF_CI8:
			case GX_TF_CI14:
				TDF_ReleaseTexture(texture);
				*tex = NULL;
				return 0;
		}

		if(texture->maxlod>0) bMipMap = true;
		if(texture->lodbias>0) biasclamp = GX_ENABLE;
		GX_InitTexObj(&texture->tex,texture->image->data,texture->image->width,texture->image->height,texture->image->fmt,texture->wraps,texture->wrapt,bMipMap);
		
		if(bMipMap==true) GX_InitTexObjLOD(&texture->tex,texture->minfilter,texture->magfilter,texture->minlod,texture->maxlod,
											texture->lodbias,biasclamp,biasclamp,texture->edgelod);
	}
	return nRet;
}

u32 TDF_InitCITextureFromFile(TDFile *tdf,u32 id,u32 tlut_name,TDTexture **tex)
{
	u32 nRet;
	u32 biasclamp = GX_DISABLE;
	boolean bMipMap = false;
	TDTexture *texture = NULL;

	nRet = TDF_GetTextureFromFile(tdf,id,tex);
	if(nRet) {
		// sanity check to avoid the try to set a wrong texture format
		texture = *tex;
		switch(texture->image->fmt) {
			case GX_TF_I4:
			case GX_TF_I8:
			case GX_TF_IA4:
			case GX_TF_IA8:
			case GX_TF_RGB565:
			case GX_TF_RGB5A3:
			case GX_TF_RGBA8:
				TDF_ReleaseTexture(texture);
				*tex = NULL;
				return 0;
		}

		//if no palette is present we destroy our texture object and return immediately
		//CI textures need to have a palette associated.
		if(!texture->palette) {
			TDF_ReleaseTexture(texture);
			*tex = NULL;
			return 0;
		}
		if(texture->maxlod>0) bMipMap = true;
		if(texture->lodbias>0) biasclamp = GX_ENABLE;

		GX_InitTlutObj(&texture->tlut,texture->palette->data,texture->palette->fmt,texture->palette->nitems);
		GX_LoadTlut(&texture->tlut,tlut_name);
		
		GX_InitTexObjCI(&texture->tex,texture->image->data,texture->image->width,texture->image->height,texture->image->fmt,texture->wraps,texture->wrapt,bMipMap,tlut_name);
		if(bMipMap==true) GX_InitTexObjLOD(&texture->tex,texture->minfilter,texture->magfilter,texture->minlod,texture->maxlod,
											texture->lodbias,biasclamp,biasclamp,texture->edgelod);
	}
	return nRet;
}

void TDF_ReleaseTexture(TDTexture *tex)
{
	if(!tex) return;

	if(tex->palette) {
		if(tex->palette->data) free(tex->palette->data);
		free(tex->palette);
	}

	if(tex->image) {
		if(tex->image->data) free(tex->image->data);
		free(tex->image);
	}

	free(tex);
}

void TDF_CloseTDFile(TDFile *tdf)
{
	int i;
	TDDescHeader *deschead;

	if(!tdf) return;

	if(tdf->tdf_name) free(tdf->tdf_name);

	deschead = (TDDescHeader*)tdf->texdesc;
	if(!deschead) return;
	
	for(i=0;i<tdf->ntextures;i++) {
		if(deschead[i].imghead) free(deschead[i].imghead);
		if(deschead[i].palhead) free(deschead[i].palhead);
	}
	free(tdf->texdesc);
}
