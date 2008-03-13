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

#define IOCTL_ES_LAUNCH				0x08
#define IOCTL_ES_GETTITLECOUNT		0x0E
#define IOCTL_ES_GETTITLES			0x0F
#define IOCTL_ES_GETVIEWCNT			0x12
#define IOCTL_ES_GETVIEWS			0x13
#define IOCTL_ES_DIVERIFY			0x1C
#define IOCTL_ES_GETTITLEDIR		0x1D
#define IOCTL_ES_GETTITLEID			0x20

#define ES_HEAP_SIZE 0x800

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
	if(titleID == NULL) return ES_EINVAL;
	
	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLEID,":q",&title);
	if(ret<0) return ret;

	*titleID = title;
	return ret;
}

s32 ES_GetDataDir(u64 titleID,char *filepath)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(filepath==NULL || ((s32)filepath%32)!=0) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLEDIR,"q:d",titleID,filepath,30);

	return ret;
}

s32 ES_LaunchTitle(u64 titleID, tikview *view)
{

	static u64 title ATTRIBUTE_ALIGN(32);
	static ioctlv vectors[2] ATTRIBUTE_ALIGN(32);
	s32 res;
	
	if(__es_fd<0) return ES_ENOTINIT;
	if(view==NULL || ((u32)view%32)!=0) return ES_EINVAL;

#ifdef DEBUG_ES
	printf("ES LaunchTitle %d %016llx 0x%08x 0x%02x\n",__es_fd,titleID,(u32)view,sizeof(tikview));
#endif	

	
	title = titleID;
	vectors[0].data = (void*)&title;
	vectors[0].len = sizeof(u64);
	vectors[1].data = view;
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

#ifdef DEBUG_ES
	printf("ES GNTV %d %016llx\n",__es_fd,titleID);
#endif	
	
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt == NULL) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETVIEWCNT,"q:i",titleID,&cntviews);
#ifdef DEBUG_ES
	printf(" =%d (%d)\n",ret,cntviews);
#endif	
	
	
	if(ret<0) return ret;
	*cnt = cntviews;
	return ret;
}

s32 ES_GetTicketViews(u64 titleID, tikview *views, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	
	if(views==NULL || ((u32)views%32)!=0) return ES_EINVAL;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETVIEWS,"qi:d",titleID,cnt,views,sizeof(tikview)*cnt);
}

s32 ES_GetNumTitles(u32 *cnt)
{
	s32 ret;
	u32 cnttitles;

	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt == NULL) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLECOUNT,":i",&cnttitles);
	
	if(ret<0) return ret;
	*cnt = cnttitles;
	return ret;
}

s32 ES_GetTitles(u64 *titles, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	
	if(titles==NULL || ((u32)titles%32)!=0) return ES_EINVAL;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLES,"i:d",cnt,titles,sizeof(u64)*cnt);
}

signed_blob *ES_NextCert(signed_blob *certs)
{
	cert_header *cert;
	if(!SIGNATURE_SIZE(certs)) return NULL;
	cert = SIGNATURE_PAYLOAD(certs);
	if(!CERTIFICATE_SIZE(cert)) return NULL;
	return (signed_blob*)(((u8*)cert) + CERTIFICATE_SIZE(cert));
}

s32 __ES_sanity_check_certlist(signed_blob *certs, u32 certsize)
{
	int count = 0;
	signed_blob *end;
	
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

s32 ES_Identify(signed_blob *certificates, u32 certificates_size, signed_blob *stmd, u32 tmd_size, signed_blob *sticket, u32 ticket_size, u32 *keyid)
{

	tmd *p_tmd;
	u8 *hashes;
	s32 ret;
	u32 *keyid_buf;
	
	if(__es_fd<0) return ES_ENOTINIT;
	
	if(!IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!IS_VALID_SIGNATURE(sticket)) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	
	p_tmd = SIGNATURE_PAYLOAD(stmd);
	
	if(!(keyid_buf = iosAlloc(__es_hid, 4))) return ES_ENOMEM;
	if(!(hashes = iosAlloc(__es_hid, p_tmd->num_contents*20))) return ES_ENOMEM;
	
	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DIVERIFY, "dddd:id", certificates, certificates_size, 0, 0, sticket, ticket_size, stmd, tmd_size, keyid_buf, hashes, p_tmd->num_contents*20);
	if(ret >= 0 && keyid) *keyid = *keyid_buf;

	iosFree(__es_hid, keyid_buf);
	iosFree(__es_hid, hashes);
	return ret;
}


#endif /* defined(HW_RVL) */
