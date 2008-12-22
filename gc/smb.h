/****************************************************************************
 * TinySMB-GC
 *
 * Nintendo Gamecube SaMBa implementation.
 *
 * Copyright softdev
 * Modified by Tantric to utilize NTLM authentication
 * PathInfo added by rodries
 * SMB devoptab by scip, rodries
 *
 * Authentication modules, LMhash and DES are 
 *
 * Copyright Christopher R Hertel.
 * http://www.ubiqx.org
 *
 * You WILL find Ethereal, available from http://www.ethereal.com
 * invaluable for debugging each new SAMBA implementation.
 *
 * Recommended Reading
 *	Implementing CIFS - Christopher R Hertel
 *	SNIA CIFS Documentation - http://www.snia.org
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/
#ifndef __NBTSMB_H__
#define __NBTSMB_H__

#include <gctypes.h>

/**
* SMB Error codes
*/
#define SMB_SUCCESS					0
#define SMB_ERROR				   -1
#define SMB_BAD_PROTOCOL		   -2
#define SMB_BAD_COMMAND			   -3
#define SMB_PROTO_FAIL			   -4
#define SMB_NOT_USER			   -5
#define SMB_BAD_KEYLEN			   -6
#define SMB_BAD_DATALEN			   -7

/**
* SMB File Open Function
*/
#define SMB_OF_OPEN					1
#define SMB_OF_TRUNCATE				2
#define SMB_OF_CREATE				16

/**
* FileSearch
*/
#define SMB_SRCH_DIRECTORY			16
#define SMB_SRCH_READONLY  			1
#define SMB_SRCH_HIDDEN				2
#define SMB_SRCH_SYSTEM				4
#define SMB_SRCH_VOLUME				8

/**
* SMB File Access Modes
*/
#define SMB_OPEN_READING			0
#define SMB_OPEN_WRITING			1
#define SMB_OPEN_READWRITE			2
#define SMB_OPEN_COMPATIBLE			0
#define SMB_DENY_READWRITE			0x10
#define SMB_DENY_WRITE				0x20
#define SMB_DENY_READ				0x30
#define SMB_DENY_NONE				0x40


#ifdef __cplusplus
extern "C" {
#endif

/***
 * SMB Connection Handle
 */
typedef u32 SMBCONN;

/***
 * SMB File Handle
 */
typedef void* SMBFILE;

/*** SMB_FILEENTRY
     SMB Long Filename Directory Entry
 ***/
typedef struct
{
  u32 size_low;
  u32 size_high;
  u8 attributes;
  char name[256];
} SMBDIRENTRY;

/**
 * Prototypes
 */

/*** Înitialization functions to be used with stdio API ***/
bool smbInit(const char *user, const char *password, const char *share,	const char *ip);
void smbClose();

/*** Session ***/
s32 SMB_Connect(SMBCONN *smbhndl, const char *user, const char *password, const char *share, const char *IP);
void SMB_Close(SMBCONN smbhndl);

/*** File Find ***/
s32 SMB_PathInfo(const char *filename, SMBDIRENTRY *sdir, SMBCONN smbhndl);
s32 SMB_FindFirst(const char *filename, unsigned short flags, SMBDIRENTRY *sdir,SMBCONN smbhndl);
s32 SMB_FindNext(SMBDIRENTRY *sdir,SMBCONN smbhndl);
s32 SMB_FindClose(SMBCONN smbhndl);

/*** File I/O ***/
SMBFILE SMB_OpenFile(const char *filename, unsigned short access, unsigned short creation,SMBCONN smbhndl);
void SMB_CloseFile(SMBFILE sfid);
s32 SMB_ReadFile(char *buffer, int size, int offset, SMBFILE sfid);
s32 SMB_WriteFile(const char *buffer, int size, int offset, SMBFILE sfid);

#ifdef __cplusplus
	}
#endif

#endif
