#include <errno.h>
#include "di.h"

extern int di_fd;
static uint32_t dic[8] __attribute__((aligned(32)));

int _DI_ReadDVD_A8(void* buf, uint32_t len, uint32_t lba);
int _DI_ReadDVD_D0(void* buf, uint32_t len, uint32_t lba);

int _DI_ReadDVD_A8_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);
int _DI_ReadDVD_D0_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);

/*
Internal function, used when a modchip has been detected.
Please refrain from using this function directly.
*/
int _DI_ReadDVD_A8_Async(void* buf, uint32_t len, uint32_t lba, ipccallback ipc_cb){
	int ret;
	
	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){		// This only works with 32 byte aligned addresses!
		errno = EFAULT;
		return -1;
	}
	
	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = len << 11;			// 1 LB is 2048 bytes
	dic[2] = lba << 9;			// Nintendo's read function uses byteOffset >> 2, so we only shift 9 left, not 11.

	ret = IOS_IoctlAsync(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, len << 11,ipc_cb, buf);

	if(ret == 2) errno = EIO;

	return (ret == 1)? 0 : -ret;
}

/*
Internal function, used when the drive is DVDVideo compatible.
Please refrain from using this function directly.
*/
int _DI_ReadDVD_D0_Async(void* buf, uint32_t len, uint32_t lba, ipccallback ipc_cb){
	int ret;

	if(!buf){
		errno = EINVAL;
		return -1;
	}
	
	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

	dic[0] = DVD_READ << 24;
	dic[1] = 0;		// Unknown what this does as of now. (Sets some value to 0x10 in the drive if set).
	dic[2] = 0;		// USE_DEFAULT_CONFIG flag. Drive will use default config if this bit is set.
	dic[3] = len;
	dic[4] = lba;
	
	ret = IOS_IoctlAsync(di_fd, DVD_READ, dic, 0x20, buf, len << 11,ipc_cb, buf);

	if(ret == 2) errno = EIO;

	return (ret == 1)? 0 : -ret;
}




/*
Internal function, used when a modchip has been detected.
Please refrain from using this function directly.
*/
int _DI_ReadDVD_A8(void* buf, uint32_t len, uint32_t lba){
	int ret, retry_count = LIBDI_MAX_RETRY;
	
	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){		// This only works with 32 byte aligned addresses!
		errno = EFAULT;
		return -1;
	}
	
	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = len << 11;			// 1 LB is 2048 bytes
	dic[2] = lba << 9;			// Nintendo's read function uses byteOffset >> 2, so we only shift 9 left, not 11.

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, len << 11);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	return (ret == 1)? 0 : -ret;
}

/*
Internal function, used when the drive is DVDVideo compatible.
Please refrain from using this function directly.
*/
int _DI_ReadDVD_D0(void* buf, uint32_t len, uint32_t lba){
	int ret, retry_count = LIBDI_MAX_RETRY;
	
	if(!buf){
		errno = EINVAL;
		return -1;
	}
	
	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

	dic[0] = DVD_READ << 24;
	dic[1] = 0;		// Unknown what this does as of now. (Sets some value to 0x10 in the drive if set).
	dic[2] = 0;		// USE_DEFAULT_CONFIG flag. Drive will use default config if this bit is set.
	dic[3] = len;
	dic[4] = lba;
	
	

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ, dic, 0x20, buf, len << 11);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	return (ret == 1)? 0 : -ret;
}
