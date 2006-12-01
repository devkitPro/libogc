/****************************************************************************
 * TinySMB 0.3
 *
 * NBT Discovery Extension
 * Uses NBNS Name Query
 * See http://ubiqx.org for more details.
 ****************************************************************************/
#include <gccore.h>
#include <network.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nbt.h"

/**
 * NBT Packet Header
 */
u8 NBTHeader[] = {
  0xde, 0xad,  /* Transaction ID */
  0x01, 0x10,  /* Binary 0 0000 0010001 0000 */
  0x00, 0x01,  /* One name query.   */
  0x00, 0x00,  /* Zero answers.     */
  0x00, 0x00,  /* Zero authorities. */
  0x00, 0x00   /* Zero additional.  */
  };

u8 NBTTail[] = {
  0x00, 0x20,  /*** 0x20 == Machines ***/
  0x00, 0x01
  };	
  
NICINFO LocalNIC;
  
#if 0
/**
 * SetNonBlock
 */
void setnonblock( u32 sock )
{
	int flags;
	
	flags = fcntl(sock, F_GETFL);
	if ( flags < 0 )
		return;		/*** Should really PANIC! here ***/
	
	flags = ( flags | O_NONBLOCK );
	
	if ( fcntl( sock, F_SETFL, flags ) < 0 )
		return;		/*** More panicking here please ***/
	
	return;
	
}
#endif

/**
 * GetNETInfo
 *
 * Determine some basic information about this network.
 */
static void GetNETInfo( char *IP, char *mask )
{
	
	u32 hostip, subnetmask;
	
	memset(&LocalNIC, 0, sizeof(NICINFO));
	
	hostip = inet_addr(IP);
	subnetmask = inet_addr(mask);
	
	LocalNIC.net_address = ( hostip & subnetmask );
	LocalNIC.net_first = LocalNIC.net_address + 1;
	LocalNIC.net_broadcast = ( hostip & subnetmask ) | ( ~subnetmask );
	LocalNIC.net_last = LocalNIC.net_broadcast - 1;
#if 0
	printf("Network Address : %08x\n", LocalNIC.net_address);
	printf("Network Start   : %08x\n", LocalNIC.net_first);
	printf("Network End     : %08x\n", LocalNIC.net_last);
	printf("Network Brcast  : %08x\n", LocalNIC.net_broadcast);	
#endif
	
}

/**
 * SplitURI
 *
 * Input SMB URI '\\server\share'
 */
void SplitURI( char *URI, char *server, char *share )
{
	char *p, *q;
	char temp[1024];
	
	/*** Use a copy, as strtok destroys the source! ***/
	strcpy(temp, URI);
	
	if ( memcmp(temp, "\\\\", 2) == 0 )
	{
		p = temp + 2;
		q = strtok(p, "\\");
		if ( q != NULL )
		{
			strcpy(server, q);
			q = strtok(NULL,"\\");
			strcpy(share, q);
		}
		else
		{
			/*** Server only ***/
			strcpy(server, p);
			share[0] = 0;
		}
	}
	else
	{
		/*** Malformed URI, return NULL strings ***/
		server[0] = 0;
		share[0] = 0;
	}
}

/**
 * NBT Level1 Encoder
 *
 * Microsoft's Half-ASCII Encoding
 */
static u8 *L1_Encode( u8       *dst,
                  const u8 *name,
                  const u8  pad,
                  const u8  sfx )
  {
  int i = 0;
  int j = 0;
  int k = 0;

  while( ('\0' != name[i]) && (i < 15) )
    {
    k = toupper( name[i++] );
    dst[j++] = 'A' + ((k & 0xF0) >> 4);
    dst[j++] = 'A' +  (k & 0x0F);
    }

  i = 'A' + ((pad & 0xF0) >> 4);
  k = 'A' +  (pad & 0x0F);
  while( j < 30 )
    {
    dst[j++] = i;
    dst[j++] = k;
    }

  dst[30] = 'A' + ((sfx & 0xF0) >> 4);
  dst[31] = 'A' +  (sfx & 0x0F);
  dst[32] = '\0';

  return( dst );
  } /* L1_Encode */

/**
 * NBT Level2 Encoder
 */
