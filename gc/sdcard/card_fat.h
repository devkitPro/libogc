#ifndef __CARD_FAT_H__
#define __CARD_FAT_H__

#include <gctypes.h>

#define FS_UNKNOWN                      0
#define FS_FAT12                        1
#define FS_FAT16                        2
#define FS_FAT32                        3

#define UNUSED_CLUSTER					0
#define DEFECTIVE_CLUSTER				0xfff7
#define LAST_CLUSTER					0xffff

#define ATTRIB_ARCHIVE                  0x20
#define ATTRIB_RONLY                    0x10
#define ATTRIB_SYSTEM                   0x08
#define ATTRIB_HIDDEN					0x04
#define ATTRIB_DIR						0x02
#define ATTRIB_VOL						0x01

#define OPEN_R                          1
#define OPEN_W                          2

#define FROM_CURRENT					0
#define FROM_BEGIN                      1
#define FROM_END                        2

#define PATH_FULL                       0
#define PATH_EXCEPT_LAST				1

#define NOT_IF_EXIST					0
#define ALWAYS_CREATE					1

#define FIND_FILE_NAME					0
#define FIND_CLUSTER					1
#define FIND_FILE_INDEX					2
#define FIND_SUBDIR_INDEX				3
#define FIND_UNUSED						4
#define FIND_DELETED					5
#define FIND_PREVIOUS					6
#define FIND_FILE_FAT_NAME				7
#define FIND_NEXT                       8

#define ROOT_HANDLE                     0
#define ROOT_DIR_ENTRY					256
#define MAX_VERSION_INFO_LENGTH         40
#define MAX_DRIVE_NAME_LEN				10
#define MAX_PATH_NAME_LEN				256
#define MAX_OPENED_FILE_NUM				10

#define MAX_FILE_NAME_LEN				256

#define FAT2CPU16(x)					((u16)(((x&0x00ff)<<8)|((x&0xff00)>>8)))
#define FAT2CPU32(x)					((u32)(((x&0x000000ff)<<24)|((x&0x0000ff00)<<8)|((x&0x00ff0000)>>8)|((x&0xff000000)>>24)))

#define FAT_VAL8(x,ofs)					(u8)(*((u8*)(x)+(ofs)))
#define FAT_VAL16(x,ofs)				(u16)((u16)(*((u8*)(x)+(ofs))))|((u16)(*((u8*)(x)+(ofs)+1))<<8)
#define FAT_VAL32(x,ofs)				(u32)((u32)(*((u8*)(x)+(ofs))))|((u32)(*((u8*)(x)+(ofs)+1))<<8)|		\
											 ((u32)(*((u8*)(x)+(ofs)+2))<<16)|((u32)(*((u8*)(x)+(ofs)+3))<<24)
#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _partition_entry {
   u32 def_boot;
   u32 start_cyl;
   u32 start_head;
   u32 start_sector;
   u32 pt_type;
   u32 end_cyl;
   u32 end_head;
   u32 end_sector;
   u32 start_lba_sector;
   u32 total_avail_sectors;
} partition_entry;

typedef struct _bpb {
	u32 bytes_per_sect;
	u32 sects_per_cluster;
	u32 reserved_sects;
	u32 fat_num;
	u32 root_entry;
	u32 total_sects;
	u32 fmt_type;
	u32 sects_in_fat;
	u32 sects_per_track;
	u32 head_num;
	u32 hidden_sects;
	u32 huge_sects;
} bpb;

typedef struct _mbr {
	partition_entry partition_entries[4];
	u32 signature;
} mbr;

typedef struct _pbr {
	u8 oem_name[12];
	bpb sbpb;
	u32 drv_no;
	u32 reserved;
	u32 ext_boot_signature;
	u32 vol_id;
	u8 vol_label[12];
	u8 file_sys_type[8];
	u32 signature;
} pbr;

typedef struct _sm_info {
	mbr smbr;
	pbr spbr;
} sd_info;

