#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tdf.h"

// texture header
typedef struct _tdimgheader {
	u16 height;
	u16 width;
	u32 fmt;
	void *data;
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
	void *data;
} TDPalHeader;

// texture descriptor
typedef struct _tddesc {
	TDImgHeader *imghead;
	TDPalHeader *palhead;
} TDDescHeader;

static u32 TDF_GetTextureSize(u32 width,u32 height,u32 fmt)
{
	return 0;
}

u32 TDF_LoadHeader(TDFile* tdf, const char* file_name)
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

	//seek/read texture descriptors
	fseek(tdffile, 12, 0);
	deschead = malloc(tdf->ntextures * sizeof(TDDescHeader));
	if(deschead) {
		fread(deschead, sizeof(TDDescHeader), tdf->ntextures, tdffile);

		// read in texture and pal data
		for(c=0; c<tdf->ntextures; c++)
		{
			imghead = deschead[c].imghead;
			palhead = deschead[c].palhead;

			//if we've a palette read in all values.
			if(palhead) {
				fseek(tdffile,(long)palhead,SEEK_SET);
				palhead = malloc(sizeof(TDPalHeader));
				fread(palhead,1,sizeof(TDPalHeader),tdffile);

				deschead[c].palhead = palhead;
			}

			//now read in the image data.
			fseek(tdffile,(long)imghead,SEEK_SET);
			imghead = malloc(sizeof(TDImgHeader));
			fread(imghead,1,sizeof(TDImgHeader),tdffile);

			deschead[c].imghead = imghead;
		}
		tdf->texdesc = deschead;
	}
	fclose(tdffile);
	return 1;
}

u32 TDF_LoadTexture(TDFile *tdf,u32 id,TDTexture **tex)
{
	u32 nRet,size;
	TDDescHeader *deschead;
	TDImage *image = NULL;
	TDPalette *palette = NULL;
	TDTexture *texture = NULL;
	FILE *tdffile = NULL;

	if(!tdf) return 0;
	if(!tdf->tdf_name) return 0;

	tdffile = fopen(tdf->tdf_name,"rb");
	if(!tdffile) return 0;

	nRet = 0;
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

			if(deschead[id].imghead->data) {
				fseek(tdffile,(long)deschead[id].imghead->data,SEEK_SET);
				image = malloc(sizeof(TDImage));
				if(image) {
					image->width = deschead[id].imghead->width;
					image->height = deschead[id].imghead->height;
					image->fmt = deschead[id].imghead->fmt;

					size = TDF_GetTextureSize(image->fmt,image->width,image->height);
					image->data = malloc(size);
					if(image->data) fread(image->data,1,size,tdffile);

					texture->image = image;
				}
			}

			if(deschead[id].palhead) {
			}
		}
	}
	fclose(tdffile);
	if(tex) *tex = texture;

	return nRet;
}
/*
void GetTexFromTDF( TDF* tdf, GXTexObj *tex, u32 id)
{
	u8 mipmap = false;
	// get texture header
	TexHeader *thead = tdf.texdesc[id].texhead;
	if(head.maxlod>0) mipmap = true;
	// init texture and texture lod
	GX_InitTexObj(tex, thead.data, thead.width, thead.height, thead.fmt,
					thead.wraps, thead.wrapt, mipmap);
	GX_InitTexObjLOD( tex, thead.minfilter, thead.magfilter, (f32)thead.minlod, (f32)thead.maxlod,
						thead.lodbias, GX_FALSE, GX_FALSE, thead.edgelod);
}

void GetCITexFromTDF( TDF* tdf, GXTexObj *tex, GXTlutObj *tlo, u32 tluts, u32 id);
{
	u8 mipmap = false;
	// get texture and palette header
	TexHeader *thead = tdf.texdesc[id].texhead;
	TexPalHeader *phead = tdf.texdesc[id].palhead
	if(head.maxlod>0) mipmap = true;

	// init texture, load and init tlut, init texture lod
	GX_InitTexObjCI( tex,
		    thead.data, thead.width, thead.height, thead.fmt, thead.wraps, thead.wrapt, mipmap, tluts);
	GX_InitTlutObj(tlo, phead.data, phead.fmt, phead.nitems );
	GX_LoadTlut(tex, tluts );
	GX_InitTexObjLOD( tex, thead.minfilter, thead.magfilter, (f32)thead.minlod, (f32)thead.maxlod,
						thead.lodbias, GX_FALSE, GX_FALSE, thead.edgelod);
}
*/
