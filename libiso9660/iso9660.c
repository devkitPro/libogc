#include <di/di.h>
#include <errno.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/iosupport.h>

#include "iso9660.h"

#define OFFSET_EXTENDED 1
#define OFFSET_SECTOR 6
#define OFFSET_SIZE 14
#define OFFSET_FLAGS 25
#define OFFSET_NAMELEN 32
#define OFFSET_NAME 33

#define SECTOR_SIZE			0x800
#define BUFFER_SIZE			0x8000

#define DEVICE_NAME			"dvd"
#define DIR_SEPARATOR		'/'

#define FLAG_DIR 2

struct pvd_s
{
	char id[8];
	char system_id[32];
	char volume_id[32];
	char zero[8];
	unsigned long total_sector_le, total_sect_be;
	char zero2[32];
	unsigned long volume_set_size, volume_seq_nr;
	unsigned short sector_size_le, sector_size_be;
	unsigned long path_table_len_le, path_table_len_be;
	unsigned long path_table_le, path_table_2nd_le;
	unsigned long path_table_be, path_table_2nd_be;
	u8 root[34];
	char volume_set_id[128], publisher_id[128], data_preparer_id[128],
			application_id[128];
	char copyright_file_id[37], abstract_file_id[37],
			bibliographical_file_id[37];
}__attribute__((packed));

typedef struct
{
	u8 name_length;
	u8 extended_sectors;
	u32 sector;
	u16 parent;
	char name[ISO_MAXPATHLEN];
}__attribute__((packed)) PATHTABLE_ENTRY;

