#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "lwp.h"
#include "exi.h"
#include "card.h"

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef struct _card_block {
	CARDCallback CARDTxCallback;
} card_block;

static CardDirBlock dirblock[2] __attribute__ ((aligned (32)));
static CardFatBlock fatblock[2] __attribute__ ((aligned (32)));

static u8 sysarea[CARD_SYSAREA*CARD_SECTORSIZE] __attribute__ ((aligned (32)));

static u32 const _cardunlockdata[] =
{
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000021,0x02ff0021,
	0x13061203,0x12041305,0x009200ff,0x0088ffff,
	0x0089ffff,0x008affff,0x008bffff,0x8f0002bf,
	0x008816fc,0xdcd116fd,0x000016fb,0x000102bf,
	0x008e25ff,0x0380ff00,0x02940027,0x02bf008e,
	0x1fdf24ff,0x02400fff,0x00980400,0x009a0010,
	0x00990000,0x8e0002bf,0x009402bf,0x864402bf,
	0x008816fc,0xdcd116fd,0x000316fb,0x00018f00,
	0x02bf008e,0x0380cdd1,0x02940048,0x27ff0380,
	0x00010295,0x005a0380,0x00020295,0x8000029f,
	0x00480021,0x8e0002bf,0x008e25ff,0x02bf008e,
	0x25ff02bf,0x008e25ff,0x02bf008e,0x00c5ffff,
	0x03400fff,0x1c9f02bf,0x008e00c7,0xffff02bf,
	0x008e00c6,0xffff02bf,0x008e00c0,0xffff02bf,
	0x008e20ff,0x03400fff,0x1f5f02bf,0x008e21ff,
	0x02bf008e,0x23ff1205,0x1206029f,0x80b50021,
	0x27fc03c0,0x8000029d,0x008802df,0x27fe03c0,
	0x8000029c,0x008e02df,0x2ece2ccf,0x00f8ffcd,
	0x00f9ffc9,0x00faffcb,0x26c902c0,0x0004029d,
	0x009c02df,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000
};

static u32 cardinit[2] = {0,0};
static u32 crand_next = 0;
static u32 currslot,numsectors,currfat,currdir;

static lwpq_t wait_exi_queue;

static card_block cardmap[2];

static u32 __dounlock(u32);

extern unsigned long long gettime();
extern unsigned long gettick();

static void __card_checksum(u16 *buff,u32 len,u16 *cs1,u16 *cs2)
{
	u16 val;
	u32 i;

	*cs1 = 0;
	*cs2 = 0;
	len /= 2;

	for (i = 0; i < len; i++) {
		val = *cs1;
		*cs1 = buff[i] + val;

		val = *cs2;
		*cs2 = ~buff[i] + val;
	}

	if (*cs1 == 0xffff)
		*cs1 = 0;

	if (*cs2 == 0xffff)
		*cs2 = 0;
}

