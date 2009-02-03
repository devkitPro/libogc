/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * SMB devoptab
 ****************************************************************************/

#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/iosupport.h>
#include <network.h>
#include <ogcsys.h>
#include <smb.h>

#define SMB_MAXPATH					4096
#define SMB_SRCH_ARCHIVE			32

static char currentpath[SMB_MAXPATH];
static bool first_item_dir = false;
static bool diropen_root=false;

static mutex_t _SMB_mutex;

static inline void _SMB_lock()
{
	LWP_MutexLock(_SMB_mutex);
}

static inline void _SMB_unlock()
{
	LWP_MutexUnlock(_SMB_mutex);
}

typedef struct
{
	SMBFILE handle;
	off_t offset;
	off_t len;
	char filename[SMB_MAXPATH];
} SMBFILESTRUCT;

typedef struct
{
	SMBDIRENTRY smbdir;
} SMBDIRSTATESTRUCT;

//globals
static SMBCONN smbconn;
static u8 SMBCONNECTED = false;

///////////////////////////////////////////
//      CACHE FUNCTION DEFINITIONS       //
///////////////////////////////////////////
#define SMB_CACHE_FREE 0xFFFFFFFF
#define SMB_READ_BUFFERSIZE		7236
#define SMB_WRITE_BUFFERSIZE	7236

typedef struct
{
	off_t offset;
	u32 last_used;
	SMBFILESTRUCT *file;
	void *ptr;
} smb_cache_page;

typedef struct
{

	off_t used;
	off_t len;
	SMBFILESTRUCT *file;
	void *ptr;
} smb_write_cache;

static smb_write_cache SMBWriteCache;
static smb_cache_page *SMBReadAheadCache = NULL;
static u32 SMB_RA_pages = 0;

u32 gettick();

void DestroySMBReadAheadCache();
void SMBEnableReadAhead(u32 pages);
int ReadSMBFromCache(void *buf, int len, SMBFILESTRUCT *file);

static lwp_t main_thread;
static bool end_cache_thread = false;
///////////////////////////////////////////
//    END CACHE FUNCTION DEFINITIONS     //
///////////////////////////////////////////

///////////////////////////////////////////
//         CACHE FUNCTIONS              //
///////////////////////////////////////////

int FlushWriteSMBCache()
{
	_SMB_lock();
	if (SMBWriteCache.file == NULL || SMBWriteCache.len == 0)
	{
		_SMB_unlock();
		return 0;
	}

	int written = 0;

	written = SMB_WriteFile(SMBWriteCache.ptr, SMBWriteCache.len,
			SMBWriteCache.file->offset, SMBWriteCache.file->handle);

	if (written <= 0)
	{
		SMBWriteCache.used = 0;
		SMBWriteCache.len = 0;
		SMBWriteCache.file = NULL;
		_SMB_unlock();
		return -1;
	}
	SMBWriteCache.file->offset += written;
	if (SMBWriteCache.file->offset > SMBWriteCache.file->len)
		SMBWriteCache.file->len = SMBWriteCache.file->offset;
	SMBWriteCache.used = 0;
	SMBWriteCache.len = 0;
	SMBWriteCache.file = NULL;
	_SMB_unlock();
	return 0;
}

void DestroySMBReadAheadCache()
{
	int i;
	if (SMBReadAheadCache == NULL)
	{
		SMBWriteCache.used = 0;
		SMBWriteCache.len = 0;
		SMBWriteCache.file = NULL;
		SMBWriteCache.ptr = NULL;
		return;
	}
	for (i = 0; i < SMB_RA_pages; i++)
	{
		free(SMBReadAheadCache[i].ptr);
	}
	free(SMBReadAheadCache);
	SMBReadAheadCache = NULL;
	SMB_RA_pages = 0;

	end_cache_thread = true;
	FlushWriteSMBCache();

	free(SMBWriteCache.ptr);
	SMBWriteCache.used = 0;
	SMBWriteCache.len = 0;
	SMBWriteCache.file = NULL;
	SMBWriteCache.ptr = NULL;

}

