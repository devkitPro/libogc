/*-------------------------------------------------------------

di.c -- Drive Interface library

Team Twiizers
Copyright (C) 2008

Erant
marcan

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <errno.h>
#include <string.h>

#include <di/di.h>
#include <ogc/ipc.h>
#include <ogc/ios.h>
#include <ogc/mutex.h>

int di_fd = -1;

int _DI_ReadDVD_ReadID(void* buf, uint32_t len, uint32_t lba);
int _DI_ReadDVD_ReadID_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);

int _DI_ReadDVD_A8(void* buf, uint32_t len, uint32_t lba);
int _DI_ReadDVD_D0(void* buf, uint32_t len, uint32_t lba);

int _DI_ReadDVD_A8_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);
int _DI_ReadDVD_D0_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);

void _DI_SetCallback(int di_command, ipccallback);
static int _cover_callback(int ret, void* usrdata);

int state = DVD_INIT | DVD_NO_DISC;

static unsigned int bufferMutex = 0;
static uint32_t outbuf[8] __attribute__((aligned(32)));
static uint32_t dic[8] __attribute__((aligned(32)));
static char di_path[] ATTRIBUTE_ALIGN(32) = "/dev/di";

read_func DI_ReadDVDptr = NULL;
read_func_async DI_ReadDVDAsyncptr = NULL;
di_callback di_cb = NULL;

/*
Initialize the DI interface, should always be called first!
*/

s32 __DI_LoadStub();

int DI_Init(){
	static int init = 0;

	state = DVD_INIT | DVD_NO_DISC;

	if(!init){
		__DI_LoadStub();	// Marcan's 1337 magics happen here!
		init = 1;
	}

	if(di_fd < 0)
		di_fd = IOS_Open(di_path, 2);

	if(!bufferMutex)
		LWP_MutexInit(&bufferMutex, false);

	return (di_fd >= 0)? di_fd : -1;
}

void DI_Mount(){
	uint32_t status;

	if(DI_GetCoverRegister(&status)!=0) {
		state = DVD_NO_DISC;
		return;
	}

	if((status&DVD_COVER_DISC_INSERTED)==0) {
		state = DVD_NO_DISC;
		return;
	}

	state = DVD_INIT | DVD_NO_DISC;
	_cover_callback(1, NULL);	// Initialize the callback chain.
}

void DI_Close(){
	if(di_fd > 0)
		IOS_Close(di_fd);

	di_fd = -1;

	DI_ReadDVDptr = NULL;
	DI_ReadDVDAsyncptr = NULL;
	state = DVD_INIT | DVD_NO_DISC;

	if(bufferMutex) {
		LWP_MutexDestroy(bufferMutex);
		bufferMutex = 0;
	}
}
	

#define COVER_CLOSED (*((uint32_t*)usrdata) & DVD_COVER_DISC_INSERTED)

static int _cover_callback(int ret, void* usrdata){
	static int cur_state = 0;
	static int retry_count = LIBDI_MAX_RETRIES;
	const int callback_table[] = {
		DVD_GETCOVER,
		DVD_WAITFORCOVERCLOSE,
		DVD_RESET,
		DVD_IDENTIFY,		// This one will complete when the drive is ready.
		DVD_READ_DISCID,
		0};
	const int return_table[] = {1,1,4,1,1};

	if(cur_state > 1)
		state &= ~DVD_NO_DISC;

	if(callback_table[cur_state]){
		if(ret == return_table[cur_state]){
			if(cur_state == 1 && COVER_CLOSED)	// Disc inside, skipping wait for cover.
				cur_state += 2;	
			else
				cur_state++; // If the previous callback succeeded, moving on to the next

			retry_count = LIBDI_MAX_RETRIES;
		}
		else
		{
			retry_count--;
			if(retry_count < 0){		// Drive init failed for unknown reasons.
				retry_count = LIBDI_MAX_RETRIES;
				cur_state = 0;
				state = DVD_UNKNOWN;
				return 0;
			}
		}
		_DI_SetCallback(callback_table[cur_state - 1], _cover_callback);

	}
	else		// Callback chain has completed OK. The drive is ready.
	{
		state = DVD_READY;
		if(IOS_GetVersion() < 200){
			state |= DVD_D0;
			DI_ReadDVDptr = _DI_ReadDVD_D0;
			DI_ReadDVDAsyncptr = _DI_ReadDVD_D0_Async;
		}
		else
		{
			state |= DVD_A8;
			DI_ReadDVDptr = _DI_ReadDVD_A8;
			DI_ReadDVDAsyncptr = _DI_ReadDVD_A8_Async;
		}

		if(di_cb)
			di_cb(state,0);

		retry_count = LIBDI_MAX_RETRIES;
		cur_state = 0;
	}
	return 0;
}