typedef struct pentry_s
{
	u16 index;
	u32 childCount;
	PATHTABLE_ENTRY table_entry;
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

static u8 read_buffer[BUFFER_SIZE] __attribute__((aligned(32)));
static u8 cluster_buffer[BUFFER_SIZE] __attribute__((aligned(32)));
static u32 cache_start = 0;
static u32 cache_sectors = 0;

static s32 dotab_device = -1;
static u64 iso_last_access = 0;
static BOOL iso_unicode = FALSE;
static PATH_ENTRY *iso_rootentry = NULL;
static PATH_ENTRY *iso_currententry = NULL;

static __inline__ BOOL is_dir(DIR_ENTRY *entry)
{
	return entry->flags & FLAG_DIR;
}

static __inline__ const DISC_INTERFACE* get_interface()
{
#if defined(HW_RVL)
	return &__io_wiidvd;
#endif
#if defined(HW_DOL)
	return &__io_gcdvd;
#endif
}

static int _read(void *ptr, u64 offset, size_t len)
{
	u32 sector = offset / SECTOR_SIZE;
	u32 end_sector = (offset + len - 1) / SECTOR_SIZE;
	u32 sectors = MIN(BUFFER_SIZE / SECTOR_SIZE, end_sector - sector + 1);
	u32 sector_offset = offset % SECTOR_SIZE;
	const DISC_INTERFACE *disc = get_interface();

	len = MIN(BUFFER_SIZE - sector_offset, len);
	if (cache_sectors && sector >= cache_start && (sector + sectors)
			<= (cache_start + cache_sectors))
	{
		memcpy(ptr, read_buffer + (sector - cache_start) * SECTOR_SIZE
				+ sector_offset, len);
		return len;
	}

	if (!disc->readSectors(sector, BUFFER_SIZE / SECTOR_SIZE, read_buffer))
	{
		iso_last_access = gettime();
		cache_sectors = 0;
		return -1;
	}

	iso_last_access = gettime();
	cache_start = sector;
	cache_sectors = BUFFER_SIZE / SECTOR_SIZE;
	memcpy(ptr, read_buffer + sector_offset, len);

	return len;
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

	for (i = strlen(path) - 1; i >= 0; i--)
	{
		if (path[i] == DIR_SEPARATOR)
			return path + i + 1;
	}
	return path;
}

static char* dirname(char *path)
{
	size_t i, j;
	char *result = strdup(path);

	j = strlen(result) - 1;
	if (j < 0)
		return result;

	for (i = j; i >= 0; i--)
	{
		if (result[i] == DIR_SEPARATOR)
		{
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

	if (strchr(path, ':') == NULL)
		return FALSE;

	namelen = strlen(DEVICE_NAME);
	if (!strncmp(DEVICE_NAME, path, namelen) && path[namelen] == ':')
		return FALSE;

	return TRUE;
}

static s32 read_direntry(DIR_ENTRY *entry, u8 *buf)
{
	u8 extended_sectors = buf[OFFSET_EXTENDED];
	u32 sector = *(u32 *) (buf + OFFSET_SECTOR) + extended_sectors;
	u32 size = *(u32 *) (buf + OFFSET_SIZE);
	u8 flags = buf[OFFSET_FLAGS];
	u8 namelen = buf[OFFSET_NAMELEN];

	if (namelen == 1 && buf[OFFSET_NAME] == 1 
			&& iso_rootentry->table_entry.sector == entry->sector)
	{
		// .. at root - do not show
	}
	else if (namelen == 1 && !buf[OFFSET_NAME])
	{
		entry->sector = sector;
		entry->size = size;
		entry->flags = flags;
	}
	else
	{
		DIR_ENTRY *newChildren = realloc(entry->children, sizeof(DIR_ENTRY)
				* (entry->fileCount + 1));
		if (!newChildren)
			return -1;
		memset(newChildren + entry->fileCount, 0, sizeof(DIR_ENTRY));
		entry->children = newChildren;
		DIR_ENTRY *child = &entry->children[entry->fileCount++];
		child->sector = sector;
		child->size = size;
		child->flags = flags;
		char *name = child->name;

		if (namelen == 1 && buf[OFFSET_NAME] == 1)
		{
			// ..
			sprintf(name, "..");
		}
		else if (iso_unicode)
		{
			u32 i;
			for (i = 0; i < (namelen / 2); i++)
				name[i] = buf[OFFSET_NAME + i * 2 + 1];
			name[i] = '\x00';
			namelen = i;
		}
		else
		{
			memcpy(name, buf + OFFSET_NAME, namelen);
			name[namelen] = '\x00';
		}
		if (!(flags & FLAG_DIR) && namelen >= 2 && name[namelen - 2] == ';')
			name[namelen - 2] = '\x00';
	}

	return *buf;
}

static BOOL read_directory(DIR_ENTRY *dir_entry, PATH_ENTRY *path_entry)
{
	u32 sector = path_entry->table_entry.sector;
	u32 remaining = 0;
	u32 sector_offset = 0;

	do
	{
		if (_read(cluster_buffer, (u64) sector * SECTOR_SIZE + sector_offset,
				(SECTOR_SIZE - sector_offset)) != (SECTOR_SIZE - sector_offset))
			return FALSE;
		int offset = read_direntry(dir_entry, cluster_buffer);
		if (offset == -1)
			return FALSE;
		if (!remaining)
		{
			remaining = dir_entry->size;
			dir_entry->path_entry = path_entry;
		}
		sector_offset += offset;
		if (sector_offset >= SECTOR_SIZE || !cluster_buffer[offset])
		{
			remaining -= SECTOR_SIZE;
			sector_offset = 0;
			sector++;
		}
	} while (remaining > 0);

	return TRUE;
}

static BOOL path_entry_from_path(PATH_ENTRY *path_entry, const char *path)
{
	BOOL found = FALSE;
	BOOL notFound = FALSE;
	const char *pathPosition = path;
	const char *pathEnd = strchr(path, '\0');
	PATH_ENTRY *entry = iso_rootentry;
	while (pathPosition[0] == DIR_SEPARATOR)
		pathPosition++;
	if (pathPosition >= pathEnd)
		found = TRUE;
	PATH_ENTRY *dir = entry;
	while (!found && !notFound)
	{
		const char *nextPathPosition = strchr(pathPosition, DIR_SEPARATOR);
		size_t dirnameLength;
		if (nextPathPosition != NULL)
			dirnameLength = nextPathPosition - pathPosition;
		else
			dirnameLength = strlen(pathPosition);
		if (dirnameLength >= ISO_MAXPATHLEN)
			return FALSE;

		u32 childIndex = 0;
		while (childIndex < dir->childCount && !found && !notFound)
		{
			entry = &dir->children[childIndex];
			if (dirnameLength == strnlen(entry->table_entry.name,
					ISO_MAXPATHLEN - 1) && !strncasecmp(pathPosition,
					entry->table_entry.name, dirnameLength))
				found = TRUE;
			if (!found)
				childIndex++;
		}

		if (childIndex >= dir->childCount)
		{
			notFound = TRUE;
			found = FALSE;
		}
		else if (!nextPathPosition || nextPathPosition >= pathEnd)
		{
			found = TRUE;
		}
		else
		{
			dir = entry;
			pathPosition = nextPathPosition;
			while (pathPosition[0] == DIR_SEPARATOR)
				pathPosition++;
			if (pathPosition >= pathEnd)
				found = TRUE;
			else
				found = FALSE;
		}
	}

	if (found)
		memcpy(path_entry, entry, sizeof(PATH_ENTRY));
	return found;
}

static BOOL find_in_directory(DIR_ENTRY *entry, PATH_ENTRY *parent,
		const char *base)
{
	u32 childIdx;
	u32 nl = strlen(base);

	if (!nl)
		return read_directory(entry, parent);

	for (childIdx = 0; childIdx < parent->childCount; childIdx++)
	{
		PATH_ENTRY *child = parent->children + childIdx;
		if (nl == strnlen(child->table_entry.name, ISO_MAXPATHLEN - 1)
				&& !strncasecmp(base, child->table_entry.name, nl))
		{
			return read_directory(entry, child);
		}
	}

	if (!read_directory(entry, parent))
		return FALSE;
	for (childIdx = 0; childIdx < entry->fileCount; childIdx++)
	{
		DIR_ENTRY *child = entry->children + childIdx;
		if (nl == strnlen(child->name, ISO_MAXPATHLEN - 1) && !strncasecmp(
				base, child->name, nl))
		{
			memcpy(entry, child, sizeof(DIR_ENTRY));
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL entry_from_path(DIR_ENTRY *entry, const char *const_path)
{
	u32 len;
	BOOL found = FALSE;
	char *path, *dir, *base;
	PATH_ENTRY parent_entry;

	if (invalid_drive_specifier(const_path))
		return FALSE;

	memset(entry, 0, sizeof(DIR_ENTRY));

	if (strchr(const_path, ':') != NULL)
		const_path = strchr(const_path, ':') + 1;

	path = strdup(const_path);
	len = strlen(path);
	while (len > 1 && path[len - 1] == DIR_SEPARATOR)
		path[--len] = '\x00';

	dir = dirname(path);
	base = basename(path);
	if (!path_entry_from_path(&parent_entry, dir))
		goto done;

	found = find_in_directory(entry, &parent_entry, base);
	if (!found && entry->children)
		free(entry->children);

done: 
	free(path);
	free(dir);
	return found;
}

static int _ISO9660_open_r(struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
	DIR_ENTRY entry;
	FILE_STRUCT *file = (FILE_STRUCT*) fileStruct;

	if (!entry_from_path(&entry, path))
	{
		r->_errno = ENOENT;
		return -1;
	}
	else if (is_dir(&entry))
	{
		if (entry.children)
			free(entry.children);
		r->_errno = EISDIR;
		return -1;
	}

	memcpy(&file->entry, &entry, sizeof(DIR_ENTRY));
	file->offset = 0;
	file->inUse = TRUE;

	return (int) file;
}

static int _ISO9660_close_r(struct _reent *r, int fd)
{
	FILE_STRUCT *file = (FILE_STRUCT*) fd;

	if (!file->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	file->inUse = FALSE;
	return 0;
}

static ssize_t _ISO9660_read_r(struct _reent *r, int fd, char *ptr, size_t len)
{
	u64 offset;
	FILE_STRUCT *file = (FILE_STRUCT*) fd;

	if (!file->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	if (file->offset >= file->entry.size)
	{
		r->_errno = EOVERFLOW;
		return 0;
	}

	if (len + file->offset > file->entry.size)
	{
		r->_errno = EOVERFLOW;
		len = file->entry.size - file->offset;
	}
	if (len <= 0)
		return 0;

	offset = file->entry.sector * SECTOR_SIZE + file->offset;
	if ((len = _read(ptr, offset, len)) < 0)
	{
		r->_errno = EIO;
		return -1;
	}

	file->offset += len;
	return len;
}

static off_t _ISO9660_seek_r(struct _reent *r, int fd, off_t pos, int dir)
{
	int position;
	FILE_STRUCT *file = (FILE_STRUCT*) fd;

	if (!file->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	switch (dir)
	{
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

	if (pos > 0 && position < 0)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	if (position < 0 || position > file->entry.size)
	{
		r->_errno = EINVAL;
		return -1;
	}

	file->offset = position;
	return position;
}

static int _ISO9660_fstat_r(struct _reent *r, int fd, struct stat *st)
{
	FILE_STRUCT *file = (FILE_STRUCT*) fd;

	if (!file->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	stat_entry(&file->entry, st);
	return 0;
}

static int _ISO9660_stat_r(struct _reent *r, const char *path, struct stat *st)
{
	DIR_ENTRY entry;

	if (!entry_from_path(&entry, path))
	{
		r->_errno = ENOENT;
		return -1;
	}

	stat_entry(&entry, st);
	if (entry.children)
		free(entry.children);

	return 0;
}

static int _ISO9660_chdir_r(struct _reent *r, const char *path)
{
	DIR_ENTRY entry;

	if (!entry_from_path(&entry, path))
	{
		r->_errno = ENOENT;
		return -1;
	}
	else if (!is_dir(&entry))
	{
		r->_errno = ENOTDIR;
		return -1;
	}

	iso_currententry = entry.path_entry;
	if (entry.children)
		free(entry.children);

	return 0;
}

static DIR_ITER* _ISO9660_diropen_r(struct _reent *r, DIR_ITER *dirState, const char *path)
{
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*) (dirState->dirStruct);

	if (!entry_from_path(&state->entry, path))
	{
		r->_errno = ENOENT;
		return NULL;
	}
	else if (!is_dir(&state->entry))
	{
		r->_errno = ENOTDIR;
		return NULL;
	}

	state->index = 0;
	state->inUse = TRUE;
	return dirState;
}

static int _ISO9660_dirreset_r(struct _reent *r, DIR_ITER *dirState)
{
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*) (dirState->dirStruct);

	if (!state->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	state->index = 0;
	return 0;
}

static int _ISO9660_dirnext_r(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st)
{
	DIR_ENTRY *entry;
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*) (dirState->dirStruct);

	if (!state->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	if (state->index >= state->entry.fileCount)
	{
		r->_errno = ENOENT;
		return -1;
	}

	entry = &state->entry.children[state->index++];
	strncpy(filename, entry->name, ISO_MAXPATHLEN - 1);
	stat_entry(entry, st);
	return 0;
}

static int _ISO9660_dirclose_r(struct _reent *r, DIR_ITER *dirState)
{
	DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT*) (dirState->dirStruct);

	if (!state->inUse)
	{
		r->_errno = EBADF;
		return -1;
	}

	state->inUse = FALSE;
	if (state->entry.children)
		free(state->entry.children);
	return 0;
}

static int _ISO9660_statvfs_r(struct _reent *r, const char *path,
		struct statvfs *buf)
{
	// FAT clusters = POSIX blocks
	buf->f_bsize = 0x800; // File system block size. 
	buf->f_frsize = 0x800; // Fundamental file system block size. 

	//buf->f_blocks	= totalsectors; // Total number of blocks on file system in units of f_frsize. 
	buf->f_bfree = 0; // Total number of free blocks. 
	buf->f_bavail = 0; // Number of free blocks available to non-privileged process. 

	// Treat requests for info on inodes as clusters
	//buf->f_files = totalentries;	// Total number of file serial numbers. 
	buf->f_ffree = 0; // Total number of free file serial numbers. 
	buf->f_favail = 0; // Number of file serial numbers available to non-privileged process. 

	// File system ID. 32bit ioType value
	buf->f_fsid = 0; //??!!? 

	// Bit mask of f_flag values.
	buf->f_flag = ST_NOSUID // No support for ST_ISUID and ST_ISGID file mode bits
			| ST_RDONLY; // Read only file system
	// Maximum filename length.
	buf->f_namemax = 208;
	return 0;
}

static const devoptab_t dotab_iso9660 = 
{
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
	_ISO9660_statvfs_r,
	NULL,               // device ftruncate_r
	NULL,           // device fsync_r
	NULL       	/* Device data */
};

static PATH_ENTRY* entry_from_index(PATH_ENTRY *entry, u16 index)
{
	u32 i;

	if (entry->index == index)
		return entry;

	for (i = 0; i < entry->childCount; i++)
	{
		PATH_ENTRY *match = entry_from_index(&entry->children[i], index);
		if (match)
			return match;
	}
	return NULL;
}

static PATH_ENTRY* add_child_entry(PATH_ENTRY *dir)
{
	PATH_ENTRY *child;
	PATH_ENTRY *newChildren = NULL;

	newChildren = realloc(dir->children, (dir->childCount + 1) * sizeof(PATH_ENTRY));
	if (newChildren == NULL)
		return NULL;

	memset(newChildren + dir->childCount, 0, sizeof(PATH_ENTRY));
	dir->children = newChildren;

	child = &dir->children[dir->childCount++];
	return child;
}

static void cleanup_recursive(PATH_ENTRY *entry)
{
	u32 i;

	for (i = 0; i < entry->childCount; i++)
		cleanup_recursive(&entry->children[i]);
	if (entry->children)
		free(entry->children);
}

static struct pvd_s* read_volume_descriptor(u8 descriptor)
{
	u8 sector;
	const DISC_INTERFACE *disc = get_interface();

	for (sector = 16; sector < 32; sector++)
	{
		if (!disc->readSectors(sector, 1, read_buffer))
			return NULL;
		if (!memcmp(read_buffer + 1, "CD001\1", 6))
		{
			if (*read_buffer == descriptor)
				return (struct pvd_s*) read_buffer;
			else if (*read_buffer == 0xff)
				return NULL;
		}
	}

	return NULL;
}

static BOOL read_directories()
{
	struct pvd_s *volume = read_volume_descriptor(2);
	if (volume)
		iso_unicode = TRUE;
	else if (!(volume = read_volume_descriptor(1)))
		return FALSE;

	if (!(iso_rootentry = malloc(sizeof(PATH_ENTRY))))
		return FALSE;
	memset(iso_rootentry, 0, sizeof(PATH_ENTRY));
	iso_rootentry->table_entry.name_length = 1;
	iso_rootentry->table_entry.extended_sectors = volume->root[OFFSET_EXTENDED];
	iso_rootentry->table_entry.sector = *(u32 *) (volume->root + OFFSET_SECTOR);
	iso_rootentry->table_entry.parent = 0;
	iso_rootentry->table_entry.name[0] = '\x00';
	iso_rootentry->index = 1;
	iso_currententry = iso_rootentry;

	u32 path_table = volume->path_table_be;
	u32 path_table_len = volume->path_table_len_be;
	u16 i = 1;
	u64 offset = sizeof(PATHTABLE_ENTRY) - ISO_MAXPATHLEN + 2;
	PATH_ENTRY *parent = iso_rootentry;
	while (i < 0xffff && offset < path_table_len)
	{
		PATHTABLE_ENTRY entry;
		if (_read(&entry, (u64) path_table * SECTOR_SIZE + offset, sizeof(PATHTABLE_ENTRY)) != sizeof(PATHTABLE_ENTRY))
			return FALSE; // kinda dodgy - could be reading too far
		if (parent->index != entry.parent)
			parent = entry_from_index(iso_rootentry, entry.parent);
		if (!parent)
			return FALSE;
		PATH_ENTRY *child = add_child_entry(parent);
		if (!child)
			return FALSE;
		memcpy(&child->table_entry, &entry, sizeof(PATHTABLE_ENTRY));
		offset += sizeof(PATHTABLE_ENTRY) - ISO_MAXPATHLEN + child->table_entry.name_length;
		if (child->table_entry.name_length % 2)
			offset++;
		child->index = ++i;

		if (iso_unicode)
		{
			u32 i;
			for (i = 0; i < (child->table_entry.name_length / 2); i++)
				child->table_entry.name[i] = entry.name[i * 2 + 1];
			child->table_entry.name[i] = '\x00';
			child->table_entry.name_length = i;
		}
		else
		{
			child->table_entry.name[child->table_entry.name_length] = '\x00';
		}

	}

	return TRUE;
}

BOOL ISO9660_Mount()
{
	BOOL success = FALSE;
	const DISC_INTERFACE *disc = get_interface();

	ISO9660_Unmount();

	if (disc->startup())
	{
		success = (read_directories() && (dotab_device = AddDevice(&dotab_iso9660)) >= 0);
		if (success)
			iso_last_access = gettime();
		else
			ISO9660_Unmount();
	}

	return success;
}

BOOL ISO9660_Unmount()
{
	if (iso_rootentry)
	{
		cleanup_recursive(iso_rootentry);
		free(iso_rootentry);
		iso_rootentry = NULL;
	}

	iso_last_access = 0;
	iso_unicode = FALSE;
	iso_currententry = iso_rootentry;
	cache_sectors = 0;
	if (dotab_device >= 0)
	{
		dotab_device = -1;
		return !RemoveDevice(DEVICE_NAME ":");
	}

	return TRUE;
}

u64 ISO9660_LastAccess()
{
	return iso_last_access;
}
