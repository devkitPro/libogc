#ifndef __IFCONFIG_H__
#define __IFCONFIG_H__

#include <netinet/in.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


s32 if_config( char *local_ip, char *netmask, char *gateway,bool use_dhcp, int max_retries);
s32 if_configex(struct in_addr *local_ip, struct in_addr *netmask, struct in_addr *gateway, bool use_dhcp, int max_retries);

#ifdef __cplusplus
	}
#endif

#endif