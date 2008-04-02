/*-------------------------------------------------------------

es.c -- ETicket services

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)

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

#if defined(HW_RVL)

#include <stdio.h>
#include "ipc.h"
#include "asm.h"
#include "processor.h"
#include "es.h"

//#define DEBUG_ES

#define IOCTL_ES_ADDTICKET				0x01
#define IOCTL_ES_ADDTITLESTART			0x02
#define IOCTL_ES_ADDCONTENTSTART		0x03
#define IOCTL_ES_ADDCONTENTDATA			0x04
#define IOCTL_ES_ADDCONTENTFINISH		0x05
#define IOCTL_ES_ADDTITLEFINISH			0x06
#define IOCTL_ES_GETDEVICEID			0x07
#define IOCTL_ES_LAUNCH					0x08
#define IOCTL_ES_OPENCONTENT			0x09
#define IOCTL_ES_READCONTENT			0x0A
#define IOCTL_ES_CLOSECONTENT			0x0B
#define IOCTL_ES_GETOWNEDTITLECNT		0x0C
#define IOCTL_ES_GETOWNEDTITLES			0x0D
#define IOCTL_ES_GETTITLECNT			0x0E
#define IOCTL_ES_GETTITLES				0x0F
#define IOCTL_ES_GETTITLECONTENTSCNT	0x10
#define IOCTL_ES_GETTITLECONTENTS		0x11
#define IOCTL_ES_GETVIEWCNT				0x12
#define IOCTL_ES_GETVIEWS				0x13
#define IOCTL_ES_GETTMDVIEWCNT			0x14
#define IOCTL_ES_GETTMDVIEWS			0x15
//#define IOCTL_ES_GETCONSUMPTION		0x16
#define IOCTL_ES_DELETETITLE			0x17
//#define IOCTL_ES_DELETETICKET			0x18
//#define IOCTL_ES_DIGETTMDVIEWSIZE		0x19
//#define IOCTL_ES_DIGETTMDVIEW			0x1A
//#define IOCTL_ES_DIGETTICKETVIEW		0x1B
#define IOCTL_ES_DIVERIFY				0x1C
#define IOCTL_ES_GETTITLEDIR			0x1D
#define IOCTL_ES_GETDEVICECERT			0x1E
//#define IOCTL_ES_IMPORTBOOT			0x1F
#define IOCTL_ES_GETTITLEID				0x20
#define IOCTL_ES_SETUID					0x21
#define IOCTL_ES_DELETETITLECONTENT		0x22
#define IOCTL_ES_SEEKCONTENT			0x23
//#define IOCTL_ES_OPENTITLECONTENT		0x24
//#define IOCTL_ES_LAUNCHBC				0x25
//#define IOCTL_ES_EXPORTTITLEINIT		0x26
//#define IOCTL_ES_EXPORTCONTENTBEGIN	0x27
//#define IOCTL_ES_EXPORTCONTENTDATA	0x28
//#define IOCTL_ES_EXPORTCONTENTEND		0x29
//#define IOCTL_ES_EXPORTTITLEDONE		0x2A
#define IOCTL_ES_ADDTMD					0x2B
#define IOCTL_ES_ENCRYPT				0x2C
#define IOCTL_ES_DECRYPT				0x2D
#define IOCTL_ES_GETBOOT2VERSION		0x2E
#define IOCTL_ES_ADDTITLECANCEL			0x2F
#define IOCTL_ES_SIGN					0x30
//#define IOCTL_ES_VERIFYSIGN			0x31
#define IOCTL_ES_GETSTOREDCONTENTCNT	0x32
#define IOCTL_ES_GETSTOREDCONTENTS		0x33
#define IOCTL_ES_GETSTOREDTMDSIZE		0x34
#define IOCTL_ES_GETSTOREDTMD			0x35
#define IOCTL_ES_GETSHAREDCONTENTCNT	0x36
#define IOCTL_ES_GETSHAREDCONTENTS		0x37


#define ES_HEAP_SIZE 0x800

#define ISALIGNED(x) ((((u32)x)&0x1F)==0)

static char __es_fs[] ATTRIBUTE_ALIGN(32) = "/dev/es";

static s32 __es_fd = -1;
static s32 __es_hid = -1;

s32 __ES_Init(void)
{
	s32 ret = 0;

#ifdef DEBUG_ES
	printf("ES Init\n");
#endif	
	
	if(__es_hid <0 ) {
		__es_hid = iosCreateHeap(ES_HEAP_SIZE);
		if(__es_hid < 0) return __es_hid;
	}
	
	ret = IOS_Open(__es_fs,0);
	if(ret<0) return ret;
	__es_fd = ret;
	
#ifdef DEBUG_ES
	printf("ES FD %d\n",__es_fd);
#endif	
	
	return 0;
}

s32 __ES_Close(void)
{
	s32 ret;

	if(__es_fd < 0) return 0;
	
	ret = IOS_Close(__es_fd);
	
	// If close fails, I'd rather return error but still reset the fd.
	// Otherwise you're stuck with an uncloseable ES, and you can't open again either
	//if(ret<0) return ret;
	if(ret<0) {
		__es_fd = -1;
		return ret;
	}
	
	__es_fd = -1;
	return 0;
}

// Used by ios.c after reloading IOS
s32 __ES_Reset(void)
{
	__es_fd = -1;
	return 0;
}

s32 ES_GetTitleID(u64 *titleID)
{
	s32 ret;
	u64 title;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!titleID) return ES_EINVAL;
	
	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLEID,":q",&title);
	if(ret<0) return ret;

	*titleID = title;
	return ret;
}

s32 ES_SetUID(u64 uid)
{
	if(__es_fd<0) return ES_ENOTINIT;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_SETUID,"q:",uid);
}

s32 ES_GetDataDir(u64 titleID,char *filepath)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!filepath) return ES_EINVAL;
	if(!ISALIGNED(filepath)) return ES_EALIGN;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLEDIR,"q:d",titleID,filepath,30);

	return ret;
}

s32 ES_LaunchTitle(u64 titleID, const tikview *view)
{

	static u64 title ATTRIBUTE_ALIGN(32);
	static ioctlv vectors[2] ATTRIBUTE_ALIGN(32);
	s32 res;
	
	if(__es_fd<0) return ES_ENOTINIT;
	if(!view) return ES_EINVAL;
	if(!ISALIGNED(view)) return ES_EALIGN;

#ifdef DEBUG_ES
	printf("ES LaunchTitle %d %016llx 0x%08x 0x%02x\n",__es_fd,titleID,(u32)view,sizeof(tikview));
#endif	

	
	title = titleID;
	vectors[0].data = (void*)&title;
	vectors[0].len = sizeof(u64);
	vectors[1].data = (void*)view;
	vectors[1].len = sizeof(tikview);
	res = IOS_IoctlvReboot(__es_fd,IOCTL_ES_LAUNCH,2,0,vectors);
	
#ifdef DEBUG_ES
	printf(" =%d\n",res);
#endif	
	
	return res;
}

s32 ES_GetNumTicketViews(u64 titleID, u32 *cnt)
{
	s32 ret;
	u32 cntviews;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!cnt) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETVIEWCNT,"q:i",titleID,&cntviews);
	
	if(ret<0) return ret;
	*cnt = cntviews;
	return ret;
}

s32 ES_GetTicketViews(u64 titleID, tikview *views, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!views) return ES_EINVAL;
	if(!ISALIGNED(views)) return ES_EALIGN;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETVIEWS,"qi:d",titleID,cnt,views,sizeof(tikview)*cnt);
}

s32 ES_GetNumOwnedTitles(u32 *cnt)
{
	s32 ret;
	u32 cnttitles;

	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt == NULL) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETOWNEDTITLECNT,":i",&cnttitles);
	
	if(ret<0) return ret;
	*cnt = cnttitles;
	return ret;
}

s32 ES_GetOwnedTitles(u64 *titles, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!titles) return ES_EINVAL;
	if(!ISALIGNED(titles)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETOWNEDTITLES,"i:d",cnt,titles,sizeof(u64)*cnt);
}

s32 ES_GetNumTitles(u32 *cnt)
{
	s32 ret;
	u32 cnttitles;

	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt == NULL) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLECNT,":i",&cnttitles);
	
	if(ret<0) return ret;
	*cnt = cnttitles;
	return ret;
}

s32 ES_GetTitles(u64 *titles, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!titles) return ES_EINVAL;
	if(!ISALIGNED(titles)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLES,"i:d",cnt,titles,sizeof(u64)*cnt);
}

s32 ES_GetNumStoredTMDContents(const signed_blob *stmd, u32 tmd_size, u32 *cnt)
{
	s32 ret;
	u32 cntct;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!cnt) return ES_EINVAL;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDCONTENTCNT,"d:i",stmd,tmd_size,&cntct);
	
	if(ret<0) return ret;
	*cnt = cntct;
	return ret;
}

s32 ES_GetStoredTMDContents(const signed_blob *stmd, u32 tmd_size, u32 *contents, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!contents) return ES_EINVAL;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(!ISALIGNED(contents)) return ES_EALIGN;
	if(tmd_size > MAX_SIGNED_TMD_SIZE) return ES_EINVAL;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDCONTENTS,"di:d",stmd,tmd_size,cnt,contents,sizeof(u32)*cnt);
}

s32 ES_GetTitleContentsCount(u64 titleID, u32 *num)
{
	s32 ret;
	u32 _num;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!num) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLECONTENTSCNT,"q:i",titleID,&_num);
	
	if(ret<0) return ret;
	*num = _num;
	return ret;
}

s32 ES_GetTitleContents(u64 titleID, u8 *data, u32 size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(size <= 0) return ES_EINVAL;
	if(!data) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDTMD,"qi:d",titleID,size,data,size);
}

s32 ES_GetTMDViewSize(u64 titleID, u32 *size)
{
	s32 ret;
	u32 tmdsize;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!size) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTMDVIEWCNT,"q:i",titleID,&tmdsize);
	
	if(ret<0) return ret;
	*size = tmdsize;
	return ret;
}

s32 ES_GetTMDView(u64 titleID, u8 *data, u32 size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(size <= 0) return ES_EINVAL;
	if(!data) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTMDVIEWS,"qi:d",titleID,size,data,size);
}

s32 ES_GetStoredTMDSize(u64 titleID, u32 *size)
{
	s32 ret;
	u32 tmdsize;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!size) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDTMDSIZE,"q:i",titleID,&tmdsize);
	
	if(ret<0) return ret;
	*size = tmdsize;
	return ret;
}

s32 ES_GetStoredTMD(u64 titleID, signed_blob *stmd, u32 size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(size <= 0) return ES_EINVAL;
	if(!stmd) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(size > MAX_SIGNED_TMD_SIZE) return ES_EINVAL;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDTMD,"qi:d",titleID,size,stmd,size);
}

s32 ES_GetNumSharedContents(u32 *cnt)
{
	s32 ret;
	u32 cntct;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!cnt) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSHAREDCONTENTCNT,":i",&cntct);
	
	if(ret<0) return ret;
	*cnt = cntct;
	return ret;
}

s32 ES_GetSharedContents(sha1 *contents, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!contents) return ES_EINVAL;
	if(!ISALIGNED(contents)) return ES_EALIGN;
	
	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSHAREDCONTENTS,"i:d",cnt,contents,sizeof(sha1)*cnt);
}

signed_blob *ES_NextCert(const signed_blob *certs)
{
	cert_header *cert;
	if(!SIGNATURE_SIZE(certs)) return NULL;
	cert = SIGNATURE_PAYLOAD(certs);
	if(!CERTIFICATE_SIZE(cert)) return NULL;
	return (signed_blob*)(((u8*)cert) + CERTIFICATE_SIZE(cert));
}

s32 __ES_sanity_check_certlist(const signed_blob *certs, u32 certsize)
{
	int count = 0;
	signed_blob *end;
	
	if(!certs || !certsize) return 0;
	
	end = (signed_blob*)(((u8*)certs) + certsize);
	while(certs != end) {
#ifdef DEBUG_ES
		printf("Checking certificate at %p\n",certs);
#endif
		certs = ES_NextCert(certs);
		if(!certs) return 0;
		count++;
	}
#ifdef DEBUG_ES
	printf("Num of certificates: %d\n",count);
#endif
	return count;
}

s32 ES_Identify(const signed_blob *certificates, u32 certificates_size, const signed_blob *stmd, u32 tmd_size, const signed_blob *sticket, u32 ticket_size, u32 *keyid)
{

	tmd *p_tmd;
	u8 *hashes;
	s32 ret;
	u32 *keyid_buf;
	
	if(__es_fd<0) return ES_ENOTINIT;
	
	if(ticket_size != STD_SIGNED_TIK_SIZE) return ES_EINVAL;
	if(!stmd || !tmd_size || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!sticket || !IS_VALID_SIGNATURE(sticket)) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	if(!ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(!ISALIGNED(sticket)) return ES_EALIGN;
	if(tmd_size > MAX_SIGNED_TMD_SIZE) return ES_EINVAL;
	
	p_tmd = SIGNATURE_PAYLOAD(stmd);
	
	if(!(keyid_buf = iosAlloc(__es_hid, 4))) return ES_ENOMEM;
	if(!(hashes = iosAlloc(__es_hid, p_tmd->num_contents*20))) {
		iosFree(__es_hid, keyid_buf);
		return ES_ENOMEM;
	}
	
	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DIVERIFY, "dddd:id", certificates, certificates_size, 0, 0, sticket, ticket_size, stmd, tmd_size, keyid_buf, hashes, p_tmd->num_contents*20);
	if(ret >= 0 && keyid) *keyid = *keyid_buf;

	iosFree(__es_hid, keyid_buf);
	iosFree(__es_hid, hashes);
	return ret;
}

s32 ES_AddTicket(const signed_blob *stik, u32 stik_size, const signed_blob *certificates, u32 certificates_size, const signed_blob *crl, u32 crl_size)
{
	s32 ret;
	
	if(__es_fd<0) return ES_ENOTINIT;
	if(stik_size != STD_SIGNED_TIK_SIZE) return ES_EINVAL;
	if(!stik || !IS_VALID_SIGNATURE(stik)) return ES_EINVAL;
	if(crl_size && (!crl || !IS_VALID_SIGNATURE(crl))) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	if(!certificates || !ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(stik)) return ES_EALIGN;
	if(!ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(crl)) return ES_EALIGN;
	
	if(!crl_size) crl=NULL;
	
	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTICKET, "ddd:", stik, stik_size, certificates, certificates_size, crl, crl_size);
	return ret;

}

s32 ES_AddTitleTMD(const signed_blob *stmd, u32 stmd_size)
{
	s32 ret;
	
	if(__es_fd<0) return ES_ENOTINIT;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(stmd_size != SIGNED_TMD_SIZE(stmd)) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	
	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTMD, "d:", stmd, stmd_size);
	return ret;

}

s32 ES_AddTitleStart(const signed_blob *stmd, u32 tmd_size, const signed_blob *certificates, u32 certificates_size, const signed_blob *crl, u32 crl_size)
{
	s32 ret;
	
	if(__es_fd<0) return ES_ENOTINIT;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(tmd_size != SIGNED_TMD_SIZE(stmd)) return ES_EINVAL;
	if(crl_size && (!crl || !IS_VALID_SIGNATURE(crl))) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	if(!certificates || !ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(!ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(crl)) return ES_EALIGN;
	
	if(!crl_size) crl=NULL;
	
	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTITLESTART, "dddi:", stmd, tmd_size, certificates, certificates_size, crl, crl_size, 1);
	return ret;
}

s32 ES_AddContentStart(u64 titleID, u32 cid)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDCONTENTSTART, "qi:", titleID, cid);
}

s32 ES_AddContentData(s32 cfd, u8 *data, u32 data_size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	if(!data || !data_size) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDCONTENTDATA, "id:", cfd, data, data_size);
}

s32 ES_AddContentFinish(u32 cid)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDCONTENTFINISH, "i:", cid);
}

s32 ES_AddTitleFinish(void)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTITLEFINISH, "");
}

s32 ES_AddTitleCancel(void)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTITLECANCEL, "");
}

s32 ES_OpenContent(u16 index)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_OPENCONTENT, "i:", index);
}

s32 ES_ReadContent(s32 cfd, u8 *data, u32 data_size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	if(!data || !data_size) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_READCONTENT, "i:d", cfd, data, data_size);
}

s32 ES_SeekContent(s32 cfd, s32 where, s32 whence)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_SEEKCONTENT, "iii:", cfd, where, whence);
}

s32 ES_CloseContent(s32 cfd)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_CLOSECONTENT, "i:", cfd);
}

s32 ES_DeleteTitle(u64 titleID)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DELETETITLE, "q:", titleID);
}

s32 ES_DeleteTitleContent(u64 titleID)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DELETETITLECONTENT, "q:", titleID);
}

s32 ES_Encrypt(u32 keynum, u8 *iv, u8 *source, u32 size, u8 *dest)
{
	if(__es_fd<0) return ES_ENOTINIT;
 	if(!iv || !source || !size || !dest) return ES_EINVAL;
 	if(!ISALIGNED(source) || !ISALIGNED(iv) || !ISALIGNED(dest)) return ES_EALIGN;
 	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ENCRYPT, "idd:dd", keynum, iv, 0x10, source, size, iv, 0x10, dest, size);
}
 
s32 ES_Decrypt(u32 keynum, u8 *iv, u8 *source, u32 size, u8 *dest)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(!iv || !source || !size || !dest) return ES_EINVAL;
	if(!ISALIGNED(source) || !ISALIGNED(iv) || !ISALIGNED(dest)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DECRYPT, "idd:dd", keynum, iv, 0x10, source, size, iv, 0x10, dest, size);
}

s32 ES_Sign(u8 *source, u32 size, u8 *sig, u8 *certs)
{
	if(__es_fd<0) return ES_ENOTINIT;
 	if(!source || !size || !sig || !certs) return ES_EINVAL;
 	if(!ISALIGNED(source) || !ISALIGNED(sig) || !ISALIGNED(certs)) return ES_EALIGN;
 	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_SIGN, "d:dd", source, size, sig, 0x3C, certs, 0x180);
}

s32 ES_GetDeviceCert(u8 *outbuf)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(!outbuf) return ES_EINVAL;
	if(!ISALIGNED(outbuf)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_GETDEVICECERT, ":d", outbuf, 0x180);
}

s32 ES_GetDeviceID(u32 *device_id)
{
	s32 ret;
	u32 _device_id = 0;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!device_id) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_GETDEVICEID, ":i", &_device_id);
	if (ret>=0) *device_id = _device_id;

	return ret;
}

s32 ES_GetBoot2Version(u32 *version)
{
	s32 ret;
	u32 _version = 0;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!version) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_GETBOOT2VERSION, ":i", &_version);
	if (ret>=0) *version = _version;
	
	return ret;
}

#endif /* defined(HW_RVL) */
