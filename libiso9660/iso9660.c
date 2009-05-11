#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <malloc.h>
#include <errno.h>
#include <gcutil.h>
#include <ogcsys.h>

#include <sys/dir.h>
#include <sys/types.h>
#include <sys/iosupport.h>

#include <ogc/lwp_watchdog.h>

#include "di.h"
#include "iso9660.h"

#define DR_ISHIDDEN			0x01
#define DR_ISDIR			0x02
#define DR_ISASSOC			0x04
#define DR_ISREC			0x08
#define DR_ISPROTECT		0x10

#define SECTOR_SIZE			0x0800
#define BUFFER_SIZE			0x8000

#define DEVICE_NAME			"dvd"
#define DIR_SEPERATOR		'/'

typedef union
{
	struct {
		u32 dword_le;
		u32 dword_be;
	} adword;
	u64 aqword;
} union_qword;

typedef union
{
	struct {
		u16 word_le;
		u16 word_be;
	} aword;
	u32 adword;
} union_dword;

struct rdt_s
{
	u8 years,
		month,
		day,
		hour,
		min,sec,
		gmt_offset;
} __attribute__((packed));

struct pvd_s 
{ 
	u8 id[8]; 
	u8 system_id[32]; 
	u8 volume_id[32]; 
	u8 zeros1[8];
	union_qword total_sectors;
	u8 zero2[32]; 
	union_dword volume_set_size;
	union_dword volume_seq_nr;
	union_dword sector_size;
	union_qword path_table_len;
	u32 path_table_le, path_table_2nd_le; 
	u32 path_table_be, path_table_2nd_be; 
	u8 root_direntry[34]; 
	u8 volume_set_id[128], publisher_id[128], data_preparer_id[128], application_id[128]; 
	u8 copyright_file_id[37], abstract_file_id[37], bibliographical_file_id[37];
	u8 creation_date[17],modify[17],expire[17],effective[17];
	u8 filestruc_ver,zero;
	u8 app_use[512];
	u8 res[653];
} __attribute__((packed)); 

struct dir_rec_s 
{
	u8 len_dr;
	u8 ext_ar_len;
	union_qword ext_location;
	union_qword ext_data_len;
	struct rdt_s rdt;
	u8 file_flags;
	u8 file_unit_size;
	u8 ileave_gap_size;
	union_dword vol_seq_no;
	u8 len_fi;
	u8 field_ident;
} __attribute__((packed));

struct ptable_rec_s
{
	u8 len_pr;
	u8 ext_len;
	u32 sector;
	u16 parent;
	u8 field_ident;
} __attribute__((packed));

typedef struct pentry_s
{
	u16 index;
	u32 childCount;
	struct ptable_rec_s *table_entry;
	struct pentry_s *children;
} PATH_ENTRY;

typedef struct dentry_s
{
	char name[ISO_MAXPATHLEN];
	u32 sector;
	u32 size;
	u8 flags;
	u32 fileCount;
	PATH_ENTRY *path_entry;
	struct dentry_s *children;
} DIR_ENTRY;

typedef struct filestruct_s
{
	DIR_ENTRY entry;
	u32 offset;
	BOOL inUse;
} FILE_STRUCT;

typedef struct dstate_s
{
	DIR_ENTRY entry;
	u32 index;
	BOOL inUse;
} DIR_STATE_STRUCT;

static u32 cache_start = 0;
static u32 cache_sectors = 0;
static mutex_t _mutex = 0;

static s32 dotab_device = -1;
static u64 iso_last_access = 0;
static BOOL iso_unicode = FALSE;
static void *iso_path_table = NULL;
static PATH_ENTRY *iso_rootentry = NULL;
static PATH_ENTRY *iso_currententry = NULL;

static u8 read_buffer[BUFFER_SIZE] ATTRIBUTE_ALIGN(32);
static u8 cluster_buffer[BUFFER_SIZE] ATTRIBUTE_ALIGN(32);

static __inline__ BOOL is_dir(DIR_ENTRY *entry)
{
	return ((entry->flags&DR_ISDIR)==DR_ISDIR);
}

