#ifndef __FAT_H__
#define __FAT_H__

#include <gctypes.h>
#include "lwp_queue.h"

#define swap_u16(x)				((u16)(((x<<8)&0xff00)|((x>>8)&0x00ff)))
#define swap_u32(x)				((u16)(((x>>24)&0xff)|((x>>8)&0xff00)|((x<<8)&0xff0000)|((x<<24)&0xff000000)))

#define CF_LE_W(v)				swap_u16((u16)v)
#define CF_LE_L(v)				swap_u32((u32)v)
#define CT_LE_W(v)				swap_u16((u16)v)
#define CT_LE_L(v)				swap_u32((u32)v)

#ifndef MIN
#define MIN(a, b)				(((a) < (b)) ? (a) : (b))
#endif

#define FAT_DEVCHANNEL_0		0
#define FAT_DEVCHANNEL_1		1
#define FAT_DEVCHANNEL_2		2

#define FAT_HASH_SIZE			2
#define FAT_HASH_MODULE			FAT_HASH_SIZE


#define FAT_SECTOR512_SIZE		512		/* sector size (bytes) */
#define FAT_SECTOR512_BITS		9		/* log2(SECTOR_SIZE) */

/* maximum + 1 number of clusters for FAT12 */
#define FAT_FAT12_MAX_CLN       4085 

/* maximum + 1 number of clusters for FAT16 */
#define FAT_FAT16_MAX_CLN       65525 

#define FAT_FAT12               0x01
#define FAT_FAT16               0x02
#define FAT_FAT32               0x04
 
#define FAT_UNDEFINED_VALUE     (u32)0xFFFFFFFF 

#define FAT_FAT12_EOC           0x0FF8
#define FAT_FAT16_EOC           0xFFF8
#define FAT_FAT32_EOC           (u32)0x0FFFFFF8

#define FAT_FAT12_FREE          0x0000
#define FAT_FAT16_FREE          0x0000
#define FAT_FAT32_FREE          0x00000000

#define FAT_GENFAT_EOC          (u32)0xFFFFFFFF
#define FAT_GENFAT_FREE         (u32)0x00000000

#define FAT_FAT12_SHIFT         0x04

#define FAT_FAT12_MASK          0x00000FFF
#define FAT_FAT16_MASK          0x0000FFFF
#define FAT_FAT32_MASK          (u32)0x0FFFFFFF

#define FAT_MAX_BPB_SIZE        90

/* size of useful information in FSInfo sector */
#define FAT_USEFUL_INFO_SIZE    12

#define FAT_VAL8(x, ofs)        (u8)(*((u8*)(x)+(ofs)))
 
#define FAT_VAL16(x, ofs)												 \
							    (u16)((*((u8*)(x)+(ofs))) |				 \
							    ((*((u8*)(x)+(ofs)+1)) << 8))

#define FAT_VAL32(x, ofs)                                                 \
							    (u32)((u32)(*((u8*)(x)+(ofs)))|			  \
							    ((u32)(*((u8*)(x)+(ofs)+1))<<8)|		  \
							    ((u32)(*((u8*)(x)+(ofs)+2))<<16)|		  \
							    ((u32)(*((u8*)(x)+(ofs)+3))<<24))
                    
/* macros to access boot sector fields */
#define FAT_BR_BYTES_PER_SECTOR(x)       FAT_VAL16(x, 11)
#define FAT_BR_SECTORS_PER_CLUSTER(x)    FAT_VAL8(x, 13) 
#define FAT_BR_RESERVED_SECTORS_NUM(x)   FAT_VAL16(x, 14)
#define FAT_BR_FAT_NUM(x)                FAT_VAL8(x, 16)
#define FAT_BR_FILES_PER_ROOT_DIR(x)     FAT_VAL16(x, 17)
#define FAT_BR_TOTAL_SECTORS_NUM16(x)    FAT_VAL16(x, 19)
#define FAT_BR_MEDIA(x)                  FAT_VAL8(x, 21) 
#define FAT_BR_SECTORS_PER_FAT(x)        FAT_VAL16(x, 22)
#define FAT_BR_TOTAL_SECTORS_NUM32(x)    FAT_VAL32(x, 32)
#define FAT_BR_SECTORS_PER_FAT32(x)      FAT_VAL32(x, 36)
#define FAT_BR_EXT_FLAGS(x)              FAT_VAL16(x, 40)
#define FAT_BR_FAT32_ROOT_CLUSTER(x)     FAT_VAL32(x, 44)
#define FAT_BR_FAT32_FS_INFO_SECTOR(x)   FAT_VAL16(x, 48)
#define FAT_FSINFO_LEAD_SIGNATURE(x)     FAT_VAL32(x, 0)
/* 
 * I read FSInfo sector from offset 484 to access the information, so offsets 
 * of these fields a relative
 */
