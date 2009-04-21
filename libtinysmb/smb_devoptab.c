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
#include <ogc/lwp_watchdog.h>
#include <ogc/mutex.h>

#include "smb.h"

static mutex_t _SMB_mutex=LWP_MUTEX_NULL;

static inline void _SMB_lock()
{
	if(_SMB_mutex!=LWP_MUTEX_NULL) LWP_MutexLock(_SMB_mutex);
}

static inline void _SMB_unlock()
{
	if(_SMB_mutex!=LWP_MUTEX_NULL) LWP_MutexUnlock(_SMB_mutex);
}

typedef struct
{
	SMBFILE handle;
	off_t offset;
	off_t len;
	char filename[SMB_MAXPATH];
	unsigned short access;
	int env;
} SMBFILESTRUCT;

typedef struct
{
	SMBDIRENTRY smbdir;
	int env;
} SMBDIRSTATESTRUCT;

static bool FirstInit=true;
#define MAX_SMB_MOUNTED 5

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

void DestroySMBReadAheadCache(char *name);
void SMBEnableReadAhead(char *name, u32 pages);
int ReadSMBFromCache(void *buf, int len, SMBFILESTRUCT *file);

static lwp_t main_thread;
static bool end_cache_thread = true;
///////////////////////////////////////////
//    END CACHE FUNCTION DEFINITIONS     //
///////////////////////////////////////////

// SMB Enviroment
typedef struct
{
	char *name;
	int pos;
	devoptab_t *devoptab;

	SMBCONN smbconn;
	u8 SMBCONNECTED ;

	char currentpath[SMB_MAXPATH];
	bool first_item_dir ;
	bool diropen_root;

	smb_write_cache SMBWriteCache;
	smb_cache_page *SMBReadAheadCache;
	u32 SMB_RA_pages;

} smb_env;

static smb_env SMBEnv[MAX_SMB_MOUNTED];


///////////////////////////////////////////
//         CACHE FUNCTIONS              //
///////////////////////////////////////////

smb_env* FindSMBEnv(const char *name)
{
	int i;

	for(i=0;i<MAX_SMB_MOUNTED ;i++)
	{
		if(SMBEnv[i].SMBCONNECTED && strcmp(name,SMBEnv[i].name)==0)
		{
			return &SMBEnv[i];
		}
	}
	return NULL;
}

int FlushWriteSMBCache(char *name)
{
	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return -1;
	_SMB_lock();
	if (env->SMBWriteCache.file == NULL || env->SMBWriteCache.len == 0)
	{
		_SMB_unlock();
		return 0;
	}

	int written = 0;

	written = SMB_WriteFile(env->SMBWriteCache.ptr, env->SMBWriteCache.len,
			env->SMBWriteCache.file->offset, env->SMBWriteCache.file->handle);

	if (written <= 0)
	{
		env->SMBWriteCache.used = 0;
		env->SMBWriteCache.len = 0;
		env->SMBWriteCache.file = NULL;
		_SMB_unlock();
		return -1;
	}
	env->SMBWriteCache.file->offset += written;
	if (env->SMBWriteCache.file->offset > env->SMBWriteCache.file->len)
		env->SMBWriteCache.file->len = env->SMBWriteCache.file->offset;
	env->SMBWriteCache.used = 0;
	env->SMBWriteCache.len = 0;
	env->SMBWriteCache.file = NULL;
	_SMB_unlock();
	return 0;
}

void DestroySMBReadAheadCache(char *name)
{
	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return ;

	int i;
	if (env->SMBReadAheadCache != NULL)
	{
		for (i = 0; i < env->SMB_RA_pages; i++)
		{
			if(env->SMBReadAheadCache[i].ptr)
				free(env->SMBReadAheadCache[i].ptr);
		}
		free(env->SMBReadAheadCache);
		env->SMBReadAheadCache = NULL;
		env->SMB_RA_pages = 0;

		//end_cache_thread = true;
	}
	FlushWriteSMBCache(env->name);

	if(env->SMBWriteCache.ptr)
		free(env->SMBWriteCache.ptr);

	env->SMBWriteCache.used = 0;
	env->SMBWriteCache.len = 0;
	env->SMBWriteCache.file = NULL;
	env->SMBWriteCache.ptr = NULL;
}

