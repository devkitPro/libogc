#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tdf.h"

int LoadTDFHeader(TDF* tdf, const char* file_name)
{
	int c;
	TexHeader *thead;
	TexPalHeader *phead;

	//open file
	FILE *tdffile = fopen(file_name, "rb");
	if(!tdffile) return 0;

	//seek/read num textures
	fseek(tdffile, 4, SEEK_SET);
	fread(&tdf->ntextures, sizeof(int), 1, tdffile);

	//seek/read texture descriptors
	fseek(tdffile, 12, 0);
	tdf->texdesc = malloc(tdf->ntextures * sizeof(TexDesc));
	fread(tdf->texdesc, sizeof(TexDesc), tdf->ntextures, tdffile);
	// read in texture and pal data
	for(c=0; c<tdf->ntextures; c++)
	{
		thead = tdf->texdesc[c].texhead;
		phead = tdf->texdesc[c].palhead;

		//if we've a palette read in all values.
		if(phead) {
			fseek(tdffile,(long)phead,SEEK_SET);
			phead = malloc(sizeof(TexPalHeader));
			fread(phead,1,sizeof(TexPalHeader),tdffile);

			tdf->texdesc[c].palhead = phead;
		}

		//now read in the image data.
		fseek(tdffile,(long)thead,SEEK_SET);
		thead = malloc(sizeof(TexHeader));
		fread(thead,1,sizeof(TexHeader),tdffile);

		tdf->texdesc[c].texhead = thead;
	}
	fclose(tdffile);

	return 1;
}

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
