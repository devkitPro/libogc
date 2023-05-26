/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIP_IP_ADDR_H__
#define __LWIP_IP_ADDR_H__

#include "lwip/arch.h"

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct ip_addr {
  PACK_STRUCT_FIELD(u32_t addr);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct ip_addr2 {
  PACK_STRUCT_FIELD(u16_t addrw[2]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

struct netif;

extern const struct ip_addr ip_addr_any;
extern const struct ip_addr ip_addr_broadcast;

/** IP_ADDR_ can be used as a fixed IP address
 *  for the wildcard and the broadcast address
 */
#define IP_ADDR_ANY ((struct ip_addr *)&ip_addr_any)
#define IP_ADDR_BROADCAST ((struct ip_addr *)&ip_addr_broadcast)

#define IN_LOOPBACKNET      127         /* official! */


#define IP4_ADDR(ipaddr, a,b,c,d) (ipaddr)->addr = htonl(((u32_t)(a & 0xff) << 24) | ((u32_t)(b & 0xff) << 16) | \
                                                         ((u32_t)(c & 0xff) << 8) | (u32_t)(d & 0xff))

#define ip_addr_set(dest, src) (dest)->addr = \
                               ((src) == NULL? 0:\
                               (src)->addr)
/**
 * Determine if two address are on the same network.
 *
 * @arg addr1 IP address 1
 * @arg addr2 IP address 2
 * @arg mask network identifier mask
 * @return !0 if the network identifiers of both address match
 */
#define ip_addr_netcmp(addr1, addr2, mask) (((addr1)->addr & \
                                              (mask)->addr) == \
                                             ((addr2)->addr & \
                                              (mask)->addr))
#define ip_addr_cmp(addr1, addr2) ((addr1)->addr == (addr2)->addr)

#define ip_addr_isany(addr1) ((addr1) == NULL || (addr1)->addr == 0)

u8_t ip_addr_isbroadcast(struct ip_addr *, struct netif *);

#define ip_addr_ismulticast(addr1) (((addr1)->addr & ntohl(0xf0000000)) == ntohl(0xe0000000))


#define ip_addr_debug_print(debug, ipaddr) LWIP_DEBUGF(debug, ("%"U16_F".%"U16_F".%"U16_F".%"U16_F, \
        ipaddr?(u16_t)(ntohl((ipaddr)->addr) >> 24) & 0xff:0, \
        ipaddr?(u16_t)(ntohl((ipaddr)->addr) >> 16) & 0xff:0, \
        ipaddr?(u16_t)(ntohl((ipaddr)->addr) >> 8) & 0xff:0, \
        ipaddr?(u16_t)ntohl((ipaddr)->addr) & 0xff:0U))

/* cast to unsigned int, as it is used as argument to printf functions
 * which expect integer arguments. CSi: use cc.h formatters (conversion chars)! */
#define ip4_addr1(ipaddr) ((u16_t)(ntohl((ipaddr)->addr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((u16_t)(ntohl((ipaddr)->addr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((u16_t)(ntohl((ipaddr)->addr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((u16_t)(ntohl((ipaddr)->addr)) & 0xff)
#endif /* __LWIP_IP_ADDR_H__ */