static __inline__ const DISC_INTERFACE* get_interface()
{
#if defined(HW_RVL)
	return &__io_wiidvd;
#endif
#if defined(HW_DOL)
	return NULL;
#endif
}
static int _read(void *ptr,u64 offset,size_t len)
{
	u32 sector = offset/SECTOR_SIZE;
	u32 sector_offset = offset%SECTOR_SIZE;
	u32 end_sector = (offset + len + 1)/SECTOR_SIZE;
	u32 num_sectors = MIN(BUFFER_SIZE/SECTOR_SIZE,end_sector - sector + 1);
	const DISC_INTERFACE *disc = get_interface();

	len = MIN(BUFFER_SIZE - sector_offset,len);
	if(cache_sectors && sector>=cache_start && (sector+num_sectors)<=(cache_start+cache_sectors)) {
		memcpy(ptr,read_buffer+(sector-cache_start)*SECTOR_SIZE+sector_offset,len);
		return len;
	}

	if(!disc->readSectors(sector,BUFFER_SIZE/SECTOR_SIZE,read_buffer)) {
		iso_last_access = gettime();
		cache_sectors = 0;
		return -1;
	}

	iso_last_access = gettime();
	cache_start = sector;
	cache_sectors = BUFFER_SIZE/SECTOR_SIZE;
	memcpy(ptr,read_buffer+sector_offset,len);

	return len;
}

static int _iso_mbsncasecmp(const char *s1,const char *s2,size_t len1)
{
	wchar_t wc1,wc2;
	mbstate_t ps1 = {0};
	mbstate_t ps2 = {0};
	size_t b1 = 0;
	size_t b2 = 0;

	if(len1==0) return 0;

	do {
		s1 += b1;
		s2 += b2;
		b1 = mbrtowc(&wc1,s1,MB_CUR_MAX,&ps1);
		b2 = mbrtowc(&wc2,s2,MB_CUR_MAX,&ps2);
		if((int)b1<0 || (int)b2<0) break;

		len1 -= b1;
	} while(len1>0 && towlower(wc1)==towlower(wc2) && wc1!=0);

	return towlower(wc1) - towlower(wc2);
}