static void *process_cache_thread(void *ptr)
{
	int i;
	while (1)
	{
		//_SMB_lock();
		for(i=0;i<MAX_SMB_MOUNTED ;i++)
		{
			if(SMBEnv[i].SMBCONNECTED)
			{
				if (SMBEnv[i].SMBWriteCache.used > 0)
				{
					if (ticks_to_millisecs(gettime())-ticks_to_millisecs(SMBEnv[i].SMBWriteCache.used) > 500)
					{
						FlushWriteSMBCache(SMBEnv[i].name);
					}
				}
			}
		}
		//_SMB_unlock();
		usleep(10000);
		if (end_cache_thread) break;
	}

	LWP_JoinThread(main_thread, NULL);
	return NULL;
}

void SMBEnableReadAhead(char *name, u32 pages)
{
	int i, j;

	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return;

	DestroySMBReadAheadCache(name);

	if (pages == 0)
		return;

	//only 1 page for write
	env->SMBWriteCache.ptr = memalign(32, SMB_WRITE_BUFFERSIZE);
	env->SMBWriteCache.used = 0;
	env->SMBWriteCache.len = 0;
	env->SMBWriteCache.file = NULL;

	env->SMB_RA_pages = pages;
	env->SMBReadAheadCache = (smb_cache_page *) malloc(sizeof(smb_cache_page) * env->SMB_RA_pages);
	if (env->SMBReadAheadCache == NULL)
		return;
	for (i = 0; i < env->SMB_RA_pages; i++)
	{
		env->SMBReadAheadCache[i].offset = SMB_CACHE_FREE;
		env->SMBReadAheadCache[i].last_used = 0;
		env->SMBReadAheadCache[i].file = NULL;
		env->SMBReadAheadCache[i].ptr = memalign(32, SMB_READ_BUFFERSIZE);
		if (env->SMBReadAheadCache[i].ptr == NULL)
		{
			for (j = i - 1; j >= 0; j--)
				if (env->SMBReadAheadCache[j].ptr)
					free(env->SMBReadAheadCache[j].ptr);
			free(env->SMBReadAheadCache);
			env->SMBReadAheadCache = NULL;
			free(env->SMBWriteCache.ptr);
			return;
		}
		memset(env->SMBReadAheadCache[i].ptr, 0, SMB_READ_BUFFERSIZE);
	}

}

// clear cache from file (clear if you write to the file)
void ClearSMBFileCache(SMBFILESTRUCT *file)
{
	int i,j;
	j=file->env;
	for (i = 0; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if (SMBEnv[j].SMBReadAheadCache[i].offset != SMB_CACHE_FREE)
		{
			if (strcmp(SMBEnv[j].SMBReadAheadCache[i].file->filename, file->filename)==0)
			{
				SMBEnv[j].SMBReadAheadCache[i].offset = SMB_CACHE_FREE;
				SMBEnv[j].SMBReadAheadCache[i].last_used = 0;
				SMBEnv[j].SMBReadAheadCache[i].file = NULL;

				memset(SMBEnv[j].SMBReadAheadCache[i].ptr, 0, SMB_READ_BUFFERSIZE);
			}
		}
	}
}

