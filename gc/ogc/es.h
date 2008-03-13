/*-------------------------------------------------------------

es.h -- tik services

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

#ifndef __ES_H__
#define __ES_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <gcutil.h>

#define ES_EINVAL -0x1004
#define ES_ENOMEM -0x100C
#define ES_ENOTINIT -0x1100

typedef u32 sigtype;
typedef sigtype sig_header;
typedef sig_header signed_blob;

typedef struct _sig_rsa2048 {
	sigtype type;
	u8 sig[256];
	u8 fill[60];
} __attribute__((packed)) sig_rsa2048;

typedef struct _sig_rsa4096 {
	sigtype type;
	u8 sig[512];
	u8 fill[60];
}  __attribute__((packed)) sig_rsa4096;

typedef char sig_issuer[0x40];

typedef struct _tiklimit {
	u32 tag;
	u32 value;
} __attribute__((packed)) tiklimit;

typedef struct _tikview {
	u32 view;
	u64 ticketid;
	u32 devicetype;
	u64 titleid;
	u16 access_mask;
	u8 reserved[0x3c];
	u8 cidx_mask[0x40];
	u16 padding;
	tiklimit limits[8];
} __attribute__((packed)) tikview;

typedef struct _tik {
	sig_issuer issuer;
	u8 fill[63]; //TODO: not really fill
	u8 cipher_title_key[16];
	u8 fill2;
	u64 ticketid;
	u32 devicetype;
	u64 titleid;
	u16 access_mask;
	u8 reserved[0x3c];
	u8 cidx_mask[0x40];
	u16 padding;
	tiklimit limits[8];
} __attribute__((packed)) tik;

#define ES_SIG_RSA4096 0x10000
#define ES_SIG_RSA2048 0x10001
#define ES_SIG_ECC 0x10002

#define ES_CERT_RSA4096 0
#define ES_CERT_RSA2048 1
#define ES_CERT_ECC 2

typedef struct _tmd_content {
	u32 cid;
	u16 index;
	u16 type;
	u64 size;
	u8 hash[20];
}  __attribute__((packed)) tmd_content;

typedef struct _tmd {
	sig_issuer issuer;
	u8 version;
	u8 ca_crl_version;
	u8 signer_crl_version;
	u8 fill2;
	u64 sys_version;
	u64 title_id;
	u32 title_type;
	u16 group_id; // publisher
	u8 reserved[62];
	u32 access_rights;
	u16 title_version;
	u16 num_contents;
	u16 boot_index;
	u16 fill3;
	// content records follow
} __attribute__((packed)) tmd;

typedef struct _cert_header {
	sig_issuer issuer;
	u32 cert_type;
	char cert_name[64];
	u32 cert_id; //???
} __attribute__((packed)) cert_header;

typedef struct _cert_rsa2048 {
	sig_issuer issuer;
	u32 cert_type;
	char cert_name[64];
	u32 cert_id;
	u8 modulus[256];
	u32 exponent;
	u8 pad[0x34];
} __attribute__((packed)) cert_rsa2048;

typedef struct _cert_rsa4096 {
	sig_issuer issuer;
	u32 cert_type;
	char cert_name[64];
	u32 cert_id;
	u8 modulus[512];
	u32 exponent;
	u8 pad[0x34];
} __attribute__((packed)) cert_rsa4096;

#define TMD_SIZE(x) (((x).num_contents)*sizeof(tmd_content) + sizeof(tmd))
#define TMD_CONTENTS(x) ((tmd_content*)(((tmd*)(x))+1))

//TODO: add ECC stuff

#define IS_VALID_SIGNATURE(x) (((*(x))==ES_SIG_RSA2048) || ((*(x))==ES_SIG_RSA4096))

#define SIGNATURE_SIZE(x) (\
	((*(x))==ES_SIG_RSA2048) ? sizeof(sig_rsa2048) : ( \
	((*(x))==ES_SIG_RSA4096) ? sizeof(sig_rsa4096) : 0 ))

#define IS_VALID_CERT(x) ((((x)->cert_type)==ES_CERT_RSA2048) || (((x)->cert_type)==ES_CERT_RSA4096))

#define CERTIFICATE_SIZE(x) (\
	(((x)->cert_type)==ES_CERT_RSA2048) ? sizeof(cert_rsa2048) : ( \
	(((x)->cert_type)==ES_CERT_RSA4096) ? sizeof(cert_rsa4096) : 0 ))

#define SIGNATURE_PAYLOAD(x) ((void *)(((u8*)(x)) + SIGNATURE_SIZE(x)))

#define SIGNED_TMD_SIZE(x) ( TMD_SIZE((tmd*)SIGNATURE_PAYLOAD(x)) + SIGNATURE_SIZE(x))
#define SIGNED_TIK_SIZE(x) ( sizeof(tik) + SIGNATURE_SIZE(x) )
#define SIGNED_CERT_SIZE(x) ( CERTIFICATE_SIZE((cert_header*)SIGNATURE_PAYLOAD(x)) + SIGNATURE_SIZE(x))

s32 __ES_Init(void);
s32 __ES_Close(void);
s32 __ES_Reset(void);
s32 ES_GetTitleID(u64 *titleID);
s32 ES_GetDataDir(u64 titleID,char *filepath);
s32 ES_GetNumTicketViews(u64 titleID, u32 *cnt);
s32 ES_GetTicketViews(u64 titleID, tikview *views, u32 cnt);
s32 ES_GetNumTitles(u32 *cnt);
s32 ES_GetTitles(u64 *titles, u32 cnt);
s32 ES_LaunchTitle(u64 titleID, tikview *view);
signed_blob *ES_NextCert(signed_blob *certs);
s32 ES_Identify(signed_blob *certificates, u32 certificates_size, signed_blob *tmd, u32 tmd_size, signed_blob *ticket, u32 ticket_size, u32 *keyid);

#endif

#endif
