#ifndef __CARD_H__
#define __CARD_H__

#include <gctypes.h>

/* slots */
#define CARD_SLOTA					0
#define CARD_SLOTB					1

#define CARD_WORKAREA				(5*8*1024)
#define CARD_READSIZE				512
#define CARD_FILENAMELEN			32
#define CARD_MAXFILES				128

/* Errors */
#define CARD_ERROR_UNLOCKED			1
#define CARD_ERROR_READY            0
#define CARD_ERROR_BUSY            -1
#define CARD_ERROR_WRONGDEVICE     -2
#define CARD_ERROR_NOCARD          -3
#define CARD_ERROR_NOFILE          -4
#define CARD_ERROR_IOERROR         -5
#define CARD_ERROR_BROKEN          -6
#define CARD_ERROR_EXIST           -7
#define CARD_ERROR_NOENT           -8
#define CARD_ERROR_INSSPACE        -9
#define CARD_ERROR_NOPERM          -10
#define CARD_ERROR_LIMIT           -11
#define CARD_ERROR_NAMETOOLONG     -12
#define CARD_ERROR_ENCODING        -13
#define CARD_ERROR_CANCELED        -14
#define CARD_ERROR_FATAL_ERROR     -128


/* File attribute defines */
#define CARD_ATTRIB_PUBLIC			0x04
#define CARD_ATTRIB_NOCOPY			0x08
#define CARD_ATTRIB_NOMOVE			0x10

/* Banner & Icon Attributes */
#define CARD_BANNER_W				96
#define CARD_BANNER_H				32

#define CARD_BANNER_NONE			0x00
#define CARD_BANNER_CI				0x01
#define CARD_BANNER_RGB				0x02
#define CARD_BANNER_MASK			0x03

#define CARD_MAXICONS				8
#define CARD_ICON_W					32
#define CARD_ICON_H					32

#define CARD_ICON_NONE				0x00
#define CARD_ICON_CI				0x01
#define CARD_ICON_RGB				0x02
#define CARD_ICON_MASK				0x03

#define CARD_ANIM_LOOP				0x00
#define CARD_ANIM_BOUNCE			0x04
#define CARD_ANIM_MASK				0x04

#define CARD_SPEED_END				0x00
#define CARD_SPEED_FAST				0x01
#define CARD_SPEED_MIDDLE			0x02
#define CARD_SPEED_SLOW				0x03
#define CARD_SPEED_MASK				0x03

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _card_file {
	s32 chn;
	s32 filenum;
	s32 offset;
	s32 len;
	u16 iblock;
} card_file;

typedef struct _card_dir { 
      s32 chn; 
      u32 fileno; 
      u8 filename[CARD_FILENAMELEN+1]; 
      u8 gamecode[5]; 
      u8 company[3];
      bool showall;
} card_dir; 

typedef struct _card_stat {
	u8 filename[CARD_FILENAMELEN];
	u32 len;
	u32 time;		//time since 1970 in seconds
	u8 gamecode[4];
	u8 company[2];
	u8 banner_fmt;
	u32 icon_addr;
	u16 icon_fmt;
	u16 icon_speed;
	u32 comment_addr;
	u32 offset_banner;
	u32 offset_banner_tlut;
	u32 offset_icon[CARD_MAXICONS];
	u32 offset_icon_tlut;
	u32 offset_data;
} card_stat;

#define CARD_GetBannerFmt(stat)         (((stat)->banner_fmt)&CARD_BANNER_MASK)
#define CARD_SetBannerFmt(stat,fmt)		((stat)->banner_fmt = (u8)(((stat)->banner_fmt&~CARD_BANNER_MASK)|(fmt)))
#define CARD_GetIconFmt(stat,n)			(((stat)->icon_fmt>>(2*(n)))&CARD_ICON_MASK)
#define CARD_SetIconFmt(stat,n,fmt)		((stat)->icon_fmt = (u16)(((stat)->icon_fmt&~(CARD_ICON_MASK<<(2*(n))))|((fmt)<<(2*(n)))))
#define CARD_GetIconSpeed(stat,n)		(((stat)->icon_speed>>(2*(n)))&~CARD_SPEED_MASK);
#define CARD_SetIconSpeed(stat,n,speed)	((stat)->icon_speed = (u16)(((stat)->icon_fmt&~(CARD_SPEED_MASK<<(2*(n))))|((speed)<<(2*(n)))))
#define CARD_SetIconAddr(stat,addr)		((stat)->icon_addr = (u32)(addr))
#define CARD_SetCommentAddr(stat,addr)	((stat)->comment_addr = (u32)(addr))