#define FAT_FSINFO_FREE_CLUSTER_COUNT(x) FAT_VAL32(x, 4)
#define FAT_FSINFO_NEXT_FREE_CLUSTER(x)  FAT_VAL32(x, 8)

#define FAT_FSINFO_FREE_CLUSTER_COUNT_OFFSET 488

#define FAT_FSINFO_NEXT_FREE_CLUSTER_OFFSET  492

#define FAT_RSRVD_CLN                        0x02  

#define FAT_FSINFO_LEAD_SIGNATURE_VALUE      0x41615252

#define FAT_FSI_LEADSIG_SIZE                 0x04

#define FAT_FSI_INFO                         484

#define MS_BYTES_PER_CLUSTER_LIMIT           0x8000     /* 32K */

#define FAT_BR_EXT_FLAGS_MIRROR              0x0080

#define FAT_BR_EXT_FLAGS_FAT_NUM             0x000F


#define FAT_DIRENTRY_SIZE					 32 
 
#define FAT_DIRENTRIES_PER_SEC512			 16

#define FAT_FAT_OFFSET(fat_type, cln)														\
											 ((fat_type)&FAT_FAT12?((cln)+((cln)>>1)) :		\
											 (fat_type)&FAT_FAT16?((cln)<<1)          :		\
											 ((cln)<<2))

#define FAT_CLUSTER_IS_ODD(n)				 ((n)&0x0001)

#define FAT12_SHIFT							 0x4    /* half of a byte */

/* initial size of array of unique ino */
#define FAT_UINO_POOL_INIT_SIZE				 0x100

/* cache support */
#define FAT_CACHE_EMPTY						 0x0
#define FAT_CACHE_ACTUAL					 0x1

#define FAT_OP_TYPE_READ					 0x1
#define FAT_OP_TYPE_GET						 0x2


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _fs_vol {
    u16   bps;            /* bytes per sector */
    u8    sec_log2;       /* log2 of bps */
    u8    sec_mul;        /* log2 of 512bts sectors number per sector */
    u8    spc;            /* sectors per cluster */
    u8    spc_log2;       /* log2 of spc */
    u16   bpc;            /* bytes per cluster */
    u8    bpc_log2;       /* log2 of bytes per cluster */
    u8    fats;           /* number of FATs */
    u8    type;           /* FAT type */
    u32   mask;
    u32   eoc_val;
    u16   fat_loc;        /* FAT start */
    u32   fat_length;     /* sectors per FAT */
    u32   rdir_loc;       /* root directory start */
    u16   rdir_entrs;     /* files per root directory */
    u32   rdir_secs;      /* sectors per root directory */
    u32   rdir_size;      /* root directory size in bytes */
    u32   tot_secs;       /* total count of sectors */
    u32   data_fsec;      /* first data sector */
    u32   data_cls;       /* count of data clusters */
    u32   rdir_cl;        /* first cluster of the root directory */
    u16   info_sec;       /* FSInfo Sector Structure location */
    u32   free_cls;       /* last known free clusters count */
    u32   next_cl;        /* next free cluster number */
    u8    mirror;         /* mirroring enabla/disable */
    u32   afat_loc;       /* active FAT location */
    u8    afat;           /* the number of active FAT */
    //dev_t        dev;            /* device ID */
    //disk_device *dd;             /* disk device (see libblock) */
    void        *private_data;   /* reserved */
} fat_vol;

typedef struct _fat_cache {
	u32		blk_num;
	boolean	modified;
	u8		state;
	
} fat_cache;

typedef struct _fat_fs_info {
	fat_vol		vol;
	lwp_queue	*vhash;
	lwp_queue	*rhash;
	char		*uino;
	u32			idx;
	u32			uino_pool_size;
	u32			uino_base;
	
} fat_fsinfo;

s32 fat_init_volume(s32 chn);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