static u32 __card_check(u32 *fixed)
{
	u16 chksum1,chksum2;
	u32 i,bad;

	*fixed = 0;

	bad = 0;
	for(i=0;i<2;i++) {
		__card_checksum((u16*)&dirblock[i],0x1ffc,&chksum1,&chksum2);
		if(chksum1!=dirblock[i].chksum1 || chksum2!=dirblock[i].chksum2)
			bad |= 1<<i;
	}

	if(bad&3) {
		if(bad==3) return CARD_ERROR_CORRUPT;
		if(bad==1) {
			memcpy(&dirblock[0],&dirblock[1],sizeof(CardDirBlock));
			currdir = 0;
		} else {
			memcpy(&dirblock[1],&dirblock[0],sizeof(CardDirBlock));
			currdir = 1;
		}
		*fixed |= 1;
	} else {
		if(dirblock[0].num<dirblock[1].num) {
			memcpy(&dirblock[0],&dirblock[1],sizeof(CardDirBlock));
			currdir = 0;
		} else {
			memcpy(&dirblock[1],&dirblock[0],sizeof(CardDirBlock));
			currdir = 1;
		}
	}

	bad = 0;
	for(i=0;i<2;i++) {
		__card_checksum((u16*)&fatblock[i]+2,0x1ffc,&chksum1,&chksum2);
		if(chksum1!=fatblock[i].chksum1 || chksum2!=fatblock[i].chksum2)
			bad |= 1<<i;
	}

	if(bad&3) {
		if(bad==3) return CARD_ERROR_CORRUPT;
		if(bad==1) {
			memcpy(&fatblock[0],&fatblock[1],sizeof(CardFatBlock));
			currfat = 0;
		} else {
			memcpy(&fatblock[1],&fatblock[0],sizeof(CardFatBlock));
			currfat = 1;
		}
		*fixed |= 2;
	} else {
		if(fatblock[0].num<fatblock[1].num) {
			memcpy(&fatblock[0],&fatblock[1],sizeof(CardFatBlock));
			currfat = 0;
		} else {
			memcpy(&fatblock[1],&fatblock[0],sizeof(CardFatBlock));
			currfat = 1;
		}
	}
	return CARD_ERROR_NONE;
}

static u32 __card_getoffsets(CardFile *pFile,CardOffset *pOffset)
{
	u32 addr, bsize, isize;
	u32 i, format, needs_tlut = 0;

	if(!cardinit) return CARD_ERROR_INIT;
	if(pFile->filenum<0 || pFile->filenum>=CARD_MAXFILES) return CARD_ERROR_FATAL;

	addr = pFile->iconaddr;
	bsize = CARD_BANNER_W * CARD_BANNER_H;
	isize = CARD_ICON_W * CARD_ICON_H;

	// The banner is the first data at the icon address.
	if(pFile->bannerfmt&CARD_BANNER_CI) {
		// Color Index format = 8bit per pixel and 256x16bit palette.
		pOffset->banner = addr;
		pOffset->banner_tlut = addr + bsize;
		addr += bsize + 512;
	} else if(pFile->bannerfmt&CARD_BANNER_RGB) {
	    // RGB5/RGB4A3 = 16bits per pixel, no palette.
	    pOffset->banner = addr;
		pOffset->banner_tlut = 0xffff;
		addr += bsize * 2;
	} else {
		pOffset->banner = 0xffff;
		pOffset->banner_tlut = 0xffff;
	}

	for(i=0;i<CARD_MAXICONS;i++) {
	    // icon_fmt has 2 bits per icon to describe format of each one
		format = pFile->iconfmt>>(i * 2);

		if (format & CARD_ICON_CI) {
			pOffset->icons[i] = addr;
			addr += isize;
			needs_tlut = 1;
		} else if(format&CARD_ICON_RGB) {
			pOffset->icons[i] = addr;
			addr += isize * 2;
		} else
			pOffset->icons[i] = 0xffff;
	}

	if(needs_tlut) {
		pOffset->icons_tlut = addr;
		addr += 512;
	} else
		pOffset->icons_tlut = 0xffff;

	pOffset->data = addr;

	return CARD_ERROR_NONE;
}

static u32 __card_exi_unlock()
{
	LWP_WakeThread(wait_exi_queue);
	return 1;
}

static void __card_exi_wait(u32 nChn)
{
	u32 level,ret = 0;

	_CPU_ISR_Disable(level);
	do {
		if((ret=EXI_Lock(nChn,EXI_DEVICE_0,__card_exi_unlock))==1) break;
		LWP_SleepThread(wait_exi_queue);
	}while(ret==0);
	_CPU_ISR_Restore(level);
}