/* Get current status, will return the API status */
int DI_GetStatus(){
	return state;
}

void DI_SetInitCallback(di_callback cb){
	di_cb = cb;
}

void _DI_SetCallback(int ioctl_nr, ipccallback ipc_cb){

	if(di_fd<0 || !ipc_cb) return;

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	memset(dic, 0x00, sizeof(dic));

	dic[0] = ioctl_nr << 24;
	dic[1] = (ioctl_nr == DVD_RESET)? 1 : 0;	// For reset callback. Dirty, I know...

	IOS_IoctlAsync(di_fd,ioctl_nr, dic, 0x20, outbuf, 0x20, ipc_cb, outbuf);

	//We're done with the buffer now.
	LWP_MutexUnlock(bufferMutex);
}

/*
Request an identification from the drive, returned in a DI_DriveID struct
*/
int DI_Identify(DI_DriveID* id){
	if(di_fd < 0) {
		errno = ENXIO;
		return -1;
	}

	if(!id){
		errno = EINVAL;
		return -1;
	}

	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_IDENTIFY << 24;
	
	int ret = IOS_Ioctl(di_fd, DVD_IDENTIFY, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	memcpy(id,outbuf,sizeof(DI_DriveID));

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Returns the current error code on the drive.
yagcd has a pretty comprehensive list of possible error codes
*/
int DI_GetError(uint32_t* error){
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!error){
		errno = EINVAL;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_GET_ERROR << 24;
	
	int ret = IOS_Ioctl(di_fd, DVD_GET_ERROR, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	*error = outbuf[0];		// Error code is returned as an int in the first four bytes of outbuf.

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;		
}

/*
Reset the drive.
*/
int DI_Reset(){
	if(di_fd < 0)
		return -1;

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_RESET << 24;
	dic[1] = 1;
	
	int ret = IOS_Ioctl(di_fd, DVD_RESET, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Main read function, basically just a wrapper to the function pointer.
Nicer then just exposing the pointer itself
*/
int DI_ReadDVD(void* buf, uint32_t len, uint32_t lba){

	int ret;
	if(DI_ReadDVDptr){
		// Wait for the lock. Doing it here saves me from doing it in all the read functions.
		while(LWP_MutexLock(bufferMutex));
		ret = DI_ReadDVDptr(buf,len,lba);
		LWP_MutexUnlock(bufferMutex);
		return ret;
	}
	return -1;
}

int DI_ReadDVDAsync(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb){

	int ret;
	if(DI_ReadDVDAsyncptr){
		while(LWP_MutexLock(bufferMutex));
		ret = DI_ReadDVDAsyncptr(buf,len,lba,ipc_cb);
		LWP_MutexUnlock(bufferMutex);
		return ret;
	}
	return -1;
}

/*
Unknown what this does as of now...
*/
int DI_ReadDVDConfig(uint32_t* val, uint32_t flag){
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!val){
		errno = EINVAL;
		return -1;
	}

	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_READ_CONFIG << 24;
	dic[1] = flag & 0x1;		// Update flag, val will be written if this is 1, val won't be written if it's 0.
	dic[2] = 0;					// Command will fail driveside if this is not zero.
	dic[3] = *val;
	
	int ret = IOS_Ioctl(di_fd, DVD_READ_CONFIG, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	*val = outbuf[0];
	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Read the copyright information on a DVDVideo
*/
int DI_ReadDVDCopyright(uint32_t* copyright){
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!copyright){
		errno = EINVAL;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_READ_COPYRIGHT << 24;
	dic[1] = 0;
	
	int ret = IOS_Ioctl(di_fd, DVD_READ_COPYRIGHT, dic, 0x20, outbuf, 0x20);
	*copyright = *((uint32_t*)outbuf);		// Copyright information is returned as an int in the first four bytes of outbuf.

	if(ret == 2) errno = EIO;
	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Returns 0x800 bytes worth of Disc key
*/
int DI_ReadDVDDiscKey(void* buf){

	int ret;
	int retry_count = LIBDI_MAX_RETRIES;

	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}
	
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_DISCKEY << 24;
	dic[1] = 0;		// Unknown what this flag does.
	do{
		ret = IOS_Ioctl(di_fd, DVD_READ_DISCKEY, dic, 0x20, buf, 0x800);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
This function will read the initial sector on the DVD, which contains stuff like the booktype
*/
int DI_ReadDVDPhysical(void* buf){


	int ret;
	int retry_count = LIBDI_MAX_RETRIES;

	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_PHYSICAL << 24;
	dic[1] = 0;		// Unknown what this flag does.
	
	do{
		ret = IOS_Ioctl(di_fd, DVD_READ_PHYSICAL, dic, 0x20, buf, 0x800);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_ReportKey(int keytype, uint32_t lba, void* buf){
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!buf){
		errno = EINVAL;
		return -1;
	}
	
	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_REPORTKEY << 24;
	dic[1] = keytype & 0xFF;
	dic[2] = lba;
	
	int ret = IOS_Ioctl(di_fd, DVD_REPORTKEY, dic, 0x20, buf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_GetCoverRegister(uint32_t* status){
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	while(LWP_MutexLock(bufferMutex));

	memset(dic, 0x00, 0x20);

	int ret = IOS_Ioctl(di_fd, DVD_GETCOVER, dic, 0x20, outbuf, 0x20);
	if(ret == 2) errno = EIO;

	*status = outbuf[0];

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/* Internal function for controlling motor operations */
int _DI_SetMotor(int flag){
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_SET_MOTOR << 24;
	dic[1] = flag & 0x1;			// Eject flag.
	dic[2] = (flag >> 1) & 0x1;		// Don't use this flag, it kills the drive untill next reset.

	int ret = IOS_Ioctl(di_fd, DVD_SET_MOTOR, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

/* Stop the drives motor, needs a reset afterwards for normal operation */
int DI_StopMotor(){
	return _DI_SetMotor(0);
}

/* Stop the motor, and eject the disc. Also needs a reset afterwards for normal operation */
int DI_Eject(){
	return _DI_SetMotor(1);
}

/* Warning, this will kill your drive untill the next reset. Will not respond to DI commands,
will not take in or eject the disc. Your drive will be d - e - d, dead.

I deem this function to be harmless, as normal operation will resume after a reset.
However, I am not liable for anyones drive exploding as a result from using this function.
*/
int DI_KillDrive(){
	return _DI_SetMotor(2);
}

int DI_ClosePartition() {
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_CLOSE_PARTITION << 24;

	int ret = IOS_Ioctl(di_fd, DVD_CLOSE_PARTITION, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

int DI_OpenPartition(uint32_t offset)
{
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	static ioctlv vectors[5] __attribute__((aligned(32)));
	static char certs[0x49e4] __attribute__((aligned(32)));
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_OPEN_PARTITION << 24;
	dic[1] = offset;

	vectors[0].data = dic;
	vectors[0].len = 0x20;
	vectors[1].data = NULL;
	vectors[1].len = 0x2a4;
	vectors[2].data = NULL;
	vectors[2].len = 0;

	vectors[3].data = certs;
	vectors[3].len = 0x49e4;
	vectors[4].data = outbuf;
	vectors[4].len = 0x20;

	int ret = IOS_Ioctlv(di_fd, DVD_OPEN_PARTITION, 3, 2, vectors);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;

}

int DI_Read(void *buf, uint32_t size, uint32_t offset)
{
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_LOW_READ << 24;
	dic[1] = size;
	dic[2] = offset;

	int ret = IOS_Ioctl(di_fd, DVD_LOW_READ, dic, 0x20, buf, size);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

int DI_UnencryptedRead(void *buf, uint32_t size, uint32_t offset)
{
	int ret, retry_count = LIBDI_MAX_RETRIES;

	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){		// This only works with 32 byte aligned addresses!
		errno = EFAULT;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = size;
	dic[2] = offset;

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, size);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);

	return (ret == 1)? 0 : -ret;
}

int DI_ReadDiscID(uint64_t *id)
{
	if(di_fd < 0){
		errno = ENXIO;
		return -1;
	}

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_DISCID << 24;

	int ret = IOS_Ioctl(di_fd, DVD_READ_DISCID, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	memcpy(id, outbuf, sizeof(*id));

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

bool diio_Startup()
{
	return true;
}

bool diio_Shutdown()
{
	return true;
}

bool diio_ReadSectors(sec_t sector,sec_t numSectors,void *buffer)
{
	return true;
}

bool diio_WriteSectors(sec_t sector,sec_t numSectors,const void *buffer)
{
	return true;
}

bool diio_ClearStatus()
{
	return true;
}

bool diio_IsInserted()
{
	return true;
}

bool diio_IsInitialized()
{
	return true;
}

const DISC_INTERFACE __io_wiidvd = {
	DEVICE_TYPE_WII_DVD,
	FEATURE_MEDIUM_CANREAD | FEATURE_WII_DVD,
	(FN_MEDIUM_STARTUP)&diio_Startup,
	(FN_MEDIUM_ISINSERTED)&diio_IsInserted,
	(FN_MEDIUM_READSECTORS)&diio_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&diio_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&diio_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&diio_Shutdown
};