static void stat_entry(DIR_ENTRY *entry,struct stat *st)
{
	st->st_dev = 69;
	st->st_ino = (ino_t)entry->sector;
	st->st_mode = (is_dir(entry) ? S_IFDIR : S_IFREG) | (S_IRUSR | S_IRGRP | S_IROTH);
	st->st_nlink = 1;
	st->st_uid = 1;
	st->st_gid = 2;
	st->st_rdev = st->st_dev;
	st->st_size = entry->size;
	st->st_atime = 0;
	st->st_spare1 = 0;
	st->st_mtime = 0;
	st->st_spare2 = 0;
	st->st_ctime = 0;
	st->st_spare3 = 0;
	st->st_blksize = SECTOR_SIZE;
	st->st_blocks = (entry->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
	st->st_spare4[0] = 0;
	st->st_spare4[1] = 0;
}

static char* basename(char *path)
{
	s32 i;

	for(i=strlen(path)-1;i>=0;i--) {
		if(path[i]==DIR_SEPERATOR) return path+i+1;
	}
	return path;
}

static char* dirname(char *path)
{
	size_t i,j;
	char *result = strdup(path);

	j = strlen(result) - 1;
	if(j<0) return result;

	for(i=j;i>=0;i--) {
		if(result[i]==DIR_SEPERATOR) {
			result[i] = '\0';
			return result;
		}
	}

	result[0] = '\0';
	return result;
}

static BOOL invalid_drive_specifier(const char *path)
{
	s32 namelen;

	if(strchr(path,':')==NULL) return FALSE;

	namelen = strlen(DEVICE_NAME);
	if(!strncmp(DEVICE_NAME,path,namelen) && path[namelen]==':') return FALSE;

	return TRUE;
}

static s32 read_direntry(DIR_ENTRY *entry,u8 *buf)
{
	u8 flags;
	s32 len_fi,i;
	u8 *namefield;
	u32 sector,size;
	struct dir_rec_s *dirrec = (struct dir_rec_s*)buf;

	len_fi = dirrec->len_fi;
	namefield = &dirrec->field_ident;
	flags = dirrec->file_flags;
	size = dirrec->ext_data_len.adword.dword_be;
	sector = dirrec->ext_location.adword.dword_be + dirrec->ext_ar_len;
	if(len_fi==1 && *namefield==1) {
		//..
	} else if(len_fi==1 && *namefield==0) {
		entry->sector = sector;
		entry->size = size;
		entry->flags = flags;
	} else {
		char *name;
		DIR_ENTRY *child;
		DIR_ENTRY *newChildren = realloc(entry->children,((entry->fileCount+1)*sizeof(DIR_ENTRY)));
		if(!newChildren) return -1;

		memset(newChildren+entry->fileCount,0,sizeof(DIR_ENTRY));
		entry->children = newChildren;

		child = &entry->children[entry->fileCount++];
		child->sector = sector;
		child->size = size;
		child->flags = flags;
		name = child->name;
		if(iso_unicode) {
			for(i=0;i<(len_fi/2);i++) name[i] = namefield[i*2+1];
			name[i] = '\0';
			len_fi = i;
		} else {
			memcpy(name,namefield,len_fi);
			name[len_fi] = '\0';
		}
		if(!(flags&DR_ISDIR) && len_fi>=2 && name[len_fi-2]==';') name[len_fi-2] = '\0';
	}

	return dirrec->len_dr;
}

static BOOL read_directory(DIR_ENTRY *entry,PATH_ENTRY *path)
{
	s32 offs;
	s32 remaining = 0;
	u32 sector_offs = 0;
	u32 sector = path->table_entry->sector;

	do {
		if(_read(cluster_buffer,(u64)sector*SECTOR_SIZE+sector_offs,(SECTOR_SIZE - sector_offs))!=(SECTOR_SIZE - sector_offs)) return FALSE;

		offs = read_direntry(entry,cluster_buffer);
		if(offs==-1) return FALSE;

		if(!remaining) {
			remaining = entry->size;
			entry->path_entry = path;
		}

		sector_offs += offs;
		if(sector_offs>=SECTOR_SIZE || !cluster_buffer[offs]) {
			remaining -= SECTOR_SIZE;
			sector_offs = 0;
			sector++;
		}
	} while(remaining>0);

	return TRUE;
}

static BOOL path_entry_from_path(PATH_ENTRY *path_entry,const char *path)
{
	BOOL found = FALSE;
	PATH_ENTRY *entry = iso_rootentry;
	PATH_ENTRY *dir = entry;
	const char *pathStart = path;
	const char *pathEnd = strchr(path,'\0');

	while(pathStart[0]==DIR_SEPERATOR) pathStart++;
	if(pathStart>=pathEnd) found = TRUE;

	while(!found) {
		u32 childIdx;
		char *path_name;
		size_t dirnameLen;
		const char *pathNext = strchr(pathStart,DIR_SEPERATOR);

		if(pathNext!=NULL) dirnameLen = pathNext - pathStart;
		else dirnameLen = strlen(pathStart);

		if(dirnameLen>=ISO_MAXPATHLEN) return FALSE;

		childIdx = 0;
		while(childIdx<dir->childCount) {
			entry = &dir->children[childIdx];
			path_name = (char*)&entry->table_entry->field_ident;
			if(dirnameLen==strnlen(path_name,ISO_MAXPATHLEN-1) && !_iso_mbsncasecmp(pathStart,path_name,dirnameLen)) {
				found = TRUE;
				break;
			}
			childIdx++;
		}
		if(childIdx>=dir->childCount) break;
		if(!pathNext || pathNext>=pathEnd) break;

		dir = entry;
		pathStart = pathNext;
		while(pathStart[0]==DIR_SEPERATOR) pathStart++;
		if(pathStart>=pathEnd) break;

		found = FALSE;
	}

	if(found) memcpy(path_entry,entry,sizeof(PATH_ENTRY));
	return found;
}

static BOOL find_in_directory(DIR_ENTRY *entry,PATH_ENTRY *parent,const char *base)
{
	u32 childIdx;
	u32 nl = strlen(base);

	if(!nl) return read_directory(entry,parent);

	for(childIdx=0;childIdx<parent->childCount;childIdx++) {
		PATH_ENTRY *child = parent->children+childIdx;
		char *pathname = (char*)&child->table_entry->field_ident;
		if(nl==strnlen(pathname,ISO_MAXPATHLEN-1) && !_iso_mbsncasecmp(base,pathname,nl)) {
			return read_directory(entry,child);
		}
	}

	if(!read_directory(entry,parent)) return FALSE;
	for(childIdx=0;childIdx<entry->fileCount;childIdx++) {
		DIR_ENTRY *child = entry->children+childIdx;
		if(nl==strnlen(child->name,ISO_MAXPATHLEN-1) && !_iso_mbsncasecmp(base,child->name,nl)) {
			memcpy(entry,child,sizeof(DIR_ENTRY));
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL entry_from_path(DIR_ENTRY *entry,const char *const_path)
{
	u32 len;
	BOOL found;
	char *path,*dir,*base;
	PATH_ENTRY parent_entry;

	if(invalid_drive_specifier(const_path)) return FALSE;

	memset(entry,0,sizeof(DIR_ENTRY));

	if(strchr(const_path,':')!=NULL) const_path = strchr(const_path,':')+1;

	path = strdup(const_path);
	len = strlen(path);
	while(len>1 && path[len-1]==DIR_SEPERATOR) path[--len] = '\0';

	found = FALSE;
	dir = dirname(path);
	base = basename(path);
	if(!path_entry_from_path(&parent_entry,dir)) goto done;

	found = find_in_directory(entry,&parent_entry,base);
	if(!found && entry->children) free(entry->children);

done:
	free(path);
	free(dir);
	return found;
}

static int _ISO9660_open_r(struct _reent *r,void *fileStruct,const char *path,int flags,int mode)
{
	DIR_ENTRY entry;
	FILE_STRUCT *file = (FILE_STRUCT*)fileStruct;

	if(!entry_from_path(&entry,path)) {
		r->_errno = ENOENT;
		return -1;
	} else if(is_dir(&entry)) {
		if(entry.children) free(entry.children);
		r->_errno = EISDIR;
		return -1;
	}

	memcpy(&file->entry,&entry,sizeof(DIR_ENTRY));
	file->offset = 0;
	file->inUse = TRUE;

	return (int)file;
}

static int _ISO9660_close_r(struct _reent *r,int fd)
{
	FILE_STRUCT *file = (FILE_STRUCT*)fd;

	if(!file->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	file->inUse = FALSE;
	return 0;
}

static ssize_t _ISO9660_read_r(struct _reent *r,int fd,char *ptr,size_t len)
{
	u64 offset;
	FILE_STRUCT *file = (FILE_STRUCT*)fd;

	if(!file->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	if(file->offset>=file->entry.size) {
		r->_errno = EOVERFLOW;
		return 0;
	}

	if(len+file->offset>file->entry.size) {
		r->_errno = EOVERFLOW;
		len = file->entry.size - file->offset;
	}
	if(len<=0) return 0;

	offset = file->entry.sector*SECTOR_SIZE+file->offset;
	if((len=_read(ptr,offset,len))<0) {
		r->_errno = EIO;
		return -1;
	}

	file->offset += len;
	return len;
}

static off_t _ISO9660_seek_r(struct _reent *r,int fd,off_t pos,int dir)
{
	int position;
	FILE_STRUCT *file = (FILE_STRUCT*)fd;

	if(!file->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	switch(dir) {
		case SEEK_SET:
			position = pos;
			break;
		case SEEK_CUR:
			position = file->offset + pos;
			break;
		case SEEK_END:
			position = file->entry.size + pos;
			break;
		default:
			r->_errno = EINVAL;
			return -1;
	}

	if(pos>0 && position<0) {
		r->_errno = EOVERFLOW;
		return -1;
	}

	if(position<0 || position>file->entry.size) {
		r->_errno = EINVAL;
		return -1;
	}

	file->offset = position;
	return position;
}

static int _ISO9660_fstat_r(struct _reent *r,int fd,struct stat *st)
{
	FILE_STRUCT *file = (FILE_STRUCT*)fd;

	if(!file->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	stat_entry(&file->entry,st);
	return 0;
}

static int _ISO9660_stat_r(struct _reent *r,const char *path,struct stat *st)
{
	DIR_ENTRY entry;
	
	if(!entry_from_path(&entry,path)) {
		r->_errno = ENOENT;
		return -1;
	}

	stat_entry(&entry,st);
	if(entry.children) free(entry.children);

	return 0;
}

static int _ISO9660_chdir_r(struct _reent *r,const char *path)
{
	DIR_ENTRY entry;

	if(!entry_from_path(&entry,path)) {
		r->_errno = ENOENT;
		return -1;
	} else if(!is_dir(&entry)) {
		if(entry.children) free(entry.children);
		r->_errno = ENOTDIR;
		return -1;
	}

	iso_currententry = entry.path_entry;
	if(entry.children) free(entry.children);

	return 0;
}

static DIR_ITER* _ISO9660_diropen_r(struct _reent *r,DIR_ITER *dirState,const char *path)
{
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*)(dirState->dirStruct);

	if(!entry_from_path(&state->entry,path)) {
		r->_errno = ENOENT;
		return NULL;
	} else if(!is_dir(&state->entry)) {
		r->_errno = ENOTDIR;
		return NULL;
	}

	state->index = 0;
	state->inUse = TRUE;
	return dirState;
}

static int _ISO9660_dirreset_r(struct _reent *r,DIR_ITER *dirState)
{
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*)(dirState->dirStruct);
	
	if(!state->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	state->index = 0;
	return 0;
}

static int _ISO9660_dirnext_r(struct _reent *r,DIR_ITER *dirState,char *filename,struct stat *st)
{
	DIR_ENTRY *entry;
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*)(dirState->dirStruct);

	if(!state->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	if(state->index>=state->entry.fileCount) {
		r->_errno = ENOENT;
		return -1;
	}

	entry = &state->entry.children[state->index++];
	strncpy(filename,entry->name,ISO_MAXPATHLEN-1);
	stat_entry(entry,st);
	return 0;
}

static int _ISO9660_dirclose_r(struct _reent *r,DIR_ITER *dirState)
{
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*)(dirState->dirStruct);

	if(!state->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	state->inUse = FALSE;
	if(state->entry.children) free(state->entry.children);
	return 0;
}

static const devoptab_t dotab_iso9660 = {
	DEVICE_NAME,
	sizeof(FILE_STRUCT),
	_ISO9660_open_r,
	_ISO9660_close_r,
	NULL,
	_ISO9660_read_r,
	_ISO9660_seek_r,
	_ISO9660_fstat_r,
	_ISO9660_stat_r,
	NULL,
	NULL,
	_ISO9660_chdir_r,
	NULL,
	NULL,
	sizeof(DIR_STATE_STRUCT),
	_ISO9660_diropen_r,
	_ISO9660_dirreset_r,
	_ISO9660_dirnext_r,
	_ISO9660_dirclose_r,
	NULL
};

static PATH_ENTRY* entry_from_index(PATH_ENTRY *entry,u16 index)
{
	u32 i;

	if(entry->index==index) return entry;

	for(i=0;i<entry->childCount;i++) {
		PATH_ENTRY *match = entry_from_index(&entry->children[i],index);
		if(match) return match;
	}
	return NULL;
}

static PATH_ENTRY* add_child_entry(PATH_ENTRY *dir)
{
	PATH_ENTRY *child;
	PATH_ENTRY *newChildren = NULL;
	
	newChildren = realloc(dir->children,(dir->childCount+1)*sizeof(PATH_ENTRY));
	if(newChildren==NULL) return NULL;

	memset(newChildren+dir->childCount,0,sizeof(PATH_ENTRY));
	dir->children = newChildren;

	child = &dir->children[dir->childCount++];
	return child;
}

static void cleanup_recursive(PATH_ENTRY *entry)
{
	u32 i;

	for(i=0;i<entry->childCount;i++) cleanup_recursive(&entry->children[i]);
	if(entry->children) free(entry->children);
}

static struct pvd_s* read_volume_descriptor(u8 descriptor)
{
	u8 sector;
	const DISC_INTERFACE *disc = get_interface();

	for(sector=16;sector<32;sector++) {
		if(!disc->readSectors(sector,1,read_buffer)) return NULL;
		if(!memcmp(read_buffer+1,"CD001\1",6)) {
			if(*read_buffer==descriptor) return (struct pvd_s*)read_buffer;
			else if(*read_buffer==0xff) return NULL;
		}
	}

	return NULL;
}

static BOOL read_directories()
{
	u16 cnt;
	u8 *pname;
	s32 offs,pos,i;
	u32 ptable_len,ptable_pos;
	u8 *path_table = NULL;
	PATH_ENTRY *child,*parent = NULL;
	struct dir_rec_s *root_dir = NULL;
	struct ptable_rec_s *ptrec = NULL;
	struct pvd_s *volume = read_volume_descriptor(2);

	iso_unicode = FALSE;
	if(volume) iso_unicode = TRUE;
	else if((volume=read_volume_descriptor(1))==NULL) return FALSE;

	root_dir = (struct dir_rec_s*)volume->root_direntry;
	if((iso_rootentry=malloc(sizeof(PATH_ENTRY)))==NULL) return FALSE;

	parent = iso_rootentry;
	iso_currententry = iso_rootentry;
	ptable_pos = volume->path_table_be;
	ptable_len = volume->path_table_len.adword.dword_be;
	iso_path_table = malloc(ptable_len);
	if(iso_path_table) {
		path_table = iso_path_table;
		if(_read(path_table,(u64)ptable_pos*SECTOR_SIZE,ptable_len)!=ptable_len) return FALSE;
		
		cnt = 1;
		offs = 0;
		parent->index = cnt++;
		ptrec = (struct ptable_rec_s*)path_table;
		while(offs<ptable_len && ptrec->len_pr) {
			pname = &ptrec->field_ident;
			pos = ptrec->len_pr+sizeof(struct ptable_rec_s);
			if(ptrec->len_pr&1) pos++;

			if(parent->index!=ptrec->parent) parent = entry_from_index(iso_rootentry,ptrec->parent);
			if(!parent) return FALSE;

			//if offs==0 then it's the root entry and we've to set it to the parent. done only once.
			if(offs) {
				child = add_child_entry(parent);
				if(!child) return FALSE;

				child->index = cnt++;
				child->table_entry = ptrec;
			} else
				iso_rootentry->table_entry = ptrec;

			if(iso_unicode) {
				for(i=0;i<(ptrec->len_pr/2);i++) pname[i] = pname[i*2+1];
				ptrec->len_pr = i;
				pname[i] = '\0';
			} else {
				pname[ptrec->len_pr] = '\0';
			}

			offs += pos;
			ptrec = (struct ptable_rec_s*)(path_table+offs);
		}
		return TRUE;
	}
	return FALSE;
}

BOOL ISO9660_Mount()
{
	BOOL success = FALSE;
	const DISC_INTERFACE *disc = get_interface();

	ISO9660_Unmount();

	if(disc->startup()) {
		LWP_MutexInit(&_mutex,false);

		success = (read_directories() && (dotab_device=AddDevice(&dotab_iso9660))>=0);
		if(success) iso_last_access = gettime();
		else ISO9660_Unmount();
	}

	return success;
}

BOOL ISO9660_Unmount()
{
	if(iso_rootentry) {
		cleanup_recursive(iso_rootentry);
		free(iso_rootentry);
		iso_rootentry = NULL;
	}

	if(iso_path_table) {
		free(iso_path_table);
		iso_path_table = NULL;
	}

	if(_mutex!=0) {
		LWP_MutexDestroy(_mutex);
		_mutex = 0;
	}

	iso_last_access = 0;
	iso_unicode = FALSE;
	iso_currententry = iso_rootentry;
	if(dotab_device>=0) {
		dotab_device = -1;
		return !RemoveDevice(DEVICE_NAME ":");
	}

	return TRUE;
}

u64 ISO9660_LastAccess()
{
	return iso_last_access;
}
