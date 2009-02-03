/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * Copyright softdev
 * Modified by Tantric to utilize NTLM authentication
 * PathInfo added by rodries
 * SMB devoptab by scip, rodries
 *
 * You will find WireShark (http://www.wireshark.org/)
 * invaluable for debugging SAMBA implementations.
 *
 * Recommended Reading
 *	Implementing CIFS - Christopher R Hertel
 *	http://www.ubiqx.org/cifs/SMB.html
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

#include <asm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <gccore.h>
#include <network.h>
#include <processor.h>
#include <lwp_threads.h>
#include <lwp_objmgr.h>
#include <smb.h>


/**
 * Field offsets.
 */
#define SMB_OFFSET_PROTO			0
#define SMB_OFFSET_CMD				4
#define SMB_OFFSET_NTSTATUS			5
#define SMB_OFFSET_ECLASS			5
#define SMB_OFFSET_ECODE			7
#define SMB_OFFSET_FLAGS			9
#define SMB_OFFSET_FLAGS2			10
#define SMB_OFFSET_EXTRA			12
#define SMB_OFFSET_TID				24
#define SMB_OFFSET_PID				26
#define SMB_OFFSET_UID				28
#define SMB_OFFSET_MID				30
#define SMB_HEADER_SIZE				32		/*** SMB Headers are always 32 bytes long ***/

/**
 * Message / Commands
 */
#define NBT_SESSISON_MSG			0x00

#define SMB_NEG_PROTOCOL			0x72
#define SMB_SETUP_ANDX				0x73
#define SMB_TREEC_ANDX				0x75

/**
 * SMBTrans2 
 */
#define SMB_TRANS2					0x32

#define SMB_OPEN2					0
#define SMB_FIND_FIRST2				1
#define SMB_FIND_NEXT2				2
#define SMB_QUERY_FS_INFO			3
#define SMB_QUERY_PATH_INFO			5
#define SMB_SET_PATH_INFO			6
#define SMB_QUERY_FILE_INFO			7
#define SMB_SET_FILE_INFO			8
#define SMB_CREATE_DIR				13
#define SMB_FIND_CLOSE2				0x34

/**
 * File I/O
 */
#define SMB_OPEN_ANDX				0x2d
#define SMB_WRITE_ANDX				0x2f
#define SMB_READ_ANDX				0x2e
#define SMB_CLOSE					0x04


/**
 * TRANS2 Offsets
 */
#define T2_WORD_CNT				    (SMB_HEADER_SIZE)
#define T2_PRM_CNT				    (T2_WORD_CNT+1)
#define T2_DATA_CNT				    (T2_PRM_CNT+2)
#define T2_MAXPRM_CNT			    (T2_DATA_CNT+2)
#define T2_MAXBUFFER			    (T2_MAXPRM_CNT+2)
#define T2_SETUP_CNT			    (T2_MAXBUFFER+2)
#define T2_SPRM_CNT				    (T2_SETUP_CNT+10)
#define T2_SPRM_OFS				    (T2_SPRM_CNT+2)
#define T2_SDATA_CNT			    (T2_SPRM_OFS+2)
#define T2_SDATA_OFS			    (T2_SDATA_CNT+2)
#define T2_SSETUP_CNT			    (T2_SDATA_OFS+2)
#define T2_SUB_CMD				    (T2_SSETUP_CNT+2)
#define T2_BYTE_CNT				    (T2_SUB_CMD+2)
								
								
#define SMB_PROTO					0x424d53ff
#define SMB_HANDLE_NULL				0xffffffff
#define SMB_MAX_BUFFERSIZE			(62*1024)	// cannot be larger than u16 (65536)
#define SMB_MAX_TRANSMIT_SIZE		7236
#define SMB_DEF_READOFFSET			59
								
#define SMB_CONNHANDLES_MAX			8
#define SMB_FILEHANDLES_MAX			(32*SMB_CONNHANDLES_MAX)

#define SMB_OBJTYPE_HANDLE			7
#define SMB_CHECK_HANDLE(hndl)		\
{									\
	if(((hndl)==SMB_HANDLE_NULL) || (LWP_OBJTYPE(hndl)!=SMB_OBJTYPE_HANDLE))	\
		return NULL;				\
}

struct _smbfile
{
	lwp_node node;
	u16 sfid;
	SMBCONN conn;
};

/**
 * NBT/SMB Wrapper 
 */
typedef struct _nbtsmb
{
  u8 msg;		 /*** NBT Message ***/
  u8 flags;		 /*** Not much here really ***/
  u16 length;	 /*** Length, excluding NBT ***/
  u8 smb[SMB_MAX_TRANSMIT_SIZE];		 /*** Wii Actual is 7240 bytes ***/
} NBTSMB;

/**
 * Session Information
 */
typedef struct _smbsession
{
  u16 TID;
  u16 PID;
  u16 UID;
  u16 MID;
  u32 sKey;
  u16 MaxBuffer;
  u16 MaxMpx;
  u16 MaxVCS;
  u8 challenge[10];
  u8 p_domain[64];
  u16 sid;
  u16 count;
  u16 eos;
} SMBSESSION;

typedef struct _smbhandle
{
	lwp_obj object;
	char *user;
	char *pwd;
	char *share_name;
	char *server_name;
	s32 sck_server;
	struct sockaddr_in server_addr;
	BOOL conn_valid;
	SMBSESSION session;
	NBTSMB message;
} SMBHANDLE;

static u32 smb_dialectcnt = 1;
static BOOL smb_inited = FALSE;
static lwp_objinfo smb_handle_objects;
static lwp_queue smb_filehandle_queue;
static struct _smbfile smb_filehandles[SMB_FILEHANDLES_MAX];
static const char *smb_dialects[] = {"NT LM 0.12",NULL};

extern void ntlm_smb_nt_encrypt(const char *passwd, const u8 * challenge, u8 * answer);

/**
 * SMB Endian aware supporting functions 
 *
 * SMB always uses Intel Little-Endian values, so htons etc are
 * of little or no use :) ... Thanks M$
 */

/*** get unsigned char ***/
static __inline__ u8 getUChar(u8 *buffer,u32 offset)
{
	return (u8)buffer[offset];
}

/*** set unsigned char ***/
static __inline__ void setUChar(u8 *buffer,u32 offset,u8 value)
{
	buffer[offset] = value;
}

/*** get unsigned short ***/
static __inline__ u16 getUShort(u8 *buffer,u32 offset)
{
	return (u16)((buffer[offset+1]<<8)|(buffer[offset]));
}

/*** set unsigned short ***/
static __inline__ void setUShort(u8 *buffer,u32 offset,u16 value)
{
	buffer[offset] = (value&0xff);
	buffer[offset+1] = ((value&0xff00)>>8);
}

/*** get unsigned int ***/
static __inline__ u32 getUInt(u8 *buffer,u32 offset)
{
	return (u32)((buffer[offset+3]<<24)|(buffer[offset+2]<<16)|(buffer[offset+1]<<8)|buffer[offset]);
}

/*** set unsigned int ***/
static __inline__ void setUInt(u8 *buffer,u32 offset,u32 value)
{
	buffer[offset] = (value&0xff);
	buffer[offset+1] = ((value&0xff00)>>8);
	buffer[offset+2] = ((value&0xff0000)>>16);
	buffer[offset+3] = ((value&0xff000000)>>24);
}

static __inline__ SMBHANDLE* __smb_handle_open(SMBCONN smbhndl)
{
	u32 level;
	SMBHANDLE *handle;

	SMB_CHECK_HANDLE(smbhndl);

	_CPU_ISR_Disable(level);
	handle = (SMBHANDLE*)__lwp_objmgr_getnoprotection(&smb_handle_objects,LWP_OBJMASKID(smbhndl));
	_CPU_ISR_Restore(level);
	return handle;
}


static __inline__ void __smb_handle_free(SMBHANDLE *handle)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_objmgr_close(&smb_handle_objects,&handle->object);
	__lwp_objmgr_free(&smb_handle_objects,&handle->object);
	_CPU_ISR_Restore(level);
}