#define ticks_to_msecs(ticks)		((u32)((u64)(ticks)/(u64)(TB_TIMER_CLOCK)))
static void *process_cache_thread(void *ptr)
{
	while (1)
	{
		_SMB_lock();
		if (SMBWriteCache.used > 0)
		{
			if (ticks_to_msecs(gettick()-SMBWriteCache.used) > 400)
			{
				FlushWriteSMBCache();
			}
		}
		_SMB_unlock();
		usleep(800);
		if (end_cache_thread)
			break;
	}

	LWP_JoinThread(main_thread, NULL);
	return NULL;
}

void SMBEnableReadAhead(u32 pages)
{
	int i, j;

	DestroySMBReadAheadCache();

	if (pages == 0)
		return;

	//only 1 page for write
	SMBWriteCache.ptr = memalign(32, SMB_WRITE_BUFFERSIZE);
	SMBWriteCache.used = 0;
	SMBWriteCache.len = 0;
	SMBWriteCache.file = NULL;

	SMB_RA_pages = pages;
	SMBReadAheadCache = (smb_cache_page *) malloc(sizeof(smb_cache_page) * SMB_RA_pages);
	if (SMBReadAheadCache == NULL)
		return;
	for (i = 0; i < SMB_RA_pages; i++)
	{
		SMBReadAheadCache[i].offset = SMB_CACHE_FREE;
		SMBReadAheadCache[i].last_used = 0;
		SMBReadAheadCache[i].file = NULL;
		SMBReadAheadCache[i].ptr = memalign(32, SMB_READ_BUFFERSIZE);
		if (SMBReadAheadCache[i].ptr == NULL)
		{
			for (j = i - 1; j >= 0; j--)
				if (SMBReadAheadCache[j].ptr)
					free(SMBReadAheadCache[j].ptr);
			free(SMBReadAheadCache);
			SMBReadAheadCache = NULL;
			free(SMBWriteCache.ptr);
			return;
		}
		memset(SMBReadAheadCache[i].ptr, 0, SMB_READ_BUFFERSIZE);
	}
	lwp_t client_thread;
	main_thread = LWP_GetSelf();
	end_cache_thread = false;
	LWP_CreateThread(&client_thread, process_cache_thread, NULL, NULL, 0, 80);

}

// clear cache from file (clear if you write to the file)
void ClearSMBFileCache(SMBFILESTRUCT *file)
{
	int i;
	for (i = 0; i < SMB_RA_pages; i++)
	{
		if (SMBReadAheadCache[i].offset != SMB_CACHE_FREE)
		{
			if (strcmp(SMBReadAheadCache[i].file->filename, file->filename)==0)
			{
				SMBReadAheadCache[i].offset = SMB_CACHE_FREE;
				SMBReadAheadCache[i].last_used = 0;
				SMBReadAheadCache[i].file = NULL;
			}
		}
	}
}

