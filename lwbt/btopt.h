/**
 * \defgroup uipopt Configuration options for uIP
 * @{
 *
 * uIP is configured using the per-project configuration file
 * "uipopt.h". This file contains all compile-time options for uIP and
 * should be tweaked to match each specific project. The uIP
 * distribution contains a documented example "uipopt.h" that can be
 * copied and modified for each project.
 */

/**
 * \file
 * Configuration options for uIP.
 * \author Adam Dunkels <adam@dunkels.com>
 *
 * This file is used for tweaking various configuration options for
 * uIP. You should make a copy of this file into one of your project's
 * directories instead of editing this example "uipopt.h" file that
 * comes with the uIP distribution.
 */

/*
 * Copyright (c) 2001-2003, Adam Dunkels.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.  
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 *
 * This file is part of the uIP TCP/IP stack.
 *
 *
 */

#ifndef __BTOPT_H__
#define __BTOPT_H__

#include <gctypes.h>
#include <stdlib.h>
#include <string.h>

/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipopttypedef uIP type definitions
 * @{
 */

/**
 * The 8-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * char" works for most compilers.
 */
typedef u8 u8_t;

/**
 * The 8-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * char" works for most compilers.
 */
typedef s8 s8_t;

/**
 * The 16-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef u16 u16_t;

/**
 * The 16-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef s16 s16_t;

/**
 * The 32-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef s32 s32_t;

/**
 * The 32-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef u32 u32_t;

/**
 * The statistics data type.
 *
 * This datatype determines how high the statistics counters are able
 * to count.
 */
typedef s8 err_t;

/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptip IP configuration options
 * @{
 *
 */
/**
 * The IP TTL (time to live) of IP packets sent by uIP.
 *
 * This should normally not be changed.
 */
#define TCP_TTL         255

#define TCP				1

/**
 * Turn on support for IP packet reassembly.
 *
 * uIP supports reassembly of fragmented IP packets. This features
 * requires an additonal amount of RAM to hold the reassembly buffer
 * and the reassembly code size is approximately 700 bytes.  The
 * reassembly buffer is of the same size as the uip_buf buffer
 * (configured by UIP_BUFSIZE).
 *
 * \note IP packet reassembly is not heavily tested.
 *
 * \hideinitializer
 */
#define IP_REASSEMBLY 0

#define IP_FRAG 0

/**
 * The maximum time an IP fragment should wait in the reassembly
 * buffer before it is dropped.
 *
 */
#define REASS_MAXAGE 30

/** @} */

/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptudp UDP configuration options
 * @{
 *
 * \note The UDP support in uIP is still not entirely complete; there
 * is no support for sending or receiving broadcast or multicast
 * packets, but it works well enough to support a number of vital
 * applications such as DNS queries, though
 */

/**
 * Toggles wether UDP support should be compiled in or not.
 *
 * \hideinitializer
 */
#define UDP           0

/**
 * Toggles if UDP checksums should be used or not.
 *
 * \note Support for UDP checksums is currently not included in uIP,
 * so this option has no function.
 *
 * \hideinitializer
 */
#define CHECKSUMS 0

/**
 * The maximum amount of concurrent UDP connections.
 *
 * \hideinitializer
 */
#define CONNS    10

/**
 * The name of the function that should be called when UDP datagrams arrive.
 *
 * \hideinitializer
 */
//#define UIP_UDP_APPCALL		((void*0)

/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipopttcp TCP configuration options
 * @{
 */

/**
 * Determines if support for opening connections from uIP should be
 * compiled in.
 *
 * If the applications that are running on top of uIP for this project
 * do not need to open outgoing TCP connections, this configration
 * option can be turned off to reduce the code size of uIP.
 *
 * \hideinitializer
 */
#define ACTIVE_OPEN				1

/**
 * The maximum number of simultaneously open TCP connections.
 *
 * Since the TCP connections are statically allocated, turning this
 * configuration knob down results in less RAM used. Each TCP
 * connection requires approximatly 30 bytes of memory.
 *
 * \hideinitializer
 */
#define HCI_PCBS				8

/**
 * The maximum number of simultaneously listening TCP ports.
 *
 * Each listening TCP port requires 2 bytes of memory.
 *
 * \hideinitializer
 */
#define HCI_LINKS				4

/**
 * The size of the advertised receiver's window.
 *
 * Should be set low (i.e., to the size of the uip_buf buffer) is the
 * application is slow to process incoming data, or high (32768 bytes)
 * if the application processes data quickly.
 *
 * \hideinitializer
 */
#define HCI_INQ_RES				32

#define TCP_SEGS				32

	
/**
 * Determines if support for TCP urgent data notification should be
 * compiled in.
 *
 * Urgent data (out-of-band data) is a rarely used TCP feature that
 * very seldom would be required.
 *
 * \hideinitializer
 */
#define URGDATA				1

/**
 * The initial retransmission timeout counted in timer pulses.
 *
 * This should not be changed.
 */
#define RTO					3

/**
 * The maximum number of times a segment should be retransmitted
 * before the connection should be aborted.
 *
 * This should not be changed.
 */
#define MAXRTX				12

/**
 * The maximum number of times a SYN segment should be retransmitted
 * before a connection request should be deemed to have been
 * unsuccessful.
 *
 * This should not need to be changed.
 */
#define MAXSYNRTX			4

/**
 * The TCP maximum segment size.
 *
 * This is should not be to set to more than UIP_BUFSIZE - UIP_LLH_LEN - 40.
 */
#define TCP_MSS				(1460)


#define TCP_SND_BUF			(4*TCP_MSS)

#define TCP_SND_QUEUELEN	(4*TCP_SND_BUF/TCP_MSS)

#define TCP_WND				(4*TCP_MSS)

/**
 * How long a connection should stay in the TIME_WAIT state.
 *
 * This configiration option has no real implication, and it should be
 * left untouched.
 */ 
#define TIME_WAIT_TIMEOUT	120


/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptarp ARP configuration options
 * @{
 */

/**
 * The size of the ARP table.
 *
 * This option should be set to a larger value if this uIP node will
 * have many connections from the local network.
 *
 * \hideinitializer
 */
#define ARPTAB_SIZE 8

/**
 * The maxium age of ARP table entries measured in 10ths of seconds.
 *
 * An UIP_ARP_MAXAGE of 120 corresponds to 20 minutes (BSD
 * default).
 */
#define ARP_MAXAGE 120

/** @} */

/*------------------------------------------------------------------------------*/

/**
 * \defgroup uipoptgeneral General configuration options
 * @{
 */

/**
 * The size of the uIP packet buffer.
 *
 * The uIP packet buffer should not be smaller than 60 bytes, and does
 * not need to be larger than 1500 bytes. Lower size results in lower
 * TCP throughput, larger size results in higher TCP throughput.
 *
 * \hideinitializer
 */
#define MEM_SIZE				(32*1024)

#define PBUF_POOL_NUM			16
#define PBUF_POOL_BUFSIZE		1800

#define PBUF_ROM_NUM			128


/**
 * Determines if statistics support should be compiled in.
 *
 * The statistics is useful for debugging and to show the user.
 *
 * \hideinitializer
 */
#define STATISTICS  0

/**
 * Determines if logging of certain events should be compiled in.
 *
 * This is useful mostly for debugging. The function uip_log()
 * must be implemented to suit the architecture of the project, if
 * logging is turned on.
 *
 * \hideinitializer
 */
#define LOGGING     0
#define ERRORING	0

/**
 * Print out a uIP log message.
 *
 * This function must be implemented by the module that uses uIP, and
 * is called by uIP whenever a log message is generated.
 */
void bt_log(const char *filename,int line_nb,char *msg);

/**
 * The link level header length.
 *
 * This is the offset into the uip_buf where the IP header can be
 * found. For Ethernet, this should be set to 14. For SLIP, this
 * should be set to 0.
 *
 * \hideinitializer
 */
#define LL_HLEN     16

#define TCPIP_HLEN	40
/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptcpu CPU architecture configuration
 * @{
 *
 * The CPU architecture configuration is where the endianess of the
 * CPU on which uIP is to be run is specified. Most CPUs today are
 * little endian, and the most notable exception are the Motorolas
 * which are big endian. The BYTE_ORDER macro should be changed to
 * reflect the CPU architecture on which uIP is to be run.
 */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN  3412
#endif /* LITTLE_ENDIAN */
#ifndef BIG_ENDIAN
#define BIG_ENDIAN     1234
#endif /* BIGE_ENDIAN */

/**
 * The byte order of the CPU architecture on which uIP is to be run.
 *
 * This option can be either BIG_ENDIAN (Motorola byte order) or
 * LITTLE_ENDIAN (Intel byte order).
 *
 * \hideinitializer
 */
#ifndef BYTE_ORDER
#define BYTE_ORDER     BIG_ENDIAN
#endif /* BYTE_ORDER */

/** @} */
/*------------------------------------------------------------------------------*/

/**
 * \defgroup uipoptapp Appication specific configurations
 * @{
 *
 * An uIP application is implemented using a single application
 * function that is called by uIP whenever a TCP/IP event occurs. The
 * name of this function must be registered with uIP at compile time
 * using the UIP_APPCALL definition.
 *
 * uIP applications can store the application state within the
 * uip_conn structure by specifying the size of the application
 * structure with the UIP_APPSTATE_SIZE macro.
 *
 * The file containing the definitions must be included in the
 * uipopt.h file.
 *
 * The following example illustrates how this can look.
 \code

void httpd_appcall(void);
#define UIP_APPCALL     httpd_appcall

struct httpd_state {
  u8_t state; 
  u16_t count;
  char *dataptr;
  char *script;
};
#define UIP_APPSTATE_SIZE (sizeof(struct httpd_state))
 \endcode
 */

/**
 * \var #define UIP_APPCALL
 *
 * The name of the application function that uIP should call in
 * response to TCP/IP events.
 *
 */

/**
 * \var #define UIP_APPSTATE_SIZE
 *
 * The size of the application state that is to be stored in the
 * uip_conn structure.
 */
/** @} */

/* Include the header file for the application program that should be
   used. If you don't use the example web server, you should change
   this. */

#define LIBC_MEMFUNCREPLACE				0

/* ---------- Memory options ---------- */
#define MAX_NUM_CLIENTS					6 /* Maximum number of connected Bluetooth clients. No more than 6 */ 

#define MEMB_NUM_HCI_PCB				1 /* Always set to one */
#define MEMB_NUM_HCI_LINK				(1 + MAX_NUM_CLIENTS) /* One for DT + One per ACL connection */
#define MEMB_NUM_HCI_INQ				1 /* One per max number of returned results from an inquiry */

/* MEMP_NUM_L2CAP_PCB: the number of simulatenously active L2CAP
   connections. */
#define MEMB_NUM_L2CAP_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* One for a closing connection + one for DT + one per number of connected Bluetooth clients */
/* MEMP_NUM_L2CAP_PCB_LISTEN: the number of listening L2CAP
   connections. */
#define MEMB_NUM_L2CAP_PCB_LISTEN		2 /* One per listening PSM */
/* MEMP_NUM_L2CAP_SIG: the number of simultaneously unresponded
   L2CAP signals */
#define MEMB_NUM_L2CAP_SIG				(2 * MAX_NUM_CLIENTS)/* Two per number of connected Bluetooth clients but min 2 */
#define MEMB_NUM_L2CAP_SEG				(2 + 2 * MAX_NUM_CLIENTS) /* One per number of L2CAP connections */

#define MEMB_NUM_SDP_PCB				MAX_NUM_CLIENTS /* One per number of connected Bluetooth clients */
#define MEMB_NUM_SDP_RECORD				1 /* One per registered service record */

#define MEMP_NUM_RFCOMM_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* Two for DT + Two per number of connected Bluetooth clients */
#define MEMP_NUM_RFCOMM_PCB_LISTEN		(2 * MAX_NUM_CLIENTS) /* Two per number of connected Bluetooth clients */

#define MEMP_NUM_HIDP_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* Two for DT + Two per number of connected Bluetooth clients */
#define MEMP_NUM_HIDP_PCB_LISTEN		(2 * MAX_NUM_CLIENTS) /* Two per number of connected Bluetooth clients */

#define MEMP_NUM_PPP_PCB				(1 + MAX_NUM_CLIENTS) /* One for DT + One per number of connected Bluetooth clients */
#define MEMP_NUM_PPP_REQ				MAX_NUM_CLIENTS /* One per number of connected Bluetooth clients but min 1 */

#define MEMP_NUM_BTE_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* Two for DT + Two per number of connected Bluetooth clients */
#define MEMP_NUM_BTE_PCB_LISTEN			(2 * MAX_NUM_CLIENTS) /* Two per number of connected Bluetooth clients */

/* ---------- HCI options ---------- */
/* HCI: Defines if we have lower layers of the Bluetooth stack running on a separate host
   controller */
#define HCI 1

#if HCI
/* HCI_HOST_MAX_NUM_ACL: The maximum number of ACL packets that the host can buffer */
#define HCI_HOST_MAX_NUM_ACL			8 //TODO: Should be equal to PBUF_POOL_SIZE/2??? */
/* HCI_HOST_ACL_MAX_LEN: The maximum size of an ACL packet that the host can buffer */
#define HCI_HOST_ACL_MAX_LEN			(HIDD_N + 14) /* Default: RFCOMM MFS + ACL header size, L2CAP header size, 
                                                RFCOMM header size and RFCOMM FCS size */
/* HCI_PACKET_TYPE: The set of packet types which may be used on the connection. In order to 
   maximize packet throughput, it is recommended that RFCOMM should make use of the 3 and 5 
   slot baseband packets.*/
#define HCI_PACKET_TYPE					0xCC18 /* Default DM1, DH1, DM3, DH3, DM5, DH5 */
/* HCI_ALLOW_ROLE_SWITCH: Tells the host controller whether to accept a Master/Slave switch 
   during establishment of a connection */
#define HCI_ALLOW_ROLE_SWITCH			1 /* Default 1 */
/* HCI_FLOW_QUEUEING: Control if a packet should be queued if the host controller is out of 
   bufferspace for outgoing packets. Only the first packet sent when out of credits will be 
   queued */
#define HCI_FLOW_QUEUEING 0 /* Default: 0 */

#endif /* HCI */

/* ---------- L2CAP options ---------- */
/* L2CAP_HCI: Option for including HCI to access the Bluetooth baseband capabilities */
#define L2CAP_HCI						1 //TODO: NEEDED?
/* L2CAP_CFG_QOS: Control if a flow specification similar to RFC 1363 should be used */
#define L2CAP_CFG_QOS					0
/* L2CAP_MTU: Maximum transmission unit for L2CAP packet payload (min 48) */
#define L2CAP_MTU						(HIDD_N + 1)/* Default for this implementation is RFCOMM MFS + RFCOMM header size and 
														 RFCOMM FCS size while the L2CAP default is 672 */
/* L2CAP_OUT_FLUSHTO: For some networking protocols, such as many real-time protocols, guaranteed delivery
   is undesirable. The flush time-out value SHALL be set to its default value 0xffff for a reliable L2CAP 
   channel, and MAY be set to other values if guaranteed delivery is not desired. (min 1) */
#define L2CAP_OUT_FLUSHTO				0xFFFF /* Default: 0xFFFF. Infinite number of retransmissions (reliable channel)
												  The value of 1 implies no retransmissions at the Baseband level 
												  should be performed since the minimum polling interval is 1.25 ms.*/ 
/* L2CAP_RTX: The Responsive Timeout eXpired timer is used to terminate
   the channel when the remote endpoint is unresponsive to signalling
   requests (min 1s, max 60s) */
#define L2CAP_RTX						60
/* L2CAP_ERTX: The Extended Response Timeout eXpired timer is used in
   place of the RTC timer when a L2CAP_ConnectRspPnd event is received
   (min 60s, max 300s) */
#define L2CAP_ERTX						300
/* L2CAP_MAXRTX: Maximum number of Request retransmissions before
   terminating the channel identified by the request. The decision
   should be based on the flush timeout of the signalling link. If the
   flush timeout is infinite, no retransmissions should be performed */
#define L2CAP_MAXRTX					0
/* L2CAP_CFG_TO: Amount of time spent arbitrating the channel parameters
   before terminating the connection (max 120s) */  
#define L2CAP_CFG_TO					30

/* ---------- BTE options ---------- */

/* ---------- HIDD options ---------- */
/* RFCOMM_N: Maximum frame size for RFCOMM segments (min 23, max 32767)*/
#define HIDD_N							672			 /* Default: Worst case byte stuffed PPP packet size + 
																 non-compressed PPP header size and FCS size */
/* RFCOMM_K: Initial amount of credits issued to the peer (min 0, max 7) */
#define RFCOMM_K						0
/* RFCOMM_TO: Acknowledgement timer (T1) and response timer for multiplexer control channel (T2).
   T1 is the timeout for frames sent with the P/F bit set to 1 (SABM and DISC) and T2 is the timeout
   for commands sent in UIH frames on DLCI 0 (min 10s, max 60s) */
#define RFCOMM_TO						20
/* RFCOMM_FLOW_QUEUEING: Control if a packet should be queued if a channel is out of credits for 
   outgoing packets. Only the first packet sent when out of credits will be queued */
#define RFCOMM_FLOW_QUEUEING			0 /* Default: 0 */


#endif /* __BTOPT_H__ */