u16 CARD_RetrieveID(u32 nChn)
{
	u8 cbuf[2];

	cbuf[0] = 0x85; cbuf[1] = 0x00;

	__card_exi_wait(nChn);

	EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
	EXI_Imm(nChn,cbuf,2,EXI_WRITE,NULL);
	EXI_Sync(nChn);
	EXI_Imm(nChn,cbuf,2,EXI_READ,NULL);
	EXI_Sync(nChn);
	EXI_Deselect(nChn);
	EXI_Unlock(nChn);

	return ((u16*)cbuf)[0];
}

u8 CARD_ReadStatus(u32 nChn)
{
	u8 cbuf[2];

	cbuf[0] = 0x83; cbuf[1] = 0x00;

	__card_exi_wait(nChn);

	EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
	EXI_Imm(nChn,(void*)cbuf,2,EXI_WRITE,NULL);
	EXI_Sync(nChn);
	EXI_Imm(nChn,(void*)cbuf,1,EXI_READ,NULL);
	EXI_Sync(nChn);
	EXI_Deselect(nChn);
	EXI_Unlock(nChn);

	return cbuf[0];
}

void CARD_ClearStatus(u32 nChn)
{
	u8 cbuf[1];
	cbuf[0] = 0x89;

	__card_exi_wait(nChn);

	EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
	EXI_Imm(nChn,(void*)cbuf,1,EXI_WRITE,NULL);
	EXI_Sync(nChn);
	EXI_Deselect(nChn);
	EXI_Unlock(nChn);
}

void CARD_ChipErase(u32 nChn)
{
	u8 cbuf[3];

	cbuf[0] = 0xf4; cbuf[1] = 0x00; cbuf[2] = 0x00;

	__card_exi_wait(nChn);

	EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
	EXI_Imm(nChn,(void*)cbuf,3,EXI_WRITE,NULL);
	EXI_Sync(nChn);
	EXI_Deselect(nChn);
	EXI_Unlock(nChn);
}

void CARD_SectorErase(u32 nChn,u32 nSector)
{
	u8 cbuf[3];

	while(!(CARD_ReadStatus(nChn)&1));

	cbuf[0] = 0xf1; cbuf[1] = (nSector>>17)&0x7f; cbuf[2] = (nSector>>9)&0xff;
	__card_exi_wait(nChn);

	EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
	EXI_Imm(nChn,(void*)cbuf,3,EXI_WRITE,NULL);
	EXI_Sync(nChn);
	EXI_Deselect(nChn);
	EXI_Unlock(nChn);

	while(CARD_ReadStatus(nChn)&0x8000);
}

void CARD_SectorProgram(u32 nChn,u32 nAddress,u8 *buff,u32 size)
{
	u32 i;
	u8 cbuf[5];

	i=0;
	while(i<size) {
		while(!(CARD_ReadStatus(nChn)&1));

		cbuf[0] = 0xf2; cbuf[1] = (nAddress>>17)&0x3f; cbuf[2] = (nAddress>>9)&0xff;
		cbuf[3] = (nAddress>>7)&3; cbuf[4] = nAddress&0x7f;

		__card_exi_wait(nChn);

		EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
		EXI_Imm(nChn,(void*)cbuf,5,EXI_WRITE,NULL);
		EXI_Sync(nChn);
		EXI_Dma(nChn,(void*)buff,CARD_WRITESIZE,EXI_WRITE,NULL);
		EXI_Deselect(nChn);
		EXI_Unlock(nChn);

		while(CARD_ReadStatus(nChn)&0x8000);

		i += CARD_WRITESIZE;
		nAddress += CARD_WRITESIZE;
		buff += CARD_WRITESIZE;
	}
}