typedef void (*cardcallback)(s32 chan,s32 result);


/*! \fn s32 CARD_Init(const char *gamecode,const char *company)
\brief Performs the initialization of the memory card subsystem
\param gamecode pointer to a 4byte long string to specify the vendors game code. May be NULL
\param company pointer to a 2byte long string to specify the vendors company code. May be NULL

\return 0 on success, <0 on error
*/
s32 CARD_Init(const char *gamecode,const char *company);


/*! \fn s32 CARD_Probe(s32 chn)
\brief Performs a check against the desired EXI channel if a device is inserted
\param chn CARD slot

\return 0 on success, <0 on error
*/
s32 CARD_Probe(s32 chn);


/*! \fn s32 CARD_ProbeEx(s32 chn,s32 *mem_size,s32 *sect_size)
\brief Performs a check against the desired EXI channel if a memory card is inserted or mounted
\param chn CARD slot
\param mem_size pointer to a integer variable, ready to take the resulting value (this param is optional and can be NULL)
\param sect_size pointer to a integer variable, ready to take the resulting value (this param is optional and can be NULL)

\return 0 on success, <0 on error
*/
s32 CARD_ProbeEx(s32 chn,s32 *mem_size,s32 *sect_size);


/*! \fn s32 CARD_Mount(s32 chn,void *workarea,cardcallback detach_cb)
\brief Mounts the memory card in the slot CHN. Synchronous version.
\param chn CARD slot
\param workarea pointer to memory area to hold the cards system area. The startaddress of the workdarea should be aligned on a 32byte boundery
\param detach_cb pointer to a callback function. This callback function will be called when the card is removed from the slot.

\return 0 on success, <0 on error
*/
s32 CARD_Mount(s32 chn,void *workarea,cardcallback detach_cb);


/*! \fn s32 CARD_MountAsync(s32 chn,void *workarea,cardcallback detach_cb,cardcallback attach_cb)
\brief Mounts the memory card in the slot CHN. This function returns immediately. Asynchronous version.
\param chn CARD slot
\param workarea pointer to memory area to hold the cards system area. The startaddress of the workdarea should be aligned on a 32byte boundery
\param detach_cb pointer to a callback function. This callback function will be called when the card is removed from the slot.
\param attach_cb pointer to a callback function. This callback function will be called when the mount process has finished.

\return >=0 on success, <0 on error
*/
s32 CARD_MountAsync(s32 chn,void *workarea,cardcallback detach_cb,cardcallback attach_cb);


/*! \fn s32 CARD_Unmount(s32 chn)
\brief Unmounts the memory card in the slot CHN and releases the EXI bus.
\param chn CARD slot

\return 0 on success, <0 on error
*/
s32 CARD_Unmount(s32 chn);


/*! \fn s32 CARD_Read(card_file *file,void *buffer,u32 len,u32 offset)
\brief Reads the data from the file into the buffer from the given offset with the given length. Synchronous version
\param chn CARD slot
\param buffer pointer to memory area read-in the data. The startaddress of the buffer should be aligned to a 32byte boundery.
\param len length of data to read.
\param offset offset into the file to read from.

\return 0 on success, <0 on error
*/
s32 CARD_Read(card_file *file,void *buffer,u32 len,u32 offset);


/*! \fn s32 CARD_ReadAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback)
\brief Reads the data from the file into the buffer from the given offset with the given length. This function returns immediately. Asynchronous version
\param chn CARD slot
\param buffer pointer to memory area read-in the data. The startaddress of the buffer should be aligned to a 32byte boundery.
\param len length of data to read.
\param offset offset into the file to read from.
\param callback pointer to a callback function. This callback will be called when the read process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_ReadAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback);


/*! \fn s32 CARD_Open(s32 chn,const char *filename,card_file *file)
\brief Opens the file with the given filename and fills in the fileinformations.
\param chn CARD slot
\param filename name of the file to open.
\param file pointer to the card_file structure. It receives the fileinformations for later usage.

\return 0 on success, <0 on error
*/
s32 CARD_Open(s32 chn,const char *filename,card_file *file);