static void __smb_init()
{
	smb_inited = TRUE;
	__lwp_objmgr_initinfo(&smb_handle_objects,SMB_CONNHANDLES_MAX,sizeof(SMBHANDLE));
	__lwp_queue_initialize(&smb_filehandle_queue,smb_filehandles,SMB_FILEHANDLES_MAX,sizeof(struct _smbfile));
}

static SMBHANDLE* __smb_allocate_handle()
{
	u32 level;
	SMBHANDLE *handle;

	_CPU_ISR_Disable(level);
	handle = (SMBHANDLE*)__lwp_objmgr_allocate(&smb_handle_objects);
	if(handle) {
		handle->user = NULL;
		handle->pwd = NULL;
		handle->server_name = NULL;
		handle->share_name = NULL;
		handle->sck_server = INVALID_SOCKET;
		handle->conn_valid = FALSE;
		__lwp_objmgr_open(&smb_handle_objects,&handle->object);
	}
	_CPU_ISR_Restore(level);
	return handle;
}

static void __smb_free_handle(SMBHANDLE *handle)
{
	if(handle->user) free(handle->user);
	if(handle->pwd) free(handle->pwd);
	if(handle->server_name) free(handle->server_name);
	if(handle->share_name) free(handle->share_name);

	handle->user = NULL;
	handle->pwd = NULL;
	handle->server_name = NULL;
	handle->share_name = NULL;
	handle->sck_server = INVALID_SOCKET;
	
	__smb_handle_free(handle);
}

static void MakeSMBHeader(u8 command,u8 flags,u16 flags2,SMBHANDLE *handle)
{
	u8 *ptr = handle->message.smb;
	NBTSMB *nbt = &handle->message;
	SMBSESSION *sess = &handle->session;

	memset(nbt,0,sizeof(NBTSMB));
	
	setUInt(ptr,SMB_OFFSET_PROTO,SMB_PROTO);
	setUChar(ptr,SMB_OFFSET_CMD,command);
	setUChar(ptr,SMB_OFFSET_FLAGS,flags);
	setUShort(ptr,SMB_OFFSET_FLAGS2,flags2);
	setUShort(ptr,SMB_OFFSET_TID,sess->TID);
	setUShort(ptr,SMB_OFFSET_PID,sess->PID);
	setUShort(ptr,SMB_OFFSET_UID,sess->UID);
	setUShort(ptr,SMB_OFFSET_MID,sess->MID);
	
	ptr[SMB_HEADER_SIZE] = 0;
}