void CARD_ReadArray(u32 nChn,u32 nAddress,u8 *buff,u32 size)
{
	u32 i;
	u8 cbuf[5];

	i=0;
	while(i<size) {
		while(!(CARD_ReadStatus(nChn)&1));

		cbuf[0] = 0x52; cbuf[1] = (nAddress>>17)&0x3f; cbuf[2] = (nAddress>>9)&0xff;
		cbuf[3] = (nAddress>>7)&3; cbuf[4] = nAddress&0x7f;

		__card_exi_wait(nChn);

		EXI_Select(nChn,EXI_DEVICE_0,EXI_SPEED16MHZ);
		EXI_ImmEx(nChn,cbuf,5,EXI_WRITE);

		cbuf[0] = 0; cbuf[1] = 0; cbuf[2] = 0; cbuf[3] = 0;
		EXI_Imm(nChn,cbuf,4,EXI_WRITE,NULL);
		EXI_Sync(nChn);

		EXI_Dma(nChn,buff,CARD_READSIZE,EXI_READ,NULL);
		EXI_Sync(nChn);
		EXI_Deselect(nChn);
		EXI_Unlock(nChn);

		i += CARD_READSIZE;
		nAddress += CARD_READSIZE;
		buff += CARD_READSIZE;
	}
}

u32 CARD_IsPresent(u32 nChn)
{
	u32 id = 0;

	EXI_GetID(nChn,EXI_DEVICE_0,&id);
	if((id&0xffff0000) || (id&0x03))
		return 0;

	return id;
}

void CARD_UpdateDir(u32 nChn)
{
	dirblock[currdir].num++;

	__card_checksum((u16*)&dirblock[currdir],0x1ffc,&dirblock[currdir].chksum1,&dirblock[currdir].chksum2);
	DCFlushRange((void*)&dirblock[currdir],sizeof(CardDirBlock));
	CARD_SectorErase(nChn,currdir*0x2000+0x2000);
	CARD_SectorProgram(nChn,currdir*0x2000+0x2000,(u8*)&dirblock[currdir],0x2000);

	currdir ^= 1;
	memcpy(&dirblock[currdir],&dirblock[currdir^1],sizeof(CardDirBlock));
}

void CARD_UpdateFat(u32 nChn)
{
	fatblock[currfat].num++;

	__card_checksum((u16*)&dirblock[currfat]+2,0x1ffc,&fatblock[currfat].chksum1,&fatblock[currfat].chksum2);
	DCFlushRange((void*)&fatblock[currfat],sizeof(CardFatBlock));
	CARD_SectorErase(nChn,currfat*0x2000+0x6000);
	CARD_SectorProgram(nChn,currfat*0x2000+0x6000,(u8*)&fatblock[currfat],0x2000);

	currfat ^= 1;
	memcpy(&fatblock[currfat],&fatblock[currfat^1],sizeof(CardFatBlock));
}

u32 CARD_Init(u32 nChn)
{
	u32 id,ret,fix;

	if(nChn<0 || nChn>=2) return CARD_ERROR_FATAL;
	
	cardinit[nChn] = 0;
	id = CARD_IsPresent(nChn);
	if(!id) return CARD_ERROR_NOCARD;

	LWP_InitQueue(&wait_exi_queue);

	__dounlock(nChn);
	
	DCInvalidateRange(sysarea,CARD_SYSAREA*CARD_SECTORSIZE);
	CARD_ReadArray(nChn,0,sysarea,CARD_SYSAREA*CARD_SECTORSIZE);
	DCFlushRange(sysarea,CARD_SYSAREA*CARD_SECTORSIZE);

	memcpy(&dirblock[0],sysarea+0x2000,sizeof(CardDirBlock));
	memcpy(&dirblock[1],sysarea+0x4000,sizeof(CardDirBlock));
	DCFlushRange(dirblock,sizeof(CardDirBlock)*2);

	memcpy(&fatblock[0],sysarea+0x6000,sizeof(CardFatBlock));
	memcpy(&fatblock[1],sysarea+0x8000,sizeof(CardFatBlock));
	DCFlushRange(fatblock,sizeof(CardFatBlock)*2);

	ret = __card_check(&fix);
	if(ret!=CARD_ERROR_NONE) return ret;

	printf("fix = %d\n",fix);
	if(fix&1)
		CARD_UpdateDir(nChn);

	if(fix&2)
		CARD_UpdateFat(nChn);

	cardinit[nChn] = 1;
	currslot = nChn;
	numsectors = id<<4;

	return CARD_ERROR_NONE;
}