/*! \fn s32 CARD_OpenEntry(card_dir *entry,card_file *file)
\brief Opens the file with the given filename and fills in the fileinformations.
\param filename name of the file to open.
\param file pointer to the card_file structure. It receives the fileinformations for later usage.

\return 0 on success, <0 on error
*/
s32 CARD_OpenEntry(card_dir *entry,card_file *file);


/*! \fn s32 CARD_Close(card_file *file)
\brief Closes the file with the given card_file structure and releases the handle.
\param card_file pointer to the card_file structure.

\return 0 on success, <0 on error
*/
s32 CARD_Close(card_file *file);


/*! \fn s32 CARD_Create(s32 chn,const char *filename,u32 size,card_file *file)
\brief Creates a new file with the given filename and fills in the fileinformations. Synchronous version.
\param chn CARD slot
\param filename name of the file to create.
\param size size of the newly created file.
\param file pointer to the card_file structure. It receives the fileinformations for later usage.

\return 0 on success, <0 on error
*/
s32 CARD_Create(s32 chn,const char *filename,u32 size,card_file *file);


/*! \fn s32 CARD_CreateAsync(s32 chn,const char *filename,u32 size,card_file *file,cardcallback callback)
\brief Creates a new file with the given filename and fills in the fileinformations. This function returns immediately. Asynchronous version.
\param chn CARD slot
\param filename name of the file to create.
\param size size of the newly created file.
\param file pointer to the card_file structure. It receives the fileinformations for later usage.
\param callback pointer to a callback function. This callback will be called when the create process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_CreateAsync(s32 chn,const char *filename,u32 size,card_file *file,cardcallback callback);


/*! \fn s32 CARD_Delete(s32 chn,const char *filename)
\brief Deletes a file with the given filename. Synchronous version.
\param chn CARD slot
\param filename name of the file to delete.

\return 0 on success, <0 on error
*/
s32 CARD_Delete(s32 chn,const char *filename);


/*! \fn s32 CARD_DeleteAsync(s32 chn,const char *filename,cardcallback callback)
\brief Deletes a file with the given filename. This function returns immediately. Asynchronous version.
\param chn CARD slot
\param filename name of the file to delete.
\param callback pointer to a callback function. This callback will be called when the delete process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_DeleteAsync(s32 chn,const char *filename,cardcallback callback);


/*! \fn s32 CARD_DeleteEntry(card_dir *dir_entry)
\brief Deletes a file with the given directory entry informations.
\param dir_entry pointer to the card_dir structure which holds the informations for the delete operation.

\return 0 on success, <0 on error
*/
s32 CARD_DeleteEntry(card_dir *dir_entry);


/*! \fn s32 CARD_DeleteEntryAsync(card_dir *dir_entry,cardcallback callback)
\brief Deletes a file with the given directory entry informations. This function returns immediately. Asynchronous version.
\param dir_entry pointer to the card_dir structure which holds the informations for the delete operation.
\param callback pointer to a callback function. This callback will be called when the delete process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_DeleteEntryAsync(card_dir *dir_entry,cardcallback callback);


/*! \fn s32 CARD_Write(card_file *file,void *buffer,u32 len,u32 offset)
\brief Writes the data to the file from the buffer to the given offset with the given length. Synchronous version
\param file pointer to the card_file structure which holds the fileinformations.
\param buffer pointer to the memory area to read from. The startaddress of the buffer should be aligned on a 32byte boundery.
\param len length of data to write.
\param offset starting point in the file to start writing.

\return 0 on success, <0 on error
*/
s32 CARD_Write(card_file *file,void *buffer,u32 len,u32 offset);


/*! \fn s32 CARD_WriteAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback)
\brief Writes the data to the file from the buffer to the given offset with the given length. This function returns immediately. Asynchronous version
\param file pointer to the card_file structure which holds the fileinformations.
\param buffer pointer to the memory area to read from. The startaddress of the buffer should be aligned on a 32byte boundery.
\param len length of data to write.
\param offset starting point in the file to start writing.
\param callback pointer to a callback function. This callback will be called when the write process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_WriteAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback);


/*! \fn s32 CARD_GetErrorCode(s32 chn)
\brief Returns the result code from the last operation.
\param chn CARD slot

\return <=0 result of last operation
*/
s32 CARD_GetErrorCode(s32 chn);