static int L2_Encode( u8       *dst,
               const u8 *name,
               const u8  pad,
               const u8  sfx,
               const u8 *scope )
  {
  int lenpos;
  int i;
  int j;

  if ( NULL == L1_Encode( &dst[1], name, pad, sfx ) )
    return( -1 );
  dst[0] = 0x20;
  lenpos = 33;

  if( '\0' != *scope )
    {
    do
      {
      for( i = 0, j = (lenpos + 1);
           ('.' != scope[i]) && ('\0' != scope[i]);
           i++, j++)
        dst[j] = toupper( scope[i] );

      dst[lenpos] = (u8)i;
      lenpos     += i + 1;
      scope      += i;
      } while( '.' == *(scope++) );
    dst[lenpos] = '\0';
    }

  return( lenpos + 1 );
  } /* L2_Encode */

/**
 * GetServerIP
 *
 * Attempt to discover the IP of the supplied server
 */
int GetServerIP( u8 *msg, int msglen, char *IP )
{
	u32 sock;	/**< Socket **/
	struct sockaddr_in sox;
	u32 ret;
	u32 on = 0x1000000;
	u16 response[1024]; /**< Would need a machine with max 256 NICS! **/
	u8 *pr = ( u8 * )&response;
	char serverip[4];
	int i, n, j;
	u32 check;

#if 0
	strcpy(IP,"192.168.1.100");
	return 1;
#endif
	
	/*** Create new socket ***/
	sock = net_socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( sock < 0 )
		return 0;

	/*** Set as broadcast ***/
	ret = net_setsockopt( sock, SOL_SOCKET, SO_BROADCAST, (char *)&on, 1);
	if ( ret < 0 )
	{
		net_close(sock);
		return 0;
	}

	memset(&sox, 0, sizeof(struct sockaddr_in));
	sox.sin_addr.s_addr = inet_addr("255.255.255.255");
	sox.sin_family = AF_INET;
	sox.sin_port = htons( NBT_PORT );

	/*** Broadcast this packet ***/
	for(i = 0; i < 3; i++)
	{
	ret = net_sendto( sock, msg, msglen, 0, ( struct sockaddr *)&sox,
			sizeof( struct sockaddr_in ) );
	}

	if ( ret != msglen )
	{
		net_close(sock);
		return 0;
	}
	
	/*** Wait for response ***/
	ret = net_recv( sock, pr, 1024, 0 );
	net_close( sock );

	if ( ret <= 0 )
		return 0;

	/*** Hopefully, we have a shiny NBNS response ***/
	if ( response[0] != 0xdead ) /*** Fixed transaction ID ***/
		return 0;

	if ( response[3] == 0 )	    /*** No answers - Impossible! ***/
		return 0;

	if ( response[27] < 6 )	     /*** Each IP Entry is 6 bytes ***/
		return 0;

	/*** Find matching IP Address ***/
	n = response[27] / 6;
	j = 29;	/**< Short offset **/
	serverip[0] = 0;

	for ( i = 0; i < n; i++ )
	{
		memcpy(&check, (char *)&response[j], 4);
		
		/*** Is this IP within range ***/
		if ( ( check >= LocalNIC.net_first ) && ( check <= LocalNIC.net_last ) )
		{
			memcpy(serverip, &check, 4);
			break;
		}
		
		j += 3;
	}
	
	if ( strlen(serverip) == 0 )
		return 0;

	/*** Set server IP ***/
	sprintf(IP, "%d.%d.%d.%d", serverip[0], serverip[1], serverip[2], serverip[3]);
	
	return 1;
}

/**
 * DiscoverURI
 */
int DiscoverURI( char *thisIP, char *thisSubnet, char *URI, char *IP )
{
	u8 buffer[512];
	int len;
	int total_len = 0;
	u8 server[1024];	/**< Should only need 32 chars, but users are users **/
	u8 scope[32];		/**< Unused. Although could be .org, .co.uk etc for ADS, Unix etc **/
	u8 share[1024];	/**< Unused. Here for completeness **/
	
	/*** Decode the URI ***/
	SplitURI( URI, server, share );
	
	if ( strlen(server) == 0 )
		return 0;
	
	/*** Start to create the buffer **/
	memcpy(buffer, NBTHeader, sizeof(NBTHeader) );
	memset(scope, 0, 32);
	
	/*** Encode ***/
	total_len = sizeof(NBTHeader);
	
	len = L2_Encode(buffer + total_len, server, ' ', '\0', scope);
	
	if ( len < 0 )
		return 0;
	
	total_len += len;
	
	memcpy(buffer + total_len, NBTTail, sizeof(NBTTail));
	total_len += sizeof(NBTTail);
	
	GetNETInfo( thisIP, thisSubnet );
	
	return GetServerIP( buffer, total_len, IP );
	
}