u32 CARD_CreateFile(CardFile *pFile,CardOffset *pOffset)
{
	u16 block,prevblock = 0,currblock = 0;
	u32 i,needblocks,count,filenum = -1;
	CardDirEntry *pEntries = NULL;

	if(!cardinit) return CARD_ERROR_INIT;

	pFile->filenum = -1;
	if(strlen(pFile->filename)>CARD_FILENAME
		|| strlen(pFile->gamecode)!=4
		|| strlen(pFile->company)!=2) return CARD_ERROR_FATAL;

	needblocks = (((pFile->size+(CARD_SECTORSIZE-1))&~(CARD_SECTORSIZE-1))/CARD_SECTORSIZE);
	if(!pFile->size) return CARD_ERROR_FATAL;
	if((fatblock[currfat].freeblocks * CARD_SECTORSIZE)<needblocks) return CARD_ERROR_NOSPACE;

	// Find a free entry and check to see if the file already exists.
	pEntries = dirblock[currdir].entries;
	for(i=0;i<CARD_MAXFILES;i++) {
		if(pEntries[i].gamecode[0]==0xff) {
	      if(filenum==-1) filenum = i;
		} else {
	      if (memcmp(pEntries[i].gamecode,pFile->gamecode,4)==0
			  && memcmp(pEntries[i].company,pFile->company,2)==0
			  && memcmp(pEntries[i].filename,pFile->filename,CARD_FILENAME)==0) return CARD_ERROR_EXISTS;
		}
	}

	if(filenum==-1) return CARD_ERROR_NOENTRY;

	// Add which blocks this file will take up into the FAT
	block = 0xffff;
	currblock = fatblock[currfat].lastalloc;
	i = needblocks;
	count = 0;

	while (1) {
		if(i==0) {
			// Done allocating blocks
			fatblock[currfat].freeblocks -= needblocks;
			fatblock[currfat].lastalloc = currblock;
			CARD_UpdateFat(currslot);
			break;
		}

		/*
		  Since testing free space has already been done, if all the blocks
		  the file takes up cannot be entered into the FAT, something is
		  wrong.
		*/
		count++;
		if(count>=(numsectors-CARD_SYSAREA)) return CARD_ERROR_CORRUPT;

		currblock++;
	    if(currblock<5 || currblock>=numsectors) currblock = 5;
		if(fatblock[currfat].fat[currblock-5]==0) {
			if(block!=0xffff)
				fatblock[currfat].fat[prevblock-5] = currblock;
			else
				block = currblock;

			fatblock[currfat].fat[currblock-5] = 0xffff;
			prevblock = currblock;
			i--;
		}
	}

	// Finally, copy the necessary info to the file entry in the dir block.
	pEntries = &dirblock[currdir].entries[filenum];
	memcpy(pEntries->filename,pFile->filename,CARD_FILENAME);
	memcpy(pEntries->gamecode,pFile->gamecode,4);
	memcpy(pEntries->company,pFile->company,2);
	pEntries->pad_ff = 0xff;
	pEntries->bannerfmt = pFile->bannerfmt;

	pEntries->lastmodified = gettime();

	pEntries->iconaddr = pFile->iconaddr;
	pEntries->iconfmt = pFile->iconfmt;
	pEntries->iconspeed = pFile->iconspeed;
	pEntries->attribute = pFile->attribute;

  if (pEntries->attribute == 0)
    pEntries->attribute = CARD_ATTRIB_PUBLIC;

  pEntries->pad_00 = 0x00;
  pEntries->block = block;
  pEntries->length = needblocks;
  pEntries->pad_ffff = 0xffff;
  pEntries->commentaddr = pFile->commentaddr;
  pFile->filenum = filenum;
  pFile->lastmodified = pEntries->lastmodified;

  CARD_UpdateDir(currslot);
  __card_getoffsets(pFile, pOffset);

  return CARD_ERROR_NONE;
}