int ReadSMBFromCache(void *buf, int len, SMBFILESTRUCT *file)
{
	int retval;
	int i, leastUsed, rest;
	u32 new_offset;
	if (SMBReadAheadCache == NULL)
	{
		_SMB_lock();
		if (SMB_ReadFile(buf, len, file->offset, file->handle) <= 0)
		{
			_SMB_unlock();
			return -1;
		}
		_SMB_unlock();
		return 0;
	}
	new_offset = file->offset;
	rest = len;
	leastUsed = 0;
	for (i = 0; i < SMB_RA_pages; i++)
	{
		if (SMBReadAheadCache[i].file == file)
		{
			if ((file->offset >= SMBReadAheadCache[i].offset) &&
				(file->offset < (SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE)))
			{
				if ((file->offset + len) <= (SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE))
				{
					SMBReadAheadCache[i].last_used = gettick();
					memcpy(buf, SMBReadAheadCache[i].ptr + (file->offset - SMBReadAheadCache[i].offset), len);
					return 0;
				}
				else
				{
					int buffer_used;
					SMBReadAheadCache[i].last_used = gettick();
					buffer_used = (SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE) - file->offset;
					memcpy(buf, SMBReadAheadCache[i].ptr + (file->offset - SMBReadAheadCache[i].offset), buffer_used);
					buf += buffer_used;
					rest = len - buffer_used;
					new_offset = SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE;
					i++;
					break;
				}

			}
		}
		if ((SMBReadAheadCache[i].last_used < SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;
	}

	for (; i < SMB_RA_pages; i++)
	{
		if ((SMBReadAheadCache[i].last_used < SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;
	}
	_SMB_lock();
	retval = SMB_ReadFile(SMBReadAheadCache[leastUsed].ptr, SMB_READ_BUFFERSIZE, new_offset, file->handle);
	_SMB_unlock();
	if (retval <= 0)
	{
		SMBReadAheadCache[leastUsed].offset = SMB_CACHE_FREE;
		SMBReadAheadCache[leastUsed].last_used = 0;
		SMBReadAheadCache[leastUsed].file = NULL;
		return -1;
	}

	SMBReadAheadCache[leastUsed].offset = new_offset;
	SMBReadAheadCache[leastUsed].last_used = gettick();
	SMBReadAheadCache[leastUsed].file = file;
	memcpy(buf, SMBReadAheadCache[leastUsed].ptr, rest);

	return 0;
}

int WriteSMBUsingCache(const char *buf, int len, SMBFILESTRUCT *file)
{
	if (file == NULL || buf == NULL)
		return -1;

	int ret = len;
	_SMB_lock();
	if (SMBWriteCache.file != NULL)
	{
		if (strcmp(SMBWriteCache.file->filename, file->filename) != 0)
		{
			//Flush current buffer
			if (FlushWriteSMBCache() < 0)
			{
				_SMB_unlock();
				return -1;
			}
		}
	}
	SMBWriteCache.file = file;

	if (SMBWriteCache.len + len >= SMB_WRITE_BUFFERSIZE)
	{
		void *send_buf;
		int rest = 0, written = 0;
		send_buf = memalign(32, SMB_WRITE_BUFFERSIZE);
		if (SMBWriteCache.len > 0)
			memcpy(send_buf, SMBWriteCache.ptr, SMBWriteCache.len);
loop:
		rest = SMB_WRITE_BUFFERSIZE - SMBWriteCache.len;
		memcpy(send_buf + SMBWriteCache.len, buf, rest);
		written = SMB_WriteFile(send_buf, SMB_WRITE_BUFFERSIZE,
				SMBWriteCache.file->offset, SMBWriteCache.file->handle);
		free(send_buf);
		if (written <= 0)
		{
			SMBWriteCache.used = 0;
			SMBWriteCache.len = 0;
			SMBWriteCache.file = NULL;
			_SMB_unlock();
			return -1;
		}
		file->offset += written;
		if (file->offset > file->len)
			file->len = file->offset;

		buf = buf + rest;
		len = SMBWriteCache.len + len - SMB_WRITE_BUFFERSIZE;

		SMBWriteCache.used = gettick();
		SMBWriteCache.len = 0;

		if(len>=SMB_WRITE_BUFFERSIZE) goto loop;
	}
	if (len > 0)
	{
		memcpy(SMBWriteCache.ptr + SMBWriteCache.len, buf, len);
		SMBWriteCache.len += len;
	}
	_SMB_unlock();
	return ret;

}

///////////////////////////////////////////
//         END CACHE FUNCTIONS           //
///////////////////////////////////////////

static char *smb_absolute_path_no_device(const char *srcpath, char *destpath)
{
	if (strchr(srcpath, ':') != NULL)
	{
		srcpath = strchr(srcpath, ':') + 1;
	}
	if (strchr(srcpath, ':') != NULL)
	{
		return NULL;
	}

	if (srcpath[0] != '\\' && srcpath[0] != '/')
	{
		strcpy(destpath, currentpath);
		strcat(destpath, srcpath);
	}
	else
	{
		strcpy(destpath, srcpath);
	}
	int i, l;
	l = strlen(destpath);
	for (i = 0; i < l; i++)
		if (destpath[i] == '/')
			destpath[i] = '\\';

	return destpath;
}

//FILE IO
static int __smb_open(struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fileStruct;

	if (!SMBCONNECTED)
	{
		r->_errno = ENODEV;
		return -1;
	}

	char fixedpath[SMB_MAXPATH];
	if (smb_absolute_path_no_device(path, fixedpath) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	SMBDIRENTRY dentry;
	bool fileExists = true;
	_SMB_lock();
	if (SMB_PathInfo(fixedpath, &dentry, smbconn) != SMB_SUCCESS)
		fileExists = false;

	_SMB_unlock();

	// Determine which mode the file is open for
	u8 smb_mode;
	unsigned short access;
	if ((flags & 0x03) == O_RDONLY)
	{
		// Open the file for read-only access
		smb_mode = SMB_OF_OPEN;
		access = SMB_OPEN_READING;
	}
	else if ((flags & 0x03) == O_WRONLY)
	{
		// Open file for write only access
		if (fileExists)
			smb_mode = SMB_OF_OPEN;
		else
			smb_mode = SMB_OF_CREATE;
		access = SMB_OPEN_WRITING;
	}
	else if ((flags & 0x03) == O_RDWR)
	{
		// Open file for read/write access
		access = SMB_OPEN_READWRITE;
		if (fileExists)
			smb_mode = SMB_OF_OPEN;
		else
			smb_mode = SMB_OF_CREATE;
	}
	else
	{
		r->_errno = EACCES;
		return -1;
	}

	if ((flags & O_CREAT) && !fileExists)
		smb_mode = SMB_OF_CREATE;
	if (!(flags & O_APPEND) && fileExists && ((flags & 0x03) != O_RDONLY))
		smb_mode = SMB_OF_TRUNCATE;

	_SMB_lock();
	file->handle = SMB_OpenFile(fixedpath, access, smb_mode, smbconn);
	_SMB_unlock();
	if (!file->handle)
	{
		r->_errno = ENOENT;
		return -1;
	}

	file->len = 0;
	if (fileExists)
		file->len = ((off_t)dentry.size_high) << 32 | dentry.size_low;

	if (flags & O_APPEND)
		file->offset = file->len;
	else
		file->offset = 0;

	strcpy(file->filename, fixedpath);
	return 0;
}

static off_t __smb_seek(struct _reent *r, int fd, off_t pos, int dir)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;
	off_t position;

	if (file == NULL)
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
		position = file->len + pos;
		break;
	default:
		r->_errno = EINVAL;
		return -1;
	}

	if (((pos > 0) && (position < 0)) || (position > file->len))
	{
		r->_errno = EOVERFLOW;
		return -1;
	}
	if (position < 0)
	{
		r->_errno = EINVAL;
		return -1;
	}

	// Save position
	file->offset = position;
	return position;
}

static ssize_t __smb_read(struct _reent *r, int fd, char *ptr, size_t len)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;

	if (file == NULL)
	{
		r->_errno = EBADF;
		return -1;
	}

	// Don't try to read if the read pointer is past the end of file
	if (file->offset > file->len)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	// Don't read past end of file
	if (len + file->offset > file->len)
	{
		len = file->len - file->offset;
		if (len < 0)
			len = 0;
		r->_errno = EOVERFLOW;

		if (len == 0)
			return 0;
	}

	// Short circuit cases where len is 0 (or less)
	if (len <= 0)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	if (ReadSMBFromCache(ptr, len, file) < 0)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}
	file->offset += len;

	return len;

}

static ssize_t __smb_write(struct _reent *r, int fd, const char *ptr, size_t len)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;
	int written;
	if (file == NULL)
	{
		r->_errno = EBADF;
		return -1;
	}

	// Don't try to write if the pointer is past the end of file
	if (file->offset > file->len)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	// Short circuit cases where len is 0 (or less)
	if (len <= 0)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	ClearSMBFileCache(file);
	written = WriteSMBUsingCache(ptr, len, file);
	if (written <= 0)
	{
		r->_errno = EIO;
		return -1;
	}

	return written;
}