/*! \fn s32 CARD_FindFirst(s32 chn, card_dir *dir, bool showall)
\brief Start to iterate thru the memory card's directory structure and returns the first directory entry.
\param chn CARD slot
\param dir pointer to card_dir structure to receive the result set.
\param showall Whether to show all files of the memory card or only those which are identified by the company and gamecode string.

\return 0 on success, <0 on error
*/
s32 CARD_FindFirst(s32 chn, card_dir *dir, bool showall);
 

/*! \fn s32 CARD_FindNext(card_dir *dir)
\brief Returns the next directory entry from the memory cards directory structure.
\param dir pointer to card_dir structure to receive the result set.

\return 0 on success, <0 on error
*/
s32 CARD_FindNext(card_dir *dir); 


/*! \fn s32 CARD_GetDirectory(s32 chn, card_dir *dir_entries, s32 *count, bool showall)
\brief Returns the directory entries. size of entries is max. 128.
\param chn CARD slot
\param dir pointer to card_dir structure to receive the result set.
\param count pointer to an integer to receive the counted entries.
\param showall Whether to show all files of the memory card or only those which are identified by the company and gamecode string.

\return 0 on success, <0 on error
*/
s32 CARD_GetDirectory(s32 chn, card_dir *dir_entries, s32 *count, bool showall);
 

/*! \fn s32 CARD_GetSectorSize(s32 chn,u32 *sector_size)
\brief Returns the next directory entry from the memory cards directory structure.
\param chn CARD slot.
\param sector_size pointer to receive the result.

\return 0 on success, <0 on error
*/
s32 CARD_GetSectorSize(s32 chn,u32 *sector_size);


/*! \fn s32 CARD_GetStatus(s32 chn,s32 fileno,card_stat *stats)
\brief Get additional file statistic informations.
\param chn CARD slot.
\param fileno file index. returned by a previous call to CARD_Open().
\param stats pointer to receive the result set.

\return 0 on success, <0 on error
*/
s32 CARD_GetStatus(s32 chn,s32 fileno,card_stat *stats);


/*! \fn s32 CARD_SetStatus(s32 chn,s32 fileno,card_stat *stats)
\brief Set additional file statistic informations. Synchronous version.
\param chn CARD slot.
\param fileno file index. returned by a previous call to CARD_Open().
\param stats pointer which holds the informations to set.

\return 0 on success, <0 on error
*/
s32 CARD_SetStatus(s32 chn,s32 fileno,card_stat *stats);


/*! \fn s32 CARD_SetStatusAsync(s32 chn,s32 fileno,card_stat *stats,cardcallback callback)
\brief Set additional file statistic informations. This function returns immediately. Asynchronous version.
\param chn CARD slot.
\param fileno file index. returned by a previous call to CARD_Open().
\param stats pointer which holds the informations to set.
\param callback pointer to a callback function. This callback will be called when the setstatus process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_SetStatusAsync(s32 chn,s32 fileno,card_stat *stats,cardcallback callback);


/*! \fn s32 CARD_GetAttributes(s32 chn,s32 fileno,u8 *attr)
\brief Get additional file attributes. Synchronous version.
\param chn CARD slot.
\param fileno file index. returned by a previous call to CARD_Open().
\param attr pointer to receive attribute value.

\return 0 on success, <0 on error
*/
s32 CARD_GetAttributes(s32 chn,s32 fileno,u8 *attr);


/*! \fn s32 CARD_SetAttributes(s32 chn,s32 fileno,u8 attr)
\brief Set additional file attributes. Synchronous version.
\param chn CARD slot.
\param fileno file index. returned by a previous call to CARD_Open().
\param attr attribute value to set.

\return 0 on success, <0 on error
*/
s32 CARD_SetAttributes(s32 chn,s32 fileno,u8 attr);


/*! \fn s32 CARD_SetAttributesAsync(s32 chn,s32 fileno,u8 attr,cardcallback callback)
\brief Set additional file attributes. This function returns immediately. Asynchronous version.
\param chn CARD slot.
\param fileno file index. returned by a previous call to CARD_Open().
\param attr attribute value to set.
\param callback pointer to a callback function. This callback will be called when the setattributes process has finished.

\return 0 on success, <0 on error
*/
s32 CARD_SetAttributesAsync(s32 chn,s32 fileno,u8 attr,cardcallback callback);

/**
 * Not finished functions
*/
s32 CARD_Format(s32 chn);
s32 CARD_FormatAsync(s32 chn,cardcallback callback);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