u32 CARD_OpenFile(CardFile *pFile,CardOffset *pOffset)
{
	u32 i;
	CardDirEntry *pEntries = NULL;

	if(!cardinit) return CARD_ERROR_INIT;

	pFile->filenum = -1;
	if(strlen(pFile->filename)>CARD_FILENAME
		|| strlen(pFile->company)!=2
		|| strlen(pFile->gamecode)!=4) return CARD_ERROR_FATAL;

	pEntries = dirblock[currdir].entries;
	for(i=0;i<CARD_MAXFILES;i++) {
		if(pEntries[i].gamecode[0]!=0xff)  {
			if(memcmp(pEntries[i].filename,pFile->filename,CARD_FILENAME)==0
				&& memcmp(pEntries[i].company,pFile->company,2)==0
				&& memcmp(pEntries[i].gamecode,pFile->gamecode,4)==0) break;
		}
	}

	if(i>=CARD_MAXFILES) return CARD_ERROR_NOENTRY;

	memcpy(pFile->filename,pEntries[i].filename,CARD_FILENAME);
	memcpy(pFile->company,pEntries[i].company,2);
	memcpy(pFile->gamecode,pEntries[i].gamecode,4);
	pFile->filename[CARD_FILENAME] = 0;
	pFile->company[2] = 0;
	pFile->gamecode[4] = 0;

	pFile->bannerfmt = pEntries[i].bannerfmt;
	pFile->lastmodified = pEntries[i].lastmodified;
	pFile->iconaddr = pEntries[i].iconaddr;
	pFile->iconfmt = pEntries[i].iconfmt;
	pFile->iconspeed = pEntries[i].iconspeed;
	pFile->attribute = pEntries[i].attribute;
	pFile->commentaddr = pEntries[i].commentaddr;
	pFile->size = pEntries[i].length*CARD_SECTORSIZE;
	pFile->filenum = i;

	return CARD_ERROR_NONE;
}

u32 CARD_OpenFileByNum(CardFile *pFile,CardOffset *pOffset,u32 nFilenum)
{
	CardDirEntry *pEntry = NULL;

	if(!cardinit) return CARD_ERROR_INIT;
	if(nFilenum<0 || nFilenum>=CARD_MAXFILES) return CARD_ERROR_FATAL;

	pEntry = &dirblock[currdir].entries[nFilenum];
	memcpy(pFile->filename,pEntry->filename,CARD_FILENAME);
	memcpy(pFile->company,pEntry->company,2);
	memcpy(pFile->gamecode,pEntry->gamecode,4);
	pFile->filename[CARD_FILENAME] = 0;
	pFile->company[2] = 0;
	pFile->gamecode[4] = 0;

	return CARD_OpenFile(pFile,pOffset);
}

u32 CARD_ReadFile(CardFile *pFile,void *pBuf)
{
	u16 i,block;
	CardDirEntry *pEntry = NULL;

	if(!cardinit) return CARD_ERROR_INIT;
	if(pFile->filenum<0 || pFile->filenum>=CARD_MAXFILES) return CARD_ERROR_FATAL;

	pEntry = &dirblock[currdir].entries[pFile->filenum];
	block = pEntry->block;

	DCInvalidateRange(pBuf,pFile->size);
	for(i=0;i<pEntry->length;i++) {
		if(block<CARD_SYSAREA || block>=numsectors) return CARD_ERROR_CORRUPT;

		CARD_ReadArray(currslot,block*CARD_SECTORSIZE,pBuf+(i*CARD_SECTORSIZE),CARD_SECTORSIZE);
		block  = fatblock[currfat].fat[block-5];
	}
	DCFlushRange(pBuf,pFile->size);
	return CARD_ERROR_NONE;
}