static int __smb_close(struct _reent *r, int fd)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;
	if (SMBWriteCache.file == file)
	{
		FlushWriteSMBCache();
	}
	_SMB_lock();
	SMB_CloseFile(file->handle);
	_SMB_unlock();
	file->len = 0;
	file->offset = 0;
	file->filename[0] = '\0';

	return 0;
}

static int __smb_chdir(struct _reent *r, const char *path)
{
	char path_absolute[SMB_MAXPATH];
	SMBDIRENTRY dentry;
	int found;

	if (smb_absolute_path_no_device(path, path_absolute) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	memset(&dentry, 0, sizeof(SMBDIRENTRY));

	_SMB_lock();
	found = SMB_PathInfo(path_absolute, &dentry, smbconn);
	_SMB_unlock();

	if (found != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		return -1;
	}

	if (!(dentry.attributes & SMB_SRCH_DIRECTORY))
	{
		r->_errno = ENOTDIR;
		return -1;
	}

	strcpy(currentpath, path_absolute);
	if (currentpath[0] != 0)
	{
		if (currentpath[strlen(currentpath) - 1] != '\\')
			strcat(currentpath, "\\");
	}
	return 0;
}

static int __smb_dirreset(struct _reent *r, DIR_ITER *dirState)
{
	char path_abs[SMB_MAXPATH];
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	memset(&dentry, 0, sizeof(SMBDIRENTRY));

	_SMB_lock();
	SMB_FindClose(smbconn);

	strcpy(path_abs,currentpath);
	strcat(path_abs,"*");
	int found = SMB_FindFirst(path_abs, SMB_SRCH_DIRECTORY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN | SMB_SRCH_READONLY | SMB_SRCH_ARCHIVE, &dentry, smbconn);
	_SMB_unlock();

	if (found != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		return -1;
	}

	if (!(dentry.attributes & SMB_SRCH_DIRECTORY))
	{
		r->_errno = ENOTDIR;
		return -1;
	}

	state->smbdir.size_low = dentry.size_low;
	state->smbdir.size_high = dentry.size_high;
	state->smbdir.attributes = dentry.attributes;
	strcpy(state->smbdir.name, dentry.name);

	first_item_dir = true;
	return 0;

}

static DIR_ITER* __smb_diropen(struct _reent *r, DIR_ITER *dirState, const char *path)
{
	char path_absolute[SMB_MAXPATH];
	int found;
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	if (smb_absolute_path_no_device(path, path_absolute) == NULL)
	{
		r->_errno = EINVAL;
		return NULL;
	}

	if (path_absolute[strlen(path_absolute) - 1] != '\\')
		strcat(path_absolute, "\\");

	if(!strcmp(path_absolute,"\\"))
		diropen_root=true;
	else
		diropen_root=false;

	strcat(path_absolute, "*");

	memset(&dentry, 0, sizeof(SMBDIRENTRY));

	_SMB_lock();
	found = SMB_FindFirst(path_absolute, SMB_SRCH_DIRECTORY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN | SMB_SRCH_READONLY | SMB_SRCH_ARCHIVE, &dentry, smbconn);
	_SMB_unlock();

	if (found != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		return NULL;
	}

	if (!(dentry.attributes & SMB_SRCH_DIRECTORY))
	{
		r->_errno = ENOTDIR;
		return NULL;
	}
	state->smbdir.size_low = dentry.size_low;
	state->smbdir.size_high = dentry.size_high;
	state->smbdir.attributes = dentry.attributes;
	strcpy(state->smbdir.name, dentry.name);
	first_item_dir = true;

	return dirState;
}

static int dentry_to_stat(SMBDIRENTRY *dentry, struct stat *st)
{
	if (!st)
		return -1;
	if (!dentry)
		return -1;

	st->st_dev = 0;
	st->st_ino = 0;

	st->st_mode = ((dentry->attributes & SMB_SRCH_DIRECTORY) ? S_IFDIR
			: S_IFREG);
	st->st_nlink = 1;
	st->st_uid = 1; // Faked
	st->st_rdev = st->st_dev;
	st->st_gid = 2; // Faked
	st->st_size = ((off_t)dentry->size_high) << 32 | dentry->size_low;
	st->st_atime = 0;//FIXME
	st->st_spare1 = 0;
	st->st_mtime = 0;//FIXME
	st->st_spare2 = 0;
	st->st_ctime = 0;//FIXME
	st->st_spare3 = 0;
	st->st_blksize = 1024;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize; // File size in blocks
	st->st_spare4[0] = 0;
	st->st_spare4[1] = 0;

	return 0;
}
static int __smb_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename,
		struct stat *filestat)
{
	int ret;
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	if (currentpath[0] == '\0' || filestat == NULL)
	{
		r->_errno = ENOENT;
		return -1;
	}

