#ifndef __TCPIP_H__
#define __TCPIP_H__

#include "uIP/uip.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "netinet/in.h"

struct dbginterface* tcpip_init(struct uip_ip_addr *localip,struct uip_ip_addr *netmask,struct uip_ip_addr *gateway,u16 port);

void tcpip_close(s32_t s);
void tcpip_starttimer(s32_t s);
void tcpip_stoptimer(s32_t s);
s32_t tcpip_socket(void);
s32_t tcpip_listen(s32_t s,u32_t backlog);
s32_t tcpip_bind(s32_t s,struct sockaddr *name,socklen_t *namelen);
s32_t tcpip_accept(s32_t s);
s32_t tcpip_read(s32_t s,void *buffer,u32_t len);
s32_t tcpip_write(s32_t s,const void *buffer,u32_t len);

#endif