/**
 * MakeTRANS2Hdr
 */
static void MakeTRANS2Header(u8 subcommand,SMBHANDLE *handle)
{
	u8 *ptr = handle->message.smb;
	SMBSESSION *sess = &handle->session;

	setUChar(ptr, T2_WORD_CNT, 15);
	setUShort(ptr, T2_MAXPRM_CNT, 10);
	setUShort(ptr, T2_MAXBUFFER, sess->MaxBuffer);
	setUChar(ptr, T2_SSETUP_CNT, 1);
	setUShort(ptr, T2_SUB_CMD, subcommand);
}


/**
 * SMBCheck
 *
 * Do very basic checking on the return SMB
 */
static s32 SMBCheck(u8 command, s32 readlen,SMBHANDLE *handle)
{
	s32 ret,recvd;
	u8 *ptr = handle->message.smb;
	NBTSMB *nbt = &handle->message;

	memset(nbt,0,sizeof(NBTSMB));
	recvd = net_recv(handle->sck_server,nbt,readlen,0);
	if(recvd<12) return SMB_BAD_DATALEN;

	/*** Do basic SMB Header checks ***/
	ret = getUInt(ptr,SMB_OFFSET_PROTO);
	if(ret!=SMB_PROTO) return SMB_BAD_PROTOCOL;

	ret = getUChar(ptr, SMB_OFFSET_CMD);
	if(ret!=command) return SMB_BAD_COMMAND;

	ret = getUInt(ptr,SMB_OFFSET_NTSTATUS);
	if(ret) return SMB_ERROR;

	return SMB_SUCCESS;
}

/**
 * SMB_SetupAndX
 *
 * Setup the SMB session, including authentication with the 
 * magic 'NTLM Response'
 */
static s32 SMB_SetupAndX(SMBHANDLE *handle)
{
	s32 pos;
	s32 bcpos;
	s32 i, ret;
	u8 *ptr = handle->message.smb;
	SMBSESSION *sess = &handle->session;
	char pwd[200], ntRespData[24];

	MakeSMBHeader(SMB_SETUP_ANDX,0x08,0x01,handle);
	pos = SMB_HEADER_SIZE;

	setUChar(ptr,pos,13);
	pos++;				    /*** Word Count ***/
	setUChar(ptr,pos,0xff);
	pos++;				      /*** Next AndX ***/
	pos++;   /*** Reserved ***/
	pos += 2; /*** Next AndX Offset ***/
	setUShort(ptr,pos,sess->MaxBuffer);
	pos += 2;
	setUShort(ptr,pos,sess->MaxMpx);
	pos += 2;
	setUShort(ptr,pos,sess->MaxVCS);
	pos += 2;
	setUInt(ptr,pos,sess->sKey);
	pos += 4;
	setUShort(ptr,pos,0);	/*** Password length (case-insensitive) ***/
	pos += 2;
	setUShort(ptr,pos,24);	/*** Password length (case-sensitive) ***/
	pos += 2;
	pos += 4; /*** Reserved ***/
	pos += 4; /*** Capabilities ***/
	bcpos = pos;
	pos += 2; /*** Byte count ***/

	/*** The magic 'NTLM Response' ***/
	strcpy(pwd, handle->pwd);
	ntlm_smb_nt_encrypt((const char *) pwd, (const u8 *) sess->challenge, (u8*) ntRespData);

	/*** Build information ***/
	memcpy(&ptr[pos],ntRespData,24);
	pos += 24;

	/*** Account ***/
	strcpy(pwd, handle->user);
	for(i=0;i<strlen(pwd);i++) pwd[i] = toupper(pwd[i]);
	memcpy(&ptr[pos],pwd,strlen(pwd));
	pos += strlen(pwd)+1;

	/*** Primary Domain ***/
	if(handle->user[0]=='\0') sess->p_domain[0] = '\0';
	memcpy(&ptr[pos],sess->p_domain,strlen((const char*)sess->p_domain));
	pos += strlen((const char*)sess->p_domain)+1;

	/*** Native OS ***/
	strcpy(pwd,"Unix (libOGC)");
	memcpy(&ptr[pos],pwd,strlen(pwd));
	pos += strlen(pwd)+1;

	/*** Native LAN Manager ***/
	strcpy(pwd,"Nintendo Wii");
	memcpy(&ptr[pos],pwd,strlen(pwd));
	pos += strlen (pwd)+1;

	/*** Update byte count ***/
	setUShort(ptr,bcpos,((pos-bcpos)-2));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons (pos);
	pos += 4;

	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);

	if((ret=SMBCheck(SMB_SETUP_ANDX,sizeof(NBTSMB),handle))==SMB_SUCCESS) {
		/*** Collect UID ***/
		sess->UID = getUShort(handle->message.smb,SMB_OFFSET_UID);
		return SMB_SUCCESS;
	}
	return ret;
}

/**
 * SMB_TreeAndX
 *
 * Finally, net_connect to the remote share
 */