	memset(&dentry, 0, sizeof(SMBDIRENTRY));
	if (first_item_dir)
	{
		first_item_dir = false;
		dentry.size_low = 0;
		dentry.size_high = 0;
		dentry.attributes = SMB_SRCH_DIRECTORY;
		strcpy(dentry.name, ".");

		state->smbdir.size_low = dentry.size_low;
		state->smbdir.size_high = dentry.size_high;
		state->smbdir.attributes = dentry.attributes;
		strcpy(state->smbdir.name, dentry.name);
		strcpy(filename, dentry.name);
		dentry_to_stat(&dentry, filestat);

		return 0;
	}

	_SMB_lock();
	ret = SMB_FindNext(&dentry, smbconn);
	if(ret==SMB_SUCCESS && diropen_root && !strcmp(dentry.name,".."))
		ret = SMB_FindNext(&dentry, smbconn);
	_SMB_unlock();

	if (ret == SMB_SUCCESS)
	{
		state->smbdir.size_low = dentry.size_low;
		state->smbdir.size_high = dentry.size_high;
		state->smbdir.attributes = dentry.attributes;
		strcpy(state->smbdir.name, dentry.name);
	}
	else
	{
		r->_errno = ENOENT;
		return -1;
	}

	strcpy(filename, dentry.name);