u32 CARD_WriteFile(CardFile *pFile,void *pBuf)
{
	u32 i,realsize;
	u16 block;
	void *data = NULL;
	CardDirEntry *pEntry = NULL;

	if(!cardinit) return CARD_ERROR_INIT;
	if(pFile->filenum<0 || pFile->filenum>=CARD_MAXFILES) return CARD_ERROR_FATAL;

	pEntry = &dirblock[currdir].entries[pFile->filenum];
	block = pEntry->block;

	//round up to next sector size, and allocate temporary buffer (address aligned to 32b).
	realsize = (pFile->size+(CARD_SECTORSIZE-1))&~(CARD_SECTORSIZE-1);
	data = memalign(realsize,32);
	if(!data) return CARD_ERROR_FATAL;
	memcpy(data,pBuf,pFile->size);
	memset(data+pFile->size,0,realsize-pFile->size);
	DCFlushRange(data,realsize);

	for (i=0;i<pEntry->length;i++) {
		if (block<CARD_SYSAREA || block>=numsectors) return CARD_ERROR_CORRUPT;

		CARD_SectorErase(currslot,block*CARD_SECTORSIZE);
		CARD_SectorProgram(currslot,block*CARD_SECTORSIZE,data+(i*CARD_SECTORSIZE),CARD_SECTORSIZE);

		// Look up the next block the files takes up in the fat
		block = fatblock[currfat].fat[block-5];
	}

	// Update the file's dir pEntry.
	memcpy(pEntry->filename,pFile->filename,CARD_FILENAME);
	memcpy(pEntry->gamecode,pFile->gamecode,4);
	memcpy(pEntry->company,pFile->company,2);
	pEntry->bannerfmt = pFile->bannerfmt;

	pEntry->lastmodified = gettime();
	//pEntry->lastmodified = currtime.u/pEntry->lastmodified;

	pEntry->iconaddr = pFile->iconaddr;
	pEntry->iconfmt = pFile->iconfmt;
	pEntry->iconspeed = pFile->iconspeed;
	pEntry->attribute = pFile->attribute;
	pEntry->commentaddr = pFile->commentaddr;
	pFile->lastmodified = pEntry->lastmodified;

	CARD_UpdateDir(currslot);

	free(data);
	return CARD_ERROR_NONE;
}

u32 CARD_DeleteFile(CardFile *pFile)
{
	u16 block, prevblock;

	if(!cardinit) return CARD_ERROR_INIT;
	if(pFile->filenum<0 || pFile->filenum>=CARD_MAXFILES) return CARD_ERROR_FATAL;

	// Get the file's initial block
	block = dirblock[currdir].entries[pFile->filenum].block;

	while (1) {
		if (block==0xffff) break;
		if (block<CARD_SYSAREA || block>=numsectors) return CARD_ERROR_CORRUPT;

		// Get the file's next block and clear the previous one from the fat
		prevblock = block;
		block = fatblock[currfat].fat[prevblock-5];
		fatblock[currfat].fat[prevblock-5] = 0;
		fatblock[currfat].freeblocks++;
	}

	CARD_UpdateFat(currslot);
	memset(&dirblock[currdir].entries[pFile->filenum],0xff,sizeof(CardDirEntry));
	CARD_UpdateDir(currslot);
	pFile->filenum = -1;

	return CARD_ERROR_NONE;
}

u32 CARD_FileAddress(CardFile *pFile)
{
	CardDirEntry *pEntry = NULL;

	if(!cardinit) return -1;
	if(pFile->filenum<0 || pFile->filenum>=CARD_MAXFILES) return -1;

	pEntry = &dirblock[currdir].entries[pFile->filenum];
	return (pEntry->block*CARD_SECTORSIZE);
}