static s32 SMB_TreeAndX(SMBHANDLE *handle)
{
	s32 pos, bcpos, ret;
	u8 path[256];
	u8 *ptr = handle->message.smb;
	SMBSESSION *sess = &handle->session;

	MakeSMBHeader(SMB_TREEC_ANDX,0x08,0x01,handle);
	pos = SMB_HEADER_SIZE;

	setUChar(ptr,pos,4);
	pos++;				    /*** Word Count ***/
	setUChar(ptr,pos,0xff);
	pos++;				    /*** Next AndX ***/
	pos++;   /*** Reserved ***/
	pos += 2; /*** Next AndX Offset ***/
	pos += 2; /*** Flags ***/
	setUShort(ptr,pos,1);
	pos += 2;				    /*** Password Length ***/
	bcpos = pos;
	pos += 2;
	pos++;    /*** NULL Password ***/

	/*** Build server share path ***/
	strcpy ((char*)path, "\\\\");
	strcat ((char*)path, handle->server_name);
	strcat ((char*)path, "\\");
	strcat ((char*)path, handle->share_name);
	for(ret=0;ret<strlen((const char*)path);ret++) path[ret] = toupper(path[ret]);

	memcpy(&ptr[pos],path,strlen((const char*)path));
	pos += strlen((const char*)path)+1;

	/*** Service ***/
	strcpy((char*)path,"?????");
	memcpy(&ptr[pos],path,strlen((const char*)path));
	pos += strlen((const char*)path)+1;

	/*** Update byte count ***/
	setUShort(ptr,bcpos,(pos-bcpos)-2);

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons (pos);
	pos += 4;

	ret = net_send(handle->sck_server,(char *)&handle->message,pos,0);

	if((ret=SMBCheck(SMB_TREEC_ANDX,sizeof(NBTSMB),handle))==SMB_SUCCESS) {
		/*** Collect Tree ID ***/
		sess->TID = getUShort(handle->message.smb,SMB_OFFSET_TID);
		return SMB_SUCCESS;
	}
	return ret;
}

/**
 * SMB_NegotiateProtocol
 *
 * The only protocol we admit to is 'NT LM 0.12'
 */
static s32 SMB_NegotiateProtocol(const char *dialects[],int dialectc,SMBHANDLE *handle)
{
	u8 *ptr;
	s32 pos;
	s32 bcnt,i,j;
	s32 ret,len;
	u16 bytecount;
	u32 serverMaxBuffer;
	SMBSESSION *sess;

	if(!handle || !dialects || dialectc<=0) 
		return SMB_ERROR;

	/*** Clear session variables ***/
	sess = &handle->session;
	memset(sess,0,sizeof(SMBSESSION));
	sess->PID = 0xdead;
	sess->MID = 1;

	MakeSMBHeader(SMB_NEG_PROTOCOL,0x08,0x01,handle);

	pos = SMB_HEADER_SIZE+3;
	ptr = handle->message.smb;
	for(i=0,bcnt=0;i<dialectc;i++) {
		len = strlen(dialects[i])+1;
		ptr[pos++] = '\x02';
		memcpy(&ptr[pos],dialects[i],len);
		pos += len;
		bcnt += len+1;
	}
	/*** Update byte count ***/
	setUShort(ptr,(SMB_HEADER_SIZE+1),bcnt);

	/*** Set NBT information ***/
	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);
	pos += 4;
	
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<=0) return SMB_ERROR;

	/*** Check response ***/
	if((ret=SMBCheck(SMB_NEG_PROTOCOL,sizeof(NBTSMB),handle))==SMB_SUCCESS) {
		pos = SMB_HEADER_SIZE;
		ptr = handle->message.smb;

		/*** Collect information ***/
		if(getUChar(ptr,pos)!=17) return SMB_PROTO_FAIL;	// UCHAR WordCount; Count of parameter words = 17

		pos++;
		if(getUShort(ptr,pos)) return SMB_PROTO_FAIL;	// USHORT DialectIndex; Index of selected dialect - should always be 0 since we only supplied 1!

		pos += 2;
		if(getUChar(ptr,pos)!=3) return SMB_NOT_USER;	//UCHAR SecurityMode; Security mode:

		pos++;
		sess->MaxMpx = getUShort(ptr, pos);	//USHORT MaxMpxCount; Max pending outstanding requests

		pos += 2;
		sess->MaxVCS = getUShort(ptr, pos);	//USHORT MaxNumberVcs; Max VCs between client and server

		pos += 2;
		serverMaxBuffer = getUInt(ptr, pos);	//ULONG MaxBufferSize; Max transmit buffer size
		if(serverMaxBuffer>SMB_MAX_TRANSMIT_SIZE)
			sess->MaxBuffer = SMB_MAX_TRANSMIT_SIZE;
		else
			sess->MaxBuffer = serverMaxBuffer;

		pos += 4;
		pos += 4;	//ULONG MaxRawSize; Maximum raw buffer size
		sess->sKey = getUInt(ptr,pos);
		pos += 4;	
		pos += 4;	//ULONG Capabilities; Server capabilities
		pos += 4;	//ULONG SystemTimeLow; System (UTC) time of the server (low).
		pos += 4;	//ULONG SystemTimeHigh; System (UTC) time of the server (high).
		pos += 2;	//USHORT ServerTimeZone; Time zone of server (minutes from UTC)
		if(getUChar(ptr,pos)!=8) return SMB_BAD_KEYLEN;	//UCHAR EncryptionKeyLength - 0 or 8

		pos++;
		bytecount = getUShort(ptr,pos);

		/*** Copy challenge key ***/
		pos += 2;
		memcpy(&sess->challenge,&ptr[pos],8);

		/*** Primary domain ***/
		pos += 8;
		i = j = 0;
		while(ptr[pos+j]!=0) {
			sess->p_domain[i] = ptr[pos+j];
			j += 2;
			i++;
		}
		sess->p_domain[i] = '\0';

		return SMB_SUCCESS;
	}

	return ret;
}