	dentry_to_stat(&dentry, filestat);

	return 0;
}

static int __smb_dirclose(struct _reent *r, DIR_ITER *dirState)
{
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);

	_SMB_lock();
	SMB_FindClose(smbconn);
	_SMB_unlock();

	memset(state, 0, sizeof(SMBDIRSTATESTRUCT));
	return 0;
}

static int __smb_stat(struct _reent *r, const char *path, struct stat *st)
{
	char path_absolute[SMB_MAXPATH];
	SMBDIRENTRY dentry;

	if (smb_absolute_path_no_device(path, path_absolute) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	_SMB_lock();
	if (SMB_PathInfo(path_absolute, &dentry, smbconn) != SMB_SUCCESS)
	{
		_SMB_unlock();
		r->_errno = ENOENT;
		return -1;
	}
	_SMB_unlock();

	if (dentry.name[0] == '\0')
	{
		r->_errno = ENOENT;
		return -1;
	}

	dentry_to_stat(&dentry, st);

	return 0;
}

static int __smb_fstat(struct _reent *r, int fd, struct stat *st)
{

	SMBFILESTRUCT *filestate = (SMBFILESTRUCT *) fd;

	if (!filestate)
	{
		r->_errno = EBADF;
		return -1;
	}

	st->st_size = filestate->len;

	return 0;
}

const devoptab_t dotab_smb =
{
		"smb", // device name
		sizeof(SMBFILESTRUCT), // size of file structure
		__smb_open, // device open
		__smb_close, // device close
		__smb_write, // device write
		__smb_read, // device read
		__smb_seek, // device seek
		__smb_fstat, // device fstat
		__smb_stat, // device stat
		NULL, // device link
		NULL, // device unlink
		__smb_chdir, // device chdir
		NULL, // device rename
		NULL, // device mkdir

		sizeof(SMBDIRSTATESTRUCT), // dirStateSize
		__smb_diropen, // device diropen_r
		__smb_dirreset, // device dirreset_r
		__smb_dirnext, // device dirnext_r
		__smb_dirclose, // device dirclose_r
		NULL,			// device statvfs_r
		NULL,               // device ftruncate_r
		NULL,           // device fsync_r
		NULL       	/* Device data */
};

bool smbInit(const char *user, const char *password, const char *share,	const char *ip)
{
	char myIP[16];
	if (if_config(myIP, NULL, NULL, true) < 0)
		return false;

	LWP_MutexInit(&_SMB_mutex, false);

	//root connect
	_SMB_lock();
	if (SMB_Connect(&smbconn, user, password, share, ip) != SMB_SUCCESS)
	{
		_SMB_unlock();
		LWP_MutexDestroy(_SMB_mutex);
		return false;
	}
	_SMB_unlock();
	SMBCONNECTED = true;

	AddDevice(&dotab_smb);

	SMBEnableReadAhead(32);

	currentpath[0] = '\\';
	currentpath[1] = '\0';
	return true;
}

void smbClose()
{
	_SMB_lock();
	SMB_Close(smbconn);
	_SMB_unlock();
	LWP_MutexDestroy(_SMB_mutex);
}