typedef struct _filestat {
	u32 attr;
	u32 cluster;
	u32 size;
	struct tm time;
} file_stat;

typedef struct _dir_entry {
	u8 name[16];
} dir_entry;

typedef struct _dir_entry_ex {
	u8 name[16];
	u16 long_name[(MAX_FILE_NAME_LEN>>1)+1];		/* unicode */
	file_stat stat;
} dir_entryex;

typedef struct _fatcache {
	u32 f_ptr;
	u32 cnt;
	u8 *buf;
} fat_cache;

typedef u32 F_HANDLE;

typedef struct _opendfile_list {
	struct _opendfile_list *prev;
	struct _opendfile_list *next;
	u32 cluster;
	F_HANDLE h_parent;
	u32 f_ptr;
	u32 cur_cluster;
	u32 old_cur_cluster;
	u32 size;
	u32 mode;
	u32 id;
	fat_cache cache;	
} opendfile_list;

s32 card_init();

void card_initFATDefault();
s32 card_initFAT(s32 drv_no);
s32 card_termFAT(s32 drv_no);
s32 card_updateFAT(const char *p_volname);
s32 card_unmountFAT(const char *p_devname);

void card_insertedCB(s32 drv_no);
void card_ejectedCB(s32 drv_no);

s32 card_deleteFromOpenedList(s32 drv_no,u32 cluster);

s32 card_allocCluster(s32 drv_no,u32 *p_cluster);
s32 card_searchCluster(s32 drv_no,u32 count, u32 *pmax_cnt);
s32 card_setCluster(s32 drv_no,u32 cluster_no,u8 val);
s32 card_expandClusterSpace(s32 drv_no,F_HANDLE handle,u32 cluster,u32 offset,u32 len);
s32 card_writeCluster(s32 drv_no,u32 cluster_no,u32 offset,const void *buf,u32 len);
s32 card_readCluster(s32 drv_no,u32 cluster_no,u32 offset,void *buf,u32 len);
s32 card_writeBlock(s32 drv_no,u32 block_no,u32 offset,const void *buf,u32 len);
s32 card_readBlock(s32 drv_no,u32 block_no,u32 offset,void *buf,u32 len);

s32 card_createFile(const char *filename,u32 create_mode,F_HANDLE *p_handle);
s32 card_openFile(const char *filename,u32 open_mode,F_HANDLE *p_handle);
s32 card_closeFile(F_HANDLE h_file);
s32 card_seekFile(F_HANDLE h_file,u32 seek_mode,s32 offset,s32 *p_oldoffset);

s32 card_getFileSize(const char *filename,u32 *p_size);
s32 card_getListNumDir(const char *p_dirname,u32 *p_num);
s32 card_readDir(const char *dirname,u32 entry_start,u32 entry_cnt,dir_entryex *dir_buf,u32 *read_cnt);
s32 card_readStat(const char *p_entry_name,file_stat *p_stat);

s32 card_readFile(F_HANDLE h_file,void *buf,u32 cnt,u32 *p_cnt);
s32 card_writeFile(F_HANDLE h_file,const void *p_buf,u32 count);

s32 card_readFromDisk(s32 drv_no,opendfile_list *p_list,void *buf,u32 cnt,u32 *p_cnt);
s32 card_writeToDisk(s32 drv_no,opendfile_list *p_list,const u8 *p_buf,u32 count);
s32 card_writeCacheToDisk(s32 drv_no,opendfile_list *p_list);

s32 card_addDirEntry(s32 drv_no,F_HANDLE h_dir,const char* long_name,const u16* p_unicode_name,file_stat* p_stat,u32 b_insert,u32 cluster,u32 offset);
void card_prepareFileClose(s32 drv_no, const opendfile_list* p_list) ;
void card_registerCallback(u32 drv_no,void (*pFuncIN)(s32),void (*pFuncOUT)(s32));

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
