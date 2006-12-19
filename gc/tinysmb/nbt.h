/****************************************************************************
 * TinySMB 0.3
 *
 * NBT Discovery Extension
 ****************************************************************************/

#ifndef _NBTDISCOVERY_
#define _NBTDISCOVERY_

#define NBT_BROADCAST "255.255.255.255"
#define NBT_PORT 137

/**
 * Network Information
 *
 * Used by Discovery when multiple NICs detected
 */
typedef struct _NICINFO {
	u32	net_address;
	u32  net_first;
	u32  net_last;
	u32  net_broadcast;
} NICINFO;

int DiscoverURI( char *thisIP, char *thisSubnet, char *URI, char *IP );
void SplitURI( char *URI, char *server, char *share );

#endif