static s32 do_netconnect(SMBHANDLE *handle)
{
	u32 nodelay;
	s32 ret;
	s32 sock;

	/*** Create the global net_socket ***/
	sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(sock==INVALID_SOCKET) return -1;

	/*** Switch off Nagle, ON TCP_NODELAY ***/
	nodelay = 1;
	ret = net_setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&nodelay,sizeof(nodelay));

	ret = net_connect(sock,(struct sockaddr*)&handle->server_addr,sizeof(handle->server_addr));
	if(ret) {
		net_close(sock);
		return -1;
	}

	handle->sck_server = sock;
	return 0;
}

static s32 do_smbconnect(SMBHANDLE *handle)
{
	s32 ret;

	ret = SMB_NegotiateProtocol(smb_dialects,smb_dialectcnt,handle);
	if(ret!=SMB_SUCCESS) {
		net_close(handle->sck_server);
		return -1;
	}
	
	ret = SMB_SetupAndX(handle);
	if(ret!=SMB_SUCCESS) {
		net_close(handle->sck_server);
		return -1;
	}

	ret = SMB_TreeAndX(handle);
	if(ret!=SMB_SUCCESS) {
		net_close(handle->sck_server);
		return -1;
	}

	handle->conn_valid = TRUE;
	return 0;
}

/****************************************************************************
 * Primary setup, logon and connection all in one :)
 ****************************************************************************/
s32 SMB_Connect(SMBCONN *smbhndl, const char *user, const char *password, const char *share, const char *IP)
{
	s32 ret;
	SMBHANDLE *handle;

	*smbhndl = SMB_HANDLE_NULL;

	if(smb_inited==FALSE) {
		u32 level;
		_CPU_ISR_Disable(level);
		if(smb_inited==FALSE) __smb_init();
		_CPU_ISR_Restore(level);
	}
	
	handle = __smb_allocate_handle();
	if(!handle) return SMB_ERROR;

	handle->user = strdup(user);
	handle->pwd = strdup(password);
	handle->server_name = strdup(IP);
	handle->share_name = strdup(share);

	handle->server_addr.sin_family = AF_INET;
	handle->server_addr.sin_port = htons(445);
	handle->server_addr.sin_addr.s_addr = inet_addr(IP);

	ret = do_netconnect(handle);
	if(ret==0) ret = do_smbconnect(handle);
	if(ret!=0) {
		__smb_free_handle(handle);
		return SMB_ERROR;
	}
	*smbhndl =(SMBCONN)(LWP_OBJMASKTYPE(SMB_OBJTYPE_HANDLE)|LWP_OBJMASKID(handle->object.id));
	return SMB_SUCCESS;
}


/****************************************************************************
 * SMB_Destroy
 *
 * Probably NEVER called on GameCube, but here for completeness
 ****************************************************************************/
void SMB_Close(SMBCONN smbhndl)
{
	SMBHANDLE *handle;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return;

	if(handle->sck_server!=INVALID_SOCKET)	net_close(handle->sck_server);
	__smb_free_handle(handle);
}

s32 SMB_Reconnect(SMBCONN smbhndl, BOOL test_conn)
{
	s32 ret = SMB_SUCCESS;
	SMBHANDLE *handle = __smb_handle_open(smbhndl);
	if(!handle)
		return SMB_ERROR; // we have no handle, so we can't reconnect

	if(handle->conn_valid && test_conn)
	{
		SMBDIRENTRY dentry;
		SMB_PathInfo("\\", &dentry, smbhndl);
	}

	if(!handle->conn_valid)
	{
		// save connection details
		const char * user = strdup(handle->user);
		const char * pwd = strdup(handle->pwd);
		const char * ip = strdup(handle->server_name);
		const char * share = strdup(handle->share_name);

		// shut down connection, and reopen
		SMB_Close(smbhndl);
		ret = SMB_Connect(&smbhndl, user, pwd, share, ip);
	}
	return ret;
}