int ReadSMBFromCache(void *buf, int len, SMBFILESTRUCT *file)
{
	int retval;
	int i,j, leastUsed, rest;
	u32 new_offset;
	j=file->env;
	_SMB_lock();
	if (SMBEnv[j].SMBReadAheadCache == NULL)
	{
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
	for (i = 0; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if (SMBEnv[j].SMBReadAheadCache[i].file == file)
		{
			if ((file->offset >= SMBEnv[j].SMBReadAheadCache[i].offset) &&
				(file->offset < (SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE)))
			{
				if ((file->offset + len) <= (SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE))
				{
					SMBEnv[j].SMBReadAheadCache[i].last_used = gettime();
					memcpy(buf, SMBEnv[j].SMBReadAheadCache[i].ptr + (file->offset - SMBEnv[j].SMBReadAheadCache[i].offset), len);
					_SMB_unlock();
					return 0;
				}
				else
				{
					int buffer_used;
					SMBEnv[j].SMBReadAheadCache[i].last_used = gettime();
					buffer_used = (SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE) - file->offset;
					memcpy(buf, SMBEnv[j].SMBReadAheadCache[i].ptr + (file->offset - SMBEnv[j].SMBReadAheadCache[i].offset), buffer_used);
					buf += buffer_used;
					rest = len - buffer_used;
					new_offset = SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE;
					i++;
					break;
				}

			}
		}
		if ((SMBEnv[j].SMBReadAheadCache[i].last_used < SMBEnv[j].SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;
	}

	for (; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if ((SMBEnv[j].SMBReadAheadCache[i].last_used < SMBEnv[j].SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;
	}

	retval = SMB_ReadFile(SMBEnv[j].SMBReadAheadCache[leastUsed].ptr, SMB_READ_BUFFERSIZE, new_offset, file->handle);

	if (retval <= 0)
	{
		SMBEnv[j].SMBReadAheadCache[leastUsed].offset = SMB_CACHE_FREE;
		SMBEnv[j].SMBReadAheadCache[leastUsed].last_used = 0;
		SMBEnv[j].SMBReadAheadCache[leastUsed].file = NULL;
		_SMB_unlock();
		return -1;
	}

	SMBEnv[j].SMBReadAheadCache[leastUsed].offset = new_offset;
	SMBEnv[j].SMBReadAheadCache[leastUsed].last_used = gettime();
	SMBEnv[j].SMBReadAheadCache[leastUsed].file = file;
	memcpy(buf, SMBEnv[j].SMBReadAheadCache[leastUsed].ptr, rest);
	_SMB_unlock();
	return 0;
}

int WriteSMBUsingCache(const char *buf, int len, SMBFILESTRUCT *file)
{
	if (file == NULL || buf == NULL)
		return -1;

	int j,ret = len;
	_SMB_lock();
	j=file->env;
	if (SMBEnv[j].SMBWriteCache.file != NULL)
	{
		if (strcmp(SMBEnv[j].SMBWriteCache.file->filename, file->filename) != 0)
		{
			//Flush current buffer
			if (FlushWriteSMBCache(SMBEnv[j].name) < 0)
			{
				_SMB_unlock();
				return -1;
			}
		}
	}
	SMBEnv[j].SMBWriteCache.file = file;

	if (SMBEnv[j].SMBWriteCache.len + len >= SMB_WRITE_BUFFERSIZE)
	{
		void *send_buf;
		int rest = 0, written = 0;
		send_buf = memalign(32, SMB_WRITE_BUFFERSIZE);
		if (SMBEnv[j].SMBWriteCache.len > 0)
			memcpy(send_buf, SMBEnv[j].SMBWriteCache.ptr, SMBEnv[j].SMBWriteCache.len);
loop:
		rest = SMB_WRITE_BUFFERSIZE - SMBEnv[j].SMBWriteCache.len;
		memcpy(send_buf + SMBEnv[j].SMBWriteCache.len, buf, rest);
		written = SMB_WriteFile(send_buf, SMB_WRITE_BUFFERSIZE,
				SMBEnv[j].SMBWriteCache.file->offset, SMBEnv[j].SMBWriteCache.file->handle);
		free(send_buf);
		if (written <= 0)
		{
			SMBEnv[j].SMBWriteCache.used = 0;
			SMBEnv[j].SMBWriteCache.len = 0;
			SMBEnv[j].SMBWriteCache.file = NULL;
			_SMB_unlock();
			return -1;
		}
		file->offset += written;
		if (file->offset > file->len)
			file->len = file->offset;

		buf = buf + rest;
		len = SMBEnv[j].SMBWriteCache.len + len - SMB_WRITE_BUFFERSIZE;

		SMBEnv[j].SMBWriteCache.used = gettime();
		SMBEnv[j].SMBWriteCache.len = 0;

		if(len>=SMB_WRITE_BUFFERSIZE) goto loop;
	}
	if (len > 0)
	{
		memcpy(SMBEnv[j].SMBWriteCache.ptr + SMBEnv[j].SMBWriteCache.len, buf, len);
		SMBEnv[j].SMBWriteCache.len += len;
	}
	_SMB_unlock();
	return ret;
}

///////////////////////////////////////////
//         END CACHE FUNCTIONS           //
///////////////////////////////////////////

static char *smb_absolute_path_no_device(const char *srcpath, char *destpath, int env)
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
		strcpy(destpath, SMBEnv[env].currentpath);
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

char *ExtractDevice(const char *path, char *device)
{
	int i,l;
	l=strlen(path);

	for(i=0;i<l && path[i]!='\0' && path[i]!=':' && i < 20;i++)
		device[i]=path[i];
	if(path[i]!=':')device[0]='\0';
	else device[i]='\0';
	return device;
}

//FILE IO
static int __smb_open(struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fileStruct;


	char fixedpath[SMB_MAXPATH];

	smb_env *env;

	ExtractDevice(path,fixedpath);
	if(fixedpath[0]=='\0')
	{
		getcwd(fixedpath,SMB_MAXPATH);
		ExtractDevice(fixedpath,fixedpath);
	}
	env=FindSMBEnv(fixedpath);
	file->env=env->pos;

	if (!env->SMBCONNECTED)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(path, fixedpath, file->env) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	SMBDIRENTRY dentry;
	bool fileExists = true;
	_SMB_lock();
	if (SMB_PathInfo(fixedpath, &dentry, env->smbconn) != SMB_SUCCESS)
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
	file->handle = SMB_OpenFile(fixedpath, access, smb_mode, env->smbconn);
	_SMB_unlock();
	if (!file->handle)
	{
		r->_errno = ENOENT;
		return -1;
	}

	file->len = 0;
	if (fileExists)
		file->len = dentry.size;

	if (flags & O_APPEND)
		file->offset = file->len;
	else
		file->offset = 0;

	file->access=access;

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

	int cnt_retry=2;
	retry_read:
	if (ReadSMBFromCache(ptr, len, file) < 0)
	{
		retry_reconnect:
		cnt_retry--;
		if(cnt_retry>=0)
		{
			_SMB_lock();
			if(CheckSMBConnection(SMBEnv[file->env].name))
			{
				ClearSMBFileCache(file);
				file->handle = SMB_OpenFile(file->filename, file->access, SMB_OF_OPEN, SMBEnv[file->env].smbconn);
				if (!file->handle)
				{
					r->_errno = ENOENT;
					_SMB_unlock();
					return -1;
				}
				_SMB_unlock();
				goto retry_read;
			}
			_SMB_unlock();
			usleep(50000);
			goto retry_reconnect;
		}
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
	int j;
	j=file->env;
	_SMB_lock();
	if (SMBEnv[j].SMBWriteCache.file == file)
	{
		FlushWriteSMBCache(SMBEnv[j].name);
	}
	ClearSMBFileCache(file);
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

	ExtractDevice(path,path_absolute);
	if(path_absolute[0]=='\0')
	{
		getcwd(path_absolute,SMB_MAXPATH);
		ExtractDevice(path_absolute,path_absolute);
	}

	smb_env* env;
	env=FindSMBEnv(path_absolute);

	if (smb_absolute_path_no_device(path, path_absolute,env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	memset(&dentry, 0, sizeof(SMBDIRENTRY));

	_SMB_lock();
	found = SMB_PathInfo(path_absolute, &dentry, env->smbconn);
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

	strcpy(env->currentpath, path_absolute);
	if (env->currentpath[0] != 0)
	{
		if (env->currentpath[strlen(env->currentpath) - 1] != '\\')
			strcat(env->currentpath, "\\");
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
	SMB_FindClose(SMBEnv[state->env].smbconn);

	strcpy(path_abs,SMBEnv[state->env].currentpath);
	strcat(path_abs,"*");
	int found = SMB_FindFirst(path_abs, SMB_SRCH_DIRECTORY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN | SMB_SRCH_READONLY | SMB_SRCH_ARCHIVE, &dentry, SMBEnv[state->env].smbconn);
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

	state->smbdir.size = dentry.size;
	state->smbdir.ctime = dentry.ctime;
	state->smbdir.atime = dentry.atime;
	state->smbdir.mtime = dentry.mtime;
	state->smbdir.attributes = dentry.attributes;
	strcpy(state->smbdir.name, dentry.name);

	SMBEnv[state->env].first_item_dir = true;
	return 0;
}

static DIR_ITER* __smb_diropen(struct _reent *r, DIR_ITER *dirState, const char *path)
{
	char path_absolute[SMB_MAXPATH];
	int found;
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	ExtractDevice(path,path_absolute);
	if(path_absolute[0]=='\0')
	{
		getcwd(path_absolute,SMB_MAXPATH);
		ExtractDevice(path_absolute,path_absolute);
	}

	smb_env* env;
	env=FindSMBEnv(path_absolute);
	if (smb_absolute_path_no_device(path, path_absolute, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return NULL;
	}
	if (path_absolute[strlen(path_absolute) - 1] != '\\')
		strcat(path_absolute, "\\");

	if(!strcmp(path_absolute,"\\"))
		env->diropen_root=true;
	else
		env->diropen_root=false;

	strcat(path_absolute, "*");


	memset(&dentry, 0, sizeof(SMBDIRENTRY));
	_SMB_lock();
	found = SMB_FindFirst(path_absolute, SMB_SRCH_DIRECTORY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN | SMB_SRCH_READONLY | SMB_SRCH_ARCHIVE, &dentry, env->smbconn);
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

	state->env=env->pos;
	state->smbdir.size = dentry.size;
	state->smbdir.ctime = dentry.ctime;
	state->smbdir.atime = dentry.atime;
	state->smbdir.mtime = dentry.mtime;
	state->smbdir.attributes = dentry.attributes;
	strcpy(state->smbdir.name, dentry.name);
	env->first_item_dir = true;
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
	st->st_size = dentry->size;
	st->st_atime = dentry->atime/10000000.0 - 11644473600LL;
	st->st_spare1 = 0;
	st->st_mtime = dentry->mtime/10000000.0 - 11644473600LL;
	st->st_spare2 = 0;
	st->st_ctime = dentry->ctime/10000000.0 - 11644473600LL;
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

	if (SMBEnv[state->env].currentpath[0] == '\0' || filestat == NULL)
	{
		r->_errno = ENOENT;
		return -1;
	}

	memset(&dentry, 0, sizeof(SMBDIRENTRY));
	if (SMBEnv[state->env].first_item_dir)
	{
		SMBEnv[state->env].first_item_dir = false;
		dentry.size = 0;
		dentry.attributes = SMB_SRCH_DIRECTORY;
		strcpy(dentry.name, ".");

		state->smbdir.size = dentry.size;
		state->smbdir.ctime = dentry.ctime;
		state->smbdir.atime = dentry.atime;
		state->smbdir.mtime = dentry.mtime;
		state->smbdir.attributes = dentry.attributes;
		strcpy(state->smbdir.name, dentry.name);
		strcpy(filename, dentry.name);
		dentry_to_stat(&dentry, filestat);

		return 0;
	}

	_SMB_lock();
	ret = SMB_FindNext(&dentry, SMBEnv[state->env].smbconn);
	if(ret==SMB_SUCCESS && SMBEnv[state->env].diropen_root && !strcmp(dentry.name,".."))
		ret = SMB_FindNext(&dentry, SMBEnv[state->env].smbconn);
	_SMB_unlock();

	if (ret == SMB_SUCCESS)
	{
		state->smbdir.size = dentry.size;
		state->smbdir.ctime = dentry.ctime;
		state->smbdir.atime = dentry.atime;
		state->smbdir.mtime = dentry.mtime;
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
	SMB_FindClose(SMBEnv[state->env].smbconn);
	_SMB_unlock();

	memset(state, 0, sizeof(SMBDIRSTATESTRUCT));
	return 0;
}

static int __smb_stat(struct _reent *r, const char *path, struct stat *st)
{
	char path_absolute[SMB_MAXPATH];
	SMBDIRENTRY dentry;

	ExtractDevice(path,path_absolute);
	if(path_absolute[0]=='\0')
	{
		getcwd(path_absolute,SMB_MAXPATH);
		ExtractDevice(path_absolute,path_absolute);
	}

	smb_env* env;
	env=FindSMBEnv(path_absolute);

	if (smb_absolute_path_no_device(path, path_absolute, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}
	_SMB_lock();
	if (SMB_PathInfo(path_absolute, &dentry, env->smbconn) != SMB_SUCCESS)
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

void MountDevice(const char *name,SMBCONN smbconn, int env)
{
	devoptab_t *dotab_smb;

	dotab_smb=(devoptab_t*)malloc(sizeof(devoptab_t));

	dotab_smb->name=strdup(name);
	dotab_smb->structSize=sizeof(SMBFILESTRUCT); // size of file structure
	dotab_smb->open_r=__smb_open; // device open
	dotab_smb->close_r=__smb_close; // device close
	dotab_smb->write_r=__smb_write; // device write
	dotab_smb->read_r=__smb_read; // device read
	dotab_smb->seek_r=__smb_seek; // device seek
	dotab_smb->fstat_r=__smb_fstat; // device fstat
	dotab_smb->stat_r=__smb_stat; // device stat
	dotab_smb->link_r=NULL; // device link
	dotab_smb->unlink_r=NULL; // device unlink
	dotab_smb->chdir_r=__smb_chdir; // device chdir
	dotab_smb->rename_r=NULL; // device rename
	dotab_smb->mkdir_r=NULL; // device mkdir

	dotab_smb->dirStateSize=sizeof(SMBDIRSTATESTRUCT); // dirStateSize
	dotab_smb->diropen_r=__smb_diropen; // device diropen_r
	dotab_smb->dirreset_r=__smb_dirreset; // device dirreset_r
	dotab_smb->dirnext_r=__smb_dirnext; // device dirnext_r
	dotab_smb->dirclose_r=__smb_dirclose; // device dirclose_r
	dotab_smb->statvfs_r=NULL;			// device statvfs_r
	dotab_smb->ftruncate_r=NULL;               // device ftruncate_r
	dotab_smb->fsync_r=NULL;           // device fsync_r
	dotab_smb->deviceData=NULL;       	/* Device data */

	AddDevice(dotab_smb);

	SMBEnv[env].pos=env;
	SMBEnv[env].smbconn=smbconn;
	SMBEnv[env].name=strdup(name);
	SMBEnv[env].SMBCONNECTED=true;
	SMBEnv[env].first_item_dir=false;
	SMBEnv[env].diropen_root=false;
	SMBEnv[env].devoptab=dotab_smb;

	SMBEnableReadAhead(SMBEnv[env].name,32);
}

bool smbInitDevice(const char* name, const char *user, const char *password, const char *share, const char *ip)
{
	char myIP[16];
	int i;
	if(FirstInit)
	{
		for(i=0;i<MAX_SMB_MOUNTED;i++)
		{
			SMBEnv[i].SMBCONNECTED=false;
			SMBEnv[i].currentpath[0]='\\';
			SMBEnv[i].currentpath[1]='\0';
			SMBEnv[i].first_item_dir=false;
			SMBEnv[i].pos=i;
			SMBEnv[i].SMBReadAheadCache=NULL;
		}
		FirstInit=false;
	}

	for(i=0;i<MAX_SMB_MOUNTED && SMBEnv[i].SMBCONNECTED;i++);
	if(i==MAX_SMB_MOUNTED) return false; //all allowed samba connections reached

	if (if_config(myIP, NULL, NULL, true) < 0)
		return false;
	SMBCONN smbconn;
	if(_SMB_mutex==LWP_MUTEX_NULL)LWP_MutexInit(&_SMB_mutex, false);

	//root connect
	_SMB_lock();
	if (SMB_Connect(&smbconn, user, password, share, ip) != SMB_SUCCESS)
	{
		_SMB_unlock();
		//LWP_MutexDestroy(_SMB_mutex);
		return false;
	}
	_SMB_unlock();

	MountDevice(name,smbconn,i);

	if(end_cache_thread == true) // never close thread
	{
		lwp_t client_thread;
		main_thread = LWP_GetSelf();
		end_cache_thread = false;
		LWP_CreateThread(&client_thread, process_cache_thread, NULL, NULL, 0, 80);
	}
	return true;
}

bool smbInit(const char *user, const char *password, const char *share, const char *ip)
{
	return smbInitDevice("smb", user, password, share, ip);
}

void smbClose(const char* name)
{
	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return;

	if(env->SMBCONNECTED)
	{
		_SMB_lock();
		SMB_Close(env->smbconn);
		_SMB_unlock();
	}
	env->SMBCONNECTED=false;
	RemoveDevice(env->name);
	//LWP_MutexDestroy(_SMB_mutex);
}

bool CheckSMBConnection(const char* name)
{
	char device[50];
	int i;
	bool ret;
	smb_env *env;

	for(i=0;i<50 && name[i]!='\0' && name[i]!=':';i++) device[i]=name[i];
	device[i]='\0';

	env=FindSMBEnv(device);
	if(env==NULL) return false;
	_SMB_lock();
	ret=(SMB_Reconnect(&env->smbconn,true)==SMB_SUCCESS);
	_SMB_unlock();
	return ret;
}