static u32 __CARD_TxHandler(u32 nChn,void *pCtx)
{
	u32 ret = 0;
	CARDCallback txcb = NULL;
	card_block *card = &cardmap[nChn];

	EXI_Deselect(nChn);
	EXI_Unlock(nChn);
	
	txcb = card->CARDTxCallback;
	card->CARDTxCallback = NULL;
	if(!txcb) return 1;
	
	txcb(nChn,ret);

	return 1;
}

static __inline__ void __card_srand(u32 val)
{
	crand_next = val;	
}

static __inline__ u32 __card_rand()
{
	crand_next = (crand_next*0x41C64E6D)+12345;
	return _SHIFTR(crand_next,16,15);
}

static u32 __card_initval()
{
	u32 ticks = gettick();
	
	__card_srand(ticks);
	return __card_rand();
}

static u32 __card_dummylen()
{
	u32 ticks = gettick();
	u32 val = 0,cnt = 0,shift = 1;

	__card_srand(ticks);
	val = __card_rand();
	val = (val&0x1f)+1;
	
	do {
		ticks = gettick();
		val = ticks<<shift;
		shift++;
		if(shift>16) shift = 1;
		__card_srand(val);
		val = __card_rand();
		val = (val&0x1f)+1;
		cnt++;
	}while(val<4 && cnt<10);
	if(val<4) val = 4;

	return val;

}
static u32 exnor_1st(u32 a,u32 b)
{
	u32 c,d,e,f,r1,r2,r3;

	c = 0;
	while(c<b) {
		d = a>>23;
		e = a>>15;
		f = a>>7;
		r1 = a^f;
		r1 = e^r1;
		r2 = (d^~r1);
		d = a>>1;
		r3 = (r2<<30)&0x40000000;
		a = d|r3;
		c++;
	};
	return a;
}

static u32 exnor(u32 a,u32 b)
{
	u32 c,d,e,f,r1,r2,r3;

	c = 0;
	while(c<b) {
		d = a<<23;
		e = a<<15;
		f = a<<7;
		r1 = a^f;
		r1 = e^r1;
		r2 = (d^~r1);
		d = a<<1;
		r3 = (r2>>31)&1;
		a = d|r3;
		c++;
	};
	return a;
}

static u32 ReadArrayUnlock(u32 chn,u32 address,void *buffer,u32 len,u32 flag)
{
	u8 regbuf[5];

	__card_exi_wait(chn);

	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) return -3;
	
	address &= 0x0C;
	memset(regbuf,0,5);

	regbuf[0] = 0x52;
	if(!flag) {
		regbuf[1] = ((address&0x60000000)>>29)&0xff;
		regbuf[2] = ((address&0x1FE00000)>>20)&0xff;
		regbuf[3] = ((address&0x00180000)>>19)&0xff;
		regbuf[4] = ((address&0x0007F000)>>12)&0xff;
	} else {
		regbuf[1] = (address>>24)&0xff;
		regbuf[2] = ((address&0x00FF0000)>>16)&0xff;
	}
	
	EXI_ImmEx(chn,regbuf,5,EXI_WRITE);
	EXI_ImmEx(chn,sysarea+512,4,EXI_WRITE);
	EXI_ImmEx(chn,buffer,len,EXI_READ);
	
	EXI_Deselect(chn);
	EXI_Unlock(chn);

	return 0;
}

static void InitCallback(void *task)
{
}

static void DoneCallback(void *task)
{
}

static u32 __dounlock(u32 nChn)
{
	u32 array_addr,len,val0 = 0x7FEC8000;
	u8 buffer[64];

	array_addr = (val0|__card_initval())&0xFFFFF000;
	len = __card_dummylen();
	
	printf("array_addr = %08x, len = %d\n",array_addr,len);
	if(ReadArrayUnlock(nChn,array_addr,buffer,len,0)!=0) return -3;

	exnor_1st(array_addr,(len<<3)+1);

	

	return 1;
}