SMBFILE SMB_OpenFile(const char *filename, u16 access, u16 creation,SMBCONN smbhndl)
{
	s32 pos;
	s32 bpos,ret;
	u8 *ptr;
	struct _smbfile *fid = NULL;
	SMBHANDLE *handle;
	char realfile[256];

	if(SMB_Reconnect(smbhndl,TRUE)!=SMB_SUCCESS) return NULL;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return NULL;

	MakeSMBHeader(SMB_OPEN_ANDX,0x08,0x01,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 15);
	pos++;			       /*** Word Count ***/
	setUChar(ptr, pos, 0xff);
	pos++;				 /*** Next AndX ***/
	pos += 3;  /*** Next AndX Offset ***/

	pos += 2;  /*** Flags ***/
	setUShort(ptr, pos, access);
	pos += 2;					 /*** Access mode ***/
	setUShort(ptr, pos, 0x6);
	pos += 2;				       /*** Type of file ***/
	pos += 2;  /*** Attributes ***/
	pos += 4;  /*** File time - don't care - let server decide ***/
	setUShort(ptr, pos, creation);
	pos += 2;				       /*** Creation flags ***/
	pos += 4;  /*** Allocation size ***/
	pos += 8;  /*** Reserved ***/
	pos += 2;  /*** Byte Count ***/
	bpos = pos;

	if (filename[0] != '\\') {
		strcpy(realfile, "\\");
		strcat(realfile,filename);
	} else
		strcpy(realfile,filename);

	memcpy(&ptr[pos],realfile,strlen(realfile));
	pos += strlen(realfile)+1;

	setUShort(ptr,(bpos-2),(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<0) goto failed;

	if(SMBCheck(SMB_OPEN_ANDX,sizeof(NBTSMB),handle)==SMB_SUCCESS) {
		/*** Check file handle ***/
		fid = (struct _smbfile*)__lwp_queue_get(&smb_filehandle_queue);
		if(fid) {
			fid->conn = smbhndl;
			fid->sfid = getUShort(handle->message.smb,(SMB_HEADER_SIZE+5));
		}
	}
	return (SMBFILE)fid;

failed:
	handle->conn_valid = FALSE;
	return NULL;
}

/**
 * SMB_Close
 */
void SMB_CloseFile(SMBFILE sfid)
{
	u8 *ptr;
	s32 pos, ret;
	SMBHANDLE *handle;
	struct _smbfile *fid = (struct _smbfile*)sfid;

	if(!fid) return;

	handle = __smb_handle_open(fid->conn);
	if(!handle) return;

	MakeSMBHeader(SMB_CLOSE,0x08,0x01,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 3);
	pos++;			      /** Word Count **/
	setUShort(ptr, pos, fid->sfid);
	pos += 2;
	setUInt(ptr, pos, 0xffffffff);
	pos += 4;					/*** Last Write ***/
	pos += 2;  /*** Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<0) handle->conn_valid = FALSE;

	SMBCheck(SMB_CLOSE,sizeof(NBTSMB),handle);
	__lwp_queue_append(&smb_filehandle_queue,&fid->node);
}

/**
 * SMB_Read
 */
s32 SMB_ReadFile(char *buffer, int size, int offset, SMBFILE sfid)
{
	u8 *ptr,*dest;
	s32 pos, ret, ofs;
	u16 length = 0;
	SMBHANDLE *handle;
	struct _smbfile *fid = (struct _smbfile*)sfid;

	if(!fid) return 0;

	/*** Don't let the size exceed! ***/
	if(size>SMB_MAX_BUFFERSIZE) return 0;

	handle = __smb_handle_open(fid->conn);
	if(!handle) return 0;

	MakeSMBHeader(SMB_READ_ANDX,0x08,0x01,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 10);
	pos++;				      /*** Word count ***/
	setUChar(ptr, pos, 0xff);
	pos++;
	pos += 3;	    /*** Reserved, Next AndX Offset ***/
	setUShort(ptr, pos, fid->sfid);
	pos += 2;					    /*** FID ***/
	setUInt(ptr, pos, offset);
	pos += 4;						 /*** Offset ***/

	setUShort(ptr, pos, size & 0xffff);
	pos += 2;
	setUShort(ptr, pos, size & 0xffff);
	pos += 2;
	setUInt(ptr, pos, 0);
	pos += 4;
	pos += 2;	    /*** Remaining ***/
	pos += 2;	    /*** Byte count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server,(char*)&handle->message, pos, 0);
	if(ret<0) goto  failed;

	/*** SMBCheck should now only read up to the end of a standard header ***/
	if((ret=SMBCheck(SMB_READ_ANDX,(SMB_HEADER_SIZE+27+4),handle))==SMB_SUCCESS) {
		ptr = handle->message.smb;
		/*** Retrieve data length for this packet ***/
		length = getUShort(ptr,(SMB_HEADER_SIZE+11));
		/*** Retrieve offset to data ***/
		ofs = getUShort(ptr,(SMB_HEADER_SIZE+13));

		/*** Default offset, with no padding is 59, so grab any outstanding padding ***/
		if(ofs>SMB_DEF_READOFFSET) {
			char pad[1024];
			ret = net_recv(handle->sck_server,pad,(ofs-SMB_DEF_READOFFSET), 0);
			if(ret<0) return ret;
		}

		/*** Finally, go grab the data ***/
		ofs = 0;
		dest = (u8*)buffer;
		if(length>0) {
			while ((ret=net_recv(handle->sck_server,&dest[ofs],length, 0))!=0) {
				if(ret<0) return ret;

				ofs += ret;
				if (ofs>=length) break;
			}
		}
		return ofs;
	}
	return 0;

failed:
	handle->conn_valid = FALSE;
	return ret;
}

/**
 * SMB_Write
 */
s32 SMB_WriteFile(const char *buffer, int size, int offset, SMBFILE sfid)
{
	u8 *ptr,*src;
	s32 pos, ret;
	s32 blocks64;
	u32 copy_len;
	SMBHANDLE *handle;
	struct _smbfile *fid = (struct _smbfile*)sfid;

	if(!fid) return 0;

	handle = __smb_handle_open(fid->conn);
	if(!handle) return SMB_ERROR;

	MakeSMBHeader(SMB_WRITE_ANDX,0x08,0x01,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 12);
	pos++;				  /*** Word Count ***/
	setUChar(ptr, pos, 0xff);
	pos += 2;				   /*** Next AndX ***/
	pos += 2; /*** Next AndX Offset ***/

	setUShort(ptr, pos, fid->sfid);
	pos += 2;
	setUInt(ptr, pos, offset);
	pos += 4;
	pos += 4; /*** Reserved ***/
	pos += 2; /*** Write Mode ***/
	pos += 2; /*** Remaining ***/

	blocks64 = size >> 16;

	setUShort(ptr, pos, blocks64);
	pos += 2;				       /*** Length High ***/
	setUShort(ptr, pos, size & 0xffff);
	pos += 2;					    /*** Length Low ***/
	setUShort(ptr, pos, 59);
	pos += 2;				 /*** Data Offset ***/
	setUShort(ptr, pos, size & 0xffff);
	pos += 2;					    /*** Data Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos+size);
	
	src = (u8*)buffer;
	copy_len = size;
	if((copy_len+pos)>SMB_MAX_TRANSMIT_SIZE) copy_len = (SMB_MAX_TRANSMIT_SIZE-pos);

	memcpy(&ptr[pos],src,copy_len);
	size -= copy_len;
	src += copy_len;
	pos += copy_len;

	pos += 4;

	/*** Send Header Information ***/
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<0) goto failed;

	if(size>0) {
		/*** Send the data ***/
		ret = net_send(handle->sck_server,src,size,0);
		if(ret<0) goto failed;
	}

	ret = 0;
	if(SMBCheck(SMB_WRITE_ANDX,sizeof(NBTSMB),handle)==SMB_SUCCESS) {
		ptr = handle->message.smb;
		ret = getUShort(ptr,(SMB_HEADER_SIZE+5));
	}

	return ret;

failed:
	handle->conn_valid = FALSE;
	return ret;
}

/**
* SMB_PathInfo
*/
s32 SMB_PathInfo(const char *filename, SMBDIRENTRY *sdir, SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	s32 bpos;
	SMBHANDLE *handle;
	char realfile[256];

	handle = __smb_handle_open(smbhndl);
	if (!handle) return SMB_ERROR;

	MakeSMBHeader(SMB_TRANS2, 0x08, 0x01, handle);
	MakeTRANS2Header(SMB_QUERY_PATH_INFO, handle);

	bpos = pos = (T2_BYTE_CNT + 2);
	pos += 3; /*** Padding ***/
	ptr = handle->message.smb;
	setUShort(ptr, pos, 0x107); //SMB_QUERY_FILE_ALL_INFO

	pos += 2;
	setUInt(ptr, pos, 0);
	pos += 4; /*** reserved ***/

	if (filename[0] != '\\') {
		strcpy(realfile, "\\");
		strcat(realfile, filename);
	} else
		strcpy(realfile, filename);

	memcpy(&ptr[pos], realfile, strlen(realfile));
	pos += strlen(realfile) + 1; /*** Include padding ***/

	/*** Update counts ***/
	setUShort(ptr, T2_PRM_CNT, (7 + strlen(realfile)));
	setUShort(ptr, T2_SPRM_CNT, (7 + strlen(realfile)));
	setUShort(ptr, T2_SPRM_OFS, 68);
	setUShort(ptr, T2_SDATA_OFS, (75 + strlen(realfile)));
	setUShort(ptr, T2_BYTE_CNT, (pos - bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server, (char*) &handle->message, pos, 0);
	if(ret<0) goto failed;

	ret = SMB_ERROR;
	if (SMBCheck(SMB_TRANS2, sizeof(NBTSMB), handle) == SMB_SUCCESS) {

		ptr = handle->message.smb;
		/*** Get parameter offset ***/
		pos = getUShort(ptr, (SMB_HEADER_SIZE + 9));
		sdir->attributes = getUShort(ptr,pos+36);

		pos += 52;
		sdir->size_low = getUInt(ptr, pos);
		pos += 4;
		sdir->size_high = getUInt(ptr, pos);
		pos += 4;
		strcpy(sdir->name,realfile);

		ret = SMB_SUCCESS;
	}
	return ret;

failed:
	handle->conn_valid = FALSE;
	return ret;
}

/**
 * SMB_FindFirst
 *
 * Uses TRANS2 to support long filenames
 */
s32 SMB_FindFirst(const char *filename, unsigned short flags, SMBDIRENTRY *sdir, SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	s32 bpos;
	SMBHANDLE *handle;
	SMBSESSION *sess;

	if(SMB_Reconnect(smbhndl,TRUE)!=SMB_SUCCESS) return SMB_ERROR;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return SMB_ERROR;

	sess = &handle->session;
	MakeSMBHeader(SMB_TRANS2,0x08,0x01,handle);
	MakeTRANS2Header(SMB_FIND_FIRST2,handle);

	bpos = pos = (T2_BYTE_CNT+2);
	pos += 3;	     /*** Padding ***/
	ptr = handle->message.smb;
	setUShort(ptr, pos, flags);
	pos += 2;					  /*** Flags ***/
	setUShort(ptr, pos, 1);
	pos += 2;					  /*** Count ***/
	setUShort(ptr, pos, 6);
	pos += 2;					  /*** Internal Flags ***/
	setUShort(ptr, pos, 260);
	pos += 2;					  /*** Level of Interest ***/
	pos += 4;    /*** Storage Type == 0 ***/
	memcpy(&ptr[pos], filename, strlen(filename));
	pos += strlen(filename)+1;			   /*** Include padding ***/

	/*** Update counts ***/
	setUShort(ptr, T2_PRM_CNT, (13+strlen(filename)));
	setUShort(ptr, T2_SPRM_CNT, (13+strlen(filename)));
	setUShort(ptr, T2_SPRM_OFS, 68);
	setUShort(ptr, T2_SDATA_OFS,(81+strlen(filename)));
	setUShort(ptr, T2_BYTE_CNT,(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<0) goto failed;

	sess->eos = 1;
	sess->sid = 0;
	sess->count = 0;
	ret = SMB_ERROR;
	if(SMBCheck(SMB_TRANS2,sizeof(NBTSMB),handle)==SMB_SUCCESS) {
		ptr = handle->message.smb;
		/*** Get parameter offset ***/
		pos = getUShort(ptr,(SMB_HEADER_SIZE+9));
		sess->sid = getUShort(ptr, pos);
		pos += 2;
		sess->count = getUShort(ptr, pos);
		pos += 2;
		sess->eos = getUShort(ptr, pos);
		pos += 2;
		pos += 46;

		if (sess->count) {
			sdir->size_low = getUInt(ptr, pos);
			pos += 4;
			sdir->size_high = getUInt(ptr, pos);
			pos += 4;
			pos += 8;
			sdir->attributes = getUInt(ptr, pos);
			pos += 38;
			strcpy(sdir->name, (const char*)&ptr[pos]);

			ret = SMB_SUCCESS;
		}
	}
	return ret;

failed:
	handle->conn_valid = FALSE;
	return ret;
}

/**
 * SMB_FindNext
 */
s32 SMB_FindNext(SMBDIRENTRY *sdir,SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	s32 bpos;
	SMBHANDLE *handle;
	SMBSESSION *sess;

	if(SMB_Reconnect(smbhndl,TRUE)!=SMB_SUCCESS) return SMB_ERROR;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return SMB_ERROR;
	
	sess = &handle->session;
	if(sess->eos || sess->sid==0) return SMB_ERROR;

	MakeSMBHeader(SMB_TRANS2,0x08,0x01,handle);
	MakeTRANS2Header(SMB_FIND_NEXT2,handle);

	bpos = pos = (T2_BYTE_CNT+2);
	pos += 3;	     /*** Padding ***/
	ptr = handle->message.smb;
	setUShort(ptr, pos, sess->sid);
	pos += 2;						    /*** Search ID ***/
	setUShort(ptr, pos, 1);
	pos += 2;					  /*** Count ***/
	setUShort(ptr, pos, 260);
	pos += 2;					  /*** Level of Interest ***/
	pos += 4;    /*** Storage Type == 0 ***/
	setUShort(ptr, pos, 12);
	pos+=2; /*** Search flags ***/
	pos++;

	/*** Update counts ***/
	setUShort(ptr, T2_PRM_CNT, 13);
	setUShort(ptr, T2_SPRM_CNT, 13);
	setUShort(ptr, T2_SPRM_OFS, 68);
	setUShort(ptr, T2_SDATA_OFS, 81);
	setUShort(ptr, T2_BYTE_CNT, (pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<0) goto failed;

	ret = SMB_ERROR;
	if (SMBCheck(SMB_TRANS2,sizeof(NBTSMB),handle)==SMB_SUCCESS) {
		ptr = handle->message.smb;
		/*** Get parameter offset ***/
		pos = getUShort(ptr,(SMB_HEADER_SIZE+9));
		sess->count = getUShort(ptr, pos);
		pos += 2;
		sess->eos = getUShort(ptr, pos);
		pos += 2;
		pos += 44;

		if (sess->count) {
			sdir->size_low = getUInt(ptr, pos);
			pos += 4;
			sdir->size_high = getUInt(ptr, pos);
			pos += 4;
			pos += 8;
			sdir->attributes = getUInt(ptr, pos);
			pos += 38;
			strcpy (sdir->name, (const char*)&ptr[pos]);

			ret = SMB_SUCCESS;
		}
	}
	return ret;

failed:
	handle->conn_valid = FALSE;
	return ret;
}

/**
 * SMB_FindClose
 */
s32 SMB_FindClose(SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	SMBHANDLE *handle;
	SMBSESSION *sess;

	if(SMB_Reconnect(smbhndl,TRUE)!=SMB_SUCCESS) return SMB_ERROR;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return SMB_ERROR;
	
	sess = &handle->session;
	if(sess->sid==0) return SMB_ERROR;

	MakeSMBHeader(SMB_FIND_CLOSE2,0x08,0x01,handle);

	pos  = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 1);
	pos++;					   /*** Word Count ***/
	setUShort(ptr, pos, sess->sid);
	pos += 2;
	pos += 2;  /*** Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = net_send(handle->sck_server,(char*)&handle->message,pos,0);
	if(ret<0) goto failed;

	ret = SMBCheck(SMB_FIND_CLOSE2,sizeof(NBTSMB),handle);
	return ret;

failed:
	handle->conn_valid = FALSE;
	return ret;
}
