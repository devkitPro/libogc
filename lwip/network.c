#include <time.h>
#include <errno.h>
#include <lwp.h>
#include <video.h>
#include <message.h>
#include <mutex.h>
#include <cond.h>
#include <semaphore.h>
#include <processor.h>
#include <lwp_threads.h>
#include <lwp_watchdog.h>
#include <lwip/debug.h>
#include <lwip/opt.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/sys.h>
#include <lwip/stats.h>
#include <lwip/ip.h>
#include <lwip/raw.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/dhcp.h>
#include <netif/etharp.h>
#include <netif/loopif.h>
#include <netif/gcif/gcif.h>

#include "libogcsys/iosupp.h"
#include "network.h"

//#define _NET_DEBUG

#define STACKSIZE		16384
#define MQBOX_SIZE		100
#define NUM_SOCKETS		64

#define NETCONN_NOCOPY	0x00
#define NETCONN_COPY	0x01

enum netmsq_type {
	NETMSG_API,
	NETMSG_INPUT,
	NETMSG_CALLBACK
};

enum apimsg_type {
	APIMSG_NEWCONN,
	APIMSG_DELCONN,
	APIMSG_BIND,
	APIMSG_CONNECT,
	APIMSG_DISCONNECT,
	APIMSG_LISTEN,
	APIMSG_ACCEPT,
	APIMSG_SEND,
	APIMSG_RECV,
	APIMSG_WRITE,
	APIMSG_CLOSE,
	APIMSG_MAX
};

enum netconn_type {
	NETCONN_TCP,
	NETCONN_UDP,
	NETCONN_UDPLITE,
	NETCONN_UDPNOCHKSUM,
	NETCONN_RAW
};

enum netconn_state {
	NETCONN_NONE,
	NETCONN_WRITE,
	NETCONN_ACCEPT,
	NETCONN_RECV,
	NETCONN_CONNECT,
	NETCONN_CLOSE
};

enum netconn_evt {
	NETCONN_EVTRCVPLUS,
	NETCONN_EVTRCVMINUS,
	NETCONN_EVTSENDPLUS,
	NETCONN_EVTSENDMINUS
};

struct netbuf {
	struct pbuf *p,*ptr;
	struct ip_addr *fromaddr;
	u16 fromport;
	err_t err;
};

struct netconn {
	enum netconn_type type;
	enum netconn_state state;
	union {
		struct tcp_pcb *tcp;
		struct udp_pcb *udp;
		struct raw_pcb *raw;
	} pcb;
	err_t err;
	sem_t sem;
	mq_box_t mbox;
	mq_box_t recvmbox;
	mq_box_t acceptmbox;
	u16 recvavail;
	u32 socket;
	void (*callback)(struct netconn *,enum netconn_evt,u32);
};

struct apimsg_msg {
	struct netconn *conn;
	enum netconn_type type;
	union {
		struct pbuf *p;
		struct {
			struct ip_addr *ipaddr;
			u16 port;
		} bc;
		struct {
			void *dataptr;
			u32 len;
			u8 copy;
		} w;
		mq_box_t mbox;
		u16 len;
	} msg;
};

struct api_msg {
	enum apimsg_type type;
	struct apimsg_msg msg;
};

struct net_msg {
	enum netmsq_type type;
	union {
		struct api_msg *apimsg;
		struct {
			struct pbuf *p;
			struct netif *net;
		} inp;
		struct {
			void (*f)(void *);
			void *ctx;
		} cb;
	} msg;
};

struct netsocket {
	struct netconn *conn;
	struct netbuf *lastdata;
	u32 lastoffset,rcvevt,sendevt,flags;
	u32 err;
};

struct netselect_cb {
	struct netselect_cb *next;
	fd_set *readset;
	fd_set *writeset;
	fd_set *exceptset;
	u32 sem_signaled;
	sem_t sem;
};

struct net_sem {
	u32 c;
	cond_t cond;
	mutex_t mutex;
};

typedef void (*apimsg_decode)(struct apimsg_msg *);

static u32 g_netinitiated = 0;
static u32 tcpiplayer_inited = 0;
static u32 net_tcp_ticks = 0;
static wd_cntrl tcp_time_cntrl;

static struct netif g_hNetIF;
static struct netif g_hLoopIF;
static struct netsocket sockets[NUM_SOCKETS];
static struct netselect_cb *selectcb_list = NULL;

static sem_t netsocket_sem;
static sem_t sockselect_sem;
static mq_box_t netthread_mbox = NULL;

static lwp_t hnet_thread;
static u32 netthread_stack[STACKSIZE/sizeof(u32)];

static u32 tcp_timer_active = 0;

static struct netbuf* netbuf_new();
static void netbuf_delete(struct netbuf *);
static void* netbuf_alloc(struct netbuf *,u32);
static void netbuf_free(struct netbuf *);
static void netbuf_copypartial(struct netbuf *,void *,u32,u32);
static void netbuf_ref(struct netbuf *,void *,u32);

static struct netconn* netconn_new_with_callback(enum netconn_type,void (*)(struct netconn *,enum netconn_evt,u32));
static struct netconn* netconn_new_with_proto_and_callback(enum netconn_type,u16,void (*)(struct netconn *,enum netconn_evt,u32));
static err_t netconn_delete(struct netconn *);
static struct netconn* netconn_accept(struct netconn* );
static err_t netconn_peer(struct netconn *,struct ip_addr *,u16 *);
static err_t netconn_bind(struct netconn *,struct ip_addr *,u16);
static err_t netconn_listen(struct netconn *);
static struct netbuf* netconn_recv(struct netconn *);
static err_t netconn_send(struct netconn *,struct netbuf *);
static err_t netconn_write(struct netconn *,void *,u32,u8);
static err_t netconn_connect(struct netconn *,struct ip_addr *,u16);
static err_t netconn_disconnect(struct netconn *);

static void do_newconn(struct apimsg_msg *);
static void do_delconn(struct apimsg_msg *);
static void do_bind(struct apimsg_msg *);
static void do_listen(struct apimsg_msg *);
static void do_connect(struct apimsg_msg *);
static void do_disconnect(struct apimsg_msg *);
static void do_accept(struct apimsg_msg *);
static void do_send(struct apimsg_msg *);
static void do_recv(struct apimsg_msg *);
static void do_write(struct apimsg_msg *);
static void do_close(struct apimsg_msg *);

static apimsg_decode decode[APIMSG_MAX] = {
	do_newconn,
	do_delconn,
	do_bind,
	do_connect,
	do_disconnect,
	do_listen,
	do_accept,
	do_send,
	do_recv,
	do_write,
	do_close
};

static void apimsg_post(struct api_msg *);

static err_t net_input(struct pbuf *,struct netif *);
static void net_apimsg(struct api_msg *);
static err_t net_callback(void (*)(void *),void *);
static void* net_thread(void *);

extern unsigned int timespec_to_interval(const struct timespec *);

/* low level stuff */
static void __tcp_timer(void *arg)
{
#ifdef _NET_DEBUG
	printf("__tcp_timer()\n");
#endif
	tcp_tmr();
	if (tcp_active_pcbs || tcp_tw_pcbs) {
		__lwp_wd_insert_ticks(&tcp_time_cntrl,net_tcp_ticks);
	} else
		tcp_timer_active = 0;
}

void tcp_timer_needed(void)
{
	if(!tcp_timer_active && (tcp_active_pcbs || tcp_tw_pcbs)) {
		tcp_timer_active = 1;
		__lwp_wd_insert_ticks(&tcp_time_cntrl,net_tcp_ticks);	
	}
}

/* netbuf part */
static inline u16 netbuf_len(struct netbuf *buf)
{
	return buf->p->tot_len;
}

static inline struct ip_addr* netbuf_fromaddr(struct netbuf *buf)
{
	return buf->fromaddr;
}

static inline u16 netbuf_fromport(struct netbuf *buf)
{
	return buf->fromport;
}

static void netbuf_copypartial(struct netbuf *buf,void *dataptr,u32 len,u32 offset)
{
	struct pbuf *p;
	u16 i,left;
	
	left = 0;
	if(buf==NULL || dataptr==NULL) return;

	for(p=buf->p;left<len && p!=NULL;p=p->next) {
		if(offset!=0 && offset>=p->len)
			offset -= p->len;
		else {
			for(i=offset;i<p->len;i++) {
				((u8*)dataptr)[left] = ((u8*)p->payload)[i];
				if(++left>=len) return;
			}
			offset = 0;
		}
	}
}

static struct netbuf* netbuf_new()
{
	struct netbuf *buf = NULL;

	buf = (struct netbuf*)malloc(sizeof(struct netbuf));
	if(buf) {
		buf->p = NULL;
		buf->ptr = NULL;
	}
	return buf;
}

static void netbuf_delete(struct netbuf *buf)
{
	if(buf!=NULL) {
		if(buf->p!=NULL) pbuf_free(buf->p);
		free(buf);
	}
}

static void* netbuf_alloc(struct netbuf *buf,u32 size)
{
	if(buf->p!=NULL) pbuf_free(buf->p);
	
	buf->p = pbuf_alloc(PBUF_TRANSPORT,size,PBUF_RAM);
	if(buf->p==NULL) return NULL;

	buf->ptr = buf->p;
	return buf->p->payload;
}

static void netbuf_free(struct netbuf *buf)
{
	if(buf->p!=NULL) pbuf_free(buf->p);
	buf->p = buf->ptr = NULL;
}

static void netbuf_ref(struct netbuf *buf,void *dataptr,u32 size)
{
	if(buf->p!=NULL) pbuf_free(buf->p);
	buf->p = pbuf_alloc(PBUF_TRANSPORT,0,PBUF_REF);
	buf->p->payload = dataptr;
	buf->p->len = buf->p->tot_len = size;
	buf->ptr = buf->p;
}

/* netconn part */
static inline enum netconn_type netconn_type(struct netconn *conn)
{
	return conn->type;
}

static struct netconn* netconn_new_with_callback(enum netconn_type t,void (*cb)(struct netconn*,enum netconn_evt,u32))
{
	return netconn_new_with_proto_and_callback(t,0,cb);
}

static struct netconn* netconn_new_with_proto_and_callback(enum netconn_type t,u16 proto,void (*cb)(struct netconn *,enum netconn_evt,u32))
{
	u32 dummy;
	struct netconn *conn;
	struct api_msg *msg;
	
	conn = (struct netconn*)malloc(sizeof(struct netconn));
	if(!conn) return NULL;

	conn->err = ERR_OK;
	conn->type = t;
	conn->pcb.tcp = NULL;

	conn->mbox = NULL;
	MQ_Init(&conn->mbox,MQBOX_SIZE);
	if(!conn->mbox) {
		free(conn);
		return NULL;
	}

	conn->sem = NULL;
	conn->acceptmbox = NULL;
	conn->recvmbox = NULL;
	conn->state = NETCONN_NONE;
	conn->socket = 0;
	conn->callback = cb;
	conn->recvavail = 0;
	
	msg = (struct api_msg*)malloc(sizeof(struct api_msg));
	if(!msg) {
		MQ_Close(conn->mbox);
		free(conn);
		return NULL;
	}
	
	msg->type = APIMSG_NEWCONN;
	msg->msg.msg.bc.port = proto;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);
	
	if(conn->err!=ERR_OK) {
		MQ_Close(conn->mbox);
		free(conn);
		return NULL;
	}
	return conn;
}

static err_t netconn_delete(struct netconn *conn)
{
	u32 dummy;
	struct api_msg *msg;
	void *mem;

	if(!conn) return ERR_OK;

	msg = (struct api_msg*)malloc(sizeof(struct api_msg));
	if(!msg) return ERR_MEM;

	msg->type = APIMSG_DELCONN;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);

	if(conn->recvmbox) {
		while(MQ_Receive(conn->recvmbox,&mem,MQ_MSG_NOBLOCK)==TRUE) {
			if(conn->type==NETCONN_TCP)
				pbuf_free((struct pbuf*)mem);
			else
				netbuf_delete((struct netbuf*)mem);
		}
		MQ_Close(conn->recvmbox);
		conn->recvmbox = NULL;
	}

	if(conn->acceptmbox) {
		while(MQ_Receive(conn->acceptmbox,&mem,MQ_MSG_NOBLOCK)==TRUE) {
			netconn_delete((struct netconn*)mem);
		}
		MQ_Close(conn->acceptmbox);
		conn->acceptmbox = NULL;
	}

	MQ_Close(conn->mbox);
	conn->mbox = NULL;
	if(conn->sem!=NULL) {
		LWP_SemDestroy(conn->sem);
		conn->sem = NULL;
	}
	
	free(conn);
	return ERR_OK;
}

static struct netconn* netconn_accept(struct netconn* conn)
{
	struct netconn *newconn;
	
	if(conn==NULL) return NULL;

	MQ_Receive(conn->acceptmbox,(mqmsg)&newconn,MQ_MSG_BLOCK);
	if(conn->callback)
		(*conn->callback)(conn,NETCONN_EVTRCVMINUS,0);

	return newconn;
}

static err_t netconn_peer(struct netconn *conn,struct ip_addr *addr,u16 *port)
{
	switch(conn->type) {
		case NETCONN_RAW:
			return ERR_CONN;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			if(conn->pcb.udp==NULL || ((conn->pcb.udp->flags&UDP_FLAGS_CONNECTED)==0))
				return ERR_CONN;
			*addr = conn->pcb.udp->remote_ip;
			*port = conn->pcb.udp->remote_port;
			break;
		case NETCONN_TCP:
			if(conn->pcb.tcp==NULL)
				return ERR_CONN;
			*addr = conn->pcb.tcp->remote_ip;
			*port = conn->pcb.tcp->remote_port;
			break;
	}
	return (conn->err = ERR_OK);
}

static err_t netconn_bind(struct netconn *conn,struct ip_addr *addr,u16 port)
{
	u32 dummy;
	struct api_msg *msg;

	if(conn==NULL) return ERR_VAL;
	if(conn->type!=NETCONN_TCP && conn->recvmbox==NULL) {
		if(MQ_Init(&conn->recvmbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) return ERR_MEM;
	}
	
	if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL)
		return (conn->err = ERR_MEM);

	msg->type = APIMSG_BIND;
	msg->msg.conn = conn;
	msg->msg.msg.bc.ipaddr = addr;
	msg->msg.msg.bc.port = port;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);
	return conn->err;
}

static err_t netconn_listen(struct netconn *conn)
{
	u32 dummy;
	struct api_msg *msg;
	
	if(conn==NULL) return -1;
	if(conn->acceptmbox==NULL) {
		MQ_Init(&conn->acceptmbox,MQBOX_SIZE);
		if(conn->acceptmbox==NULL) return ERR_MEM;
	}

	if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL) return (conn->err = ERR_MEM);
	msg->type = APIMSG_LISTEN;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);
	return conn->err;
}

static struct netbuf* netconn_recv(struct netconn *conn)
{
	u32 dummy;
	struct api_msg *msg;
	struct netbuf *buf;
	struct pbuf *p;
	u16 len;

	if(conn==NULL) return NULL;
	if(conn->recvmbox==NULL) {
		conn->err = ERR_CONN;
		return NULL;
	}
	if(conn->err!=ERR_OK) return NULL;
	
	if(conn->type==NETCONN_TCP) {
		if(conn->pcb.tcp->state==LISTEN) {
			conn->err = ERR_CONN;
			return NULL;
		}
		
		buf = (struct netbuf*)malloc(sizeof(struct netbuf));
		if(buf==NULL) {
			conn->err = ERR_MEM;
			return NULL;
		}
		
		MQ_Receive(conn->recvmbox,(mqmsg)&p,MQ_MSG_BLOCK);
		if(p!=NULL) {
			len = p->tot_len;
			conn->recvavail -= len;
		} else
			len = 0;

		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVMINUS,len);
		
		if(p==NULL) {
			free(buf);
			MQ_Close(conn->recvmbox);
			conn->recvmbox = NULL;
			return NULL;
		}

		buf->p = p;
		buf->ptr = p;
		buf->fromport = 0;
		buf->fromaddr = NULL;

		if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL) {
			conn->err = ERR_MEM;
			return buf;
		}

		msg->type = APIMSG_RECV;
		msg->msg.conn = conn;
		if(buf!=NULL)
			msg->msg.msg.len = len;
		else
			msg->msg.msg.len = 1;
		
		apimsg_post(msg);
		MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
		free(msg);
	} else {
		MQ_Receive(conn->recvmbox,(mqmsg)&buf,MQ_MSG_BLOCK);
		conn->recvavail -= buf->p->tot_len;
		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVMINUS,buf->p->tot_len);
	}
	
	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_recv: received %p (err %d)\n", (void *)buf, conn->err));
	return buf;
}

static err_t netconn_send(struct netconn *conn,struct netbuf *buf)
{
	u32 dummy;
	struct api_msg *msg;

	if(conn==NULL) return ERR_VAL;
	if(conn->err!=ERR_OK) return conn->err;
	if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL) return (conn->err = ERR_MEM);

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_send: sending %d bytes\n", buf->p->tot_len));
	msg->type = APIMSG_SEND;
	msg->msg.conn = conn;
	msg->msg.msg.p = buf->p;
	apimsg_post(msg);
	
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);
	return conn->err;
}

static err_t netconn_write(struct netconn *conn,void *dataptr,u32 size,u8 copy)
{
	u32 dummy;
	struct api_msg *msg;
	u16 len;

	if(conn==NULL) return ERR_VAL;
	if(conn->err!=ERR_OK) return conn->err;
	
	if(conn->sem==NULL)
		if(LWP_SemInit(&conn->sem,0,1)==-1) return ERR_MEM;

	if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL) return (conn->err = ERR_MEM);
	
	msg->type = APIMSG_WRITE;
	msg->msg.conn = conn;
	conn->state = NETCONN_WRITE;
	while(conn->err==ERR_OK && size>0) {
		msg->msg.msg.w.dataptr = dataptr;
		msg->msg.msg.w.copy = copy;
		if(conn->type==NETCONN_TCP) {
			if(tcp_sndbuf(conn->pcb.tcp)==0) {
				LWP_SemWait(conn->sem);
				LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write: tcp_sndbuf = 0,err = %d\n", conn->err));
				if(conn->err!=ERR_OK) break;
			}
			if(size>tcp_sndbuf(conn->pcb.tcp))
				len = tcp_sndbuf(conn->pcb.tcp);
			else
				len = size;
		} else
			len = size;

		LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write: writing %d bytes (%d)\n", len, copy));
		msg->msg.msg.w.len = len;
		apimsg_post(msg);
		MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
		if(conn->err==ERR_OK) {
			dataptr = (void*)((char*)dataptr+len);
			size -= len;
		} else if(conn->err==ERR_MEM) {
			conn->err = ERR_OK;
			LWP_SemWait(conn->sem);
		} else {
			LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write: err = %d\n", conn->err));
			break;
		}
	}
	free(msg);
	conn->state = NETCONN_NONE;
	if(conn->sem!=NULL) {
		LWP_SemDestroy(conn->sem);
		conn->sem =NULL;
	}

	return conn->err;
}

static err_t netconn_connect(struct netconn *conn,struct ip_addr *addr,u16 port)
{
	u32 dummy;
	struct api_msg *msg;

	if(conn==NULL) return -1;
	if(conn->recvmbox==NULL) {
		MQ_Init(&conn->recvmbox,MQBOX_SIZE);
		if(!conn->recvmbox) return ERR_MEM;
	}

	if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL) return ERR_MEM;
	
	msg->type = APIMSG_CONNECT;
	msg->msg.conn = conn;
	msg->msg.msg.bc.ipaddr = addr;
	msg->msg.msg.bc.port = port;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);
	return conn->err;
}

static err_t netconn_disconnect(struct netconn *conn)
{
	u32 dummy;
	struct api_msg *msg;
	
	if(conn==NULL) return ERR_VAL;
	if((msg=(struct api_msg*)malloc(sizeof(struct api_msg)))==NULL) return ERR_MEM;

	msg->type = APIMSG_DISCONNECT;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg)&dummy,MQ_MSG_BLOCK);
	free(msg);
	return conn->err;
}

/* api msg part */
static s32 recv_raw(void *arg,struct raw_pcb *pcb,struct pbuf *p,struct ip_addr *addr)
{
	struct netbuf *buf;
	struct netconn *conn = (struct netconn*)arg;

	if(!conn) return 0;

	if(conn->recvmbox) {
		if(!(buf=(struct netbuf*)malloc(sizeof(struct netbuf)))) return 0;

		pbuf_ref(p);
		buf->p = p;
		buf->ptr = p;
		buf->fromaddr = addr;
		buf->fromport = pcb->protocol;
		
		conn->recvavail += p->tot_len;
		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVPLUS,p->tot_len);
		MQ_Send(conn->recvmbox,&buf,MQ_MSG_BLOCK);
	}
	return 0;
}

static void recv_udp(void *arg,struct udp_pcb *pcb,struct pbuf *p,struct ip_addr *addr,u16 port)
{
	struct netbuf *buf;
	struct netconn *conn = (struct netconn*)arg;
	
	if(!conn) {
		pbuf_free(p);
		return;
	}

	if(conn->recvmbox) {
		buf = (struct netbuf*)malloc(sizeof(struct netbuf));
		if(!buf) {
			pbuf_free(p);
			return;
		}
		buf->p = p;
		buf->ptr = p;
		buf->fromaddr = addr;
		buf->fromport = port;

		conn->recvavail += p->tot_len;
		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVPLUS,p->tot_len);
		
		MQ_Send(conn->recvmbox,&buf,MQ_MSG_BLOCK);
	}
}

static err_t recv_tcp(void *arg,struct tcp_pcb *pcb,struct pbuf *p,err_t err)
{
	u16 len;
	struct netconn *conn = (struct netconn*)arg;

	if(conn==NULL) {
		pbuf_free(p);
		return ERR_VAL;
	}

	if(conn->recvmbox!=NULL) {
		conn->err = err;
		if(p!=NULL) {
			len = p->tot_len;
			conn->recvavail += len;
		} else len = 0;

		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVPLUS,len);

		MQ_Send(conn->recvmbox,&p,MQ_MSG_BLOCK);
	}
	return ERR_OK;
}

static void err_tcp(void *arg,err_t err)
{
	u32 dummy = 0;
	struct netconn *conn = (struct netconn*)arg;
	
	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: err_tcp: %d\n",err));
	if(conn) {
		conn->err = err;
		conn->pcb.tcp = NULL;
		if(conn->recvmbox) {
			if(conn->callback) (*conn->callback)(conn,NETCONN_EVTRCVPLUS,0);
			MQ_Send(conn->recvmbox,&dummy,MQ_MSG_BLOCK);
		}
		if(conn->mbox) {
			MQ_Send(conn->mbox,&dummy,MQ_MSG_BLOCK);
		}
		if(conn->acceptmbox) {
			if(conn->callback) (*conn->callback)(conn,NETCONN_EVTRCVPLUS,0);
			MQ_Send(conn->acceptmbox,&dummy,MQ_MSG_BLOCK);
		}
		if(conn->sem)
			LWP_SemPost(conn->sem);
	}
}

static err_t poll_tcp(void *arg,struct tcp_pcb *pcb)
{
	struct netconn *conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: poll_tcp\n"));
	if(conn && conn->sem && (conn->type==NETCONN_WRITE || conn->type==NETCONN_CLOSE))
		LWP_SemPost(conn->sem);
	
	return ERR_OK;
}

static err_t sent_tcp(void *arg,struct tcp_pcb *pcb,u16 len)
{
	struct netconn *conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: sent_tcp: sent %d bytes\n",len));
	if(conn && conn->sem)
		LWP_SemPost(conn->sem);

	if(conn && conn->callback) {
		if(tcp_sndbuf(conn->pcb.tcp)>TCP_SNDLOWAT) 
			(*conn->callback)(conn,NETCONN_EVTSENDPLUS,len);
	}
	return ERR_OK;
}

static void setuptcp(struct netconn *conn)
{
	struct tcp_pcb *pcb = conn->pcb.tcp;
	
	tcp_arg(pcb,conn);
	tcp_recv(pcb,recv_tcp);
	tcp_sent(pcb,sent_tcp);
	tcp_poll(pcb,poll_tcp,4);
	tcp_err(pcb,err_tcp);
}

static err_t accept_func(void *arg,struct tcp_pcb *newpcb,err_t err)
{
	struct netconn *newconn,*conn = (struct netconn*)arg;
	mq_box_t mbox = conn->acceptmbox;
#ifdef _NET_DEUG
	printf("accept_func() enter\n\n");
#endif
	newconn = (struct netconn*)malloc(sizeof(struct netconn));
	if(newconn==NULL) return ERR_MEM;

	newconn->type = NETCONN_TCP;
	newconn->pcb.tcp = newpcb;
	setuptcp(newconn);

	MQ_Init(&newconn->recvmbox,MQBOX_SIZE);
	if(newconn->recvmbox==NULL) {
		free(newconn);
		return ERR_MEM;
	}

	MQ_Init(&newconn->mbox,MQBOX_SIZE);
	if(newconn->mbox==NULL) {
		MQ_Close(newconn->recvmbox);
		free(newconn);
		return ERR_MEM;
	}
	
	if(LWP_SemInit(&newconn->sem,0,1)==-1) {
		MQ_Close(newconn->recvmbox);
		MQ_Close(newconn->mbox);
		free(newconn);
		return ERR_MEM;
	}
	
	newconn->acceptmbox = NULL;
	newconn->err = err;
	if(conn->callback) {
		(*conn->callback)(conn,NETCONN_EVTRCVPLUS,0);
		newconn->callback = conn->callback;
		newconn->socket = -1;
	}

	MQ_Send(mbox,&newconn,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void do_newconn(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
		return;
	}

	msg->conn->err = ERR_OK;
	switch(msg->conn->type) {
		case NETCONN_RAW:
			msg->conn->pcb.raw = raw_new(msg->msg.bc.port);
			raw_recv(msg->conn->pcb.raw,recv_raw,msg->conn);
			break;
		case NETCONN_UDPLITE:
			msg->conn->pcb.udp = udp_new();
			if(!msg->conn->pcb.udp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_UDPLITE);
			udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
			break;
		case NETCONN_UDPNOCHKSUM:
			msg->conn->pcb.udp = udp_new();
			if(!msg->conn->pcb.udp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_NOCHKSUM);
			udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
			break;
		case NETCONN_UDP:
			msg->conn->pcb.udp = udp_new();
			if(!msg->conn->pcb.udp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
			break;
		case NETCONN_TCP:
			msg->conn->pcb.tcp = tcp_new();
			if(!msg->conn->pcb.tcp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			setuptcp(msg->conn);
			break;
		default:
			break;
	}
	MQ_Send(msg->conn->mbox,(void*)&dummy,MQ_MSG_BLOCK);
}

static void do_delconn(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				raw_remove(msg->conn->pcb.raw);
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				msg->conn->pcb.udp->recv_arg = NULL;
				udp_remove(msg->conn->pcb.udp);
				break;
			case NETCONN_TCP:
				if(msg->conn->pcb.tcp->state==LISTEN) {
					tcp_arg(msg->conn->pcb.tcp,NULL);
					tcp_accept(msg->conn->pcb.tcp,NULL);
					tcp_close(msg->conn->pcb.tcp);
				} else {
					tcp_arg(msg->conn->pcb.tcp,NULL);
					tcp_sent(msg->conn->pcb.tcp,NULL);
					tcp_recv(msg->conn->pcb.tcp,NULL);
					tcp_poll(msg->conn->pcb.tcp,NULL,0);
					tcp_err(msg->conn->pcb.tcp,NULL);
					if(tcp_close(msg->conn->pcb.tcp)!=ERR_OK)
						tcp_abort(msg->conn->pcb.tcp);
				}
				break;
			default:
				break;
		}
	}
	if(msg->conn->callback) {
		(*msg->conn->callback)(msg->conn,NETCONN_EVTRCVPLUS,0);
		(*msg->conn->callback)(msg->conn,NETCONN_EVTSENDPLUS,0);
	}
	if(msg->conn->mbox)
		MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_bind(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp==NULL) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				msg->conn->pcb.raw = raw_new(msg->msg.bc.port);
				raw_recv(msg->conn->pcb.raw,recv_raw,msg->conn);
				break;
			case NETCONN_UDPLITE:
				msg->conn->pcb.udp = udp_new();
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_UDPLITE);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDPNOCHKSUM:
				msg->conn->pcb.udp = udp_new();
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_NOCHKSUM);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDP:
				msg->conn->pcb.udp = udp_new();
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_TCP:
				msg->conn->pcb.tcp = tcp_new();
				setuptcp(msg->conn);
				break;
			default:
				break;
		}
	}
	switch(msg->conn->type) {
		case NETCONN_RAW:
			msg->conn->err = raw_bind(msg->conn->pcb.raw,msg->msg.bc.ipaddr);
			break;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			msg->conn->err = udp_bind(msg->conn->pcb.udp,msg->msg.bc.ipaddr,msg->msg.bc.port);
			break;
		case NETCONN_TCP:
			msg->conn->err = tcp_bind(msg->conn->pcb.tcp,msg->msg.bc.ipaddr,msg->msg.bc.port);
			break;
		default:
			break;
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static err_t do_connected(void *arg,struct tcp_pcb *pcb,err_t err)
{
	u32 dummy = 0;
	struct netconn *conn = (struct netconn*)arg;

	if(!conn) return ERR_VAL;

	conn->err = err;
	if(conn->type==NETCONN_TCP && err==ERR_OK) setuptcp(conn);
	
	MQ_Send(conn->mbox,&dummy,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void do_connect(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(!msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				msg->conn->pcb.raw = raw_new(msg->msg.bc.port);
				raw_recv(msg->conn->pcb.raw,recv_raw,msg->conn);
				break;
			case NETCONN_UDPLITE:
				msg->conn->pcb.udp = udp_new();
				if(!msg->conn->pcb.udp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_UDPLITE);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDPNOCHKSUM:
				msg->conn->pcb.udp = udp_new();
				if(!msg->conn->pcb.udp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_NOCHKSUM);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDP:
				msg->conn->pcb.udp = udp_new();
				if(!msg->conn->pcb.udp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_TCP:
				msg->conn->pcb.tcp = tcp_new();
				if(!msg->conn->pcb.tcp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				break;
			default:
				break;
		}
	}
	switch(msg->conn->type) {
		case NETCONN_RAW:
			raw_connect(msg->conn->pcb.raw,msg->msg.bc.ipaddr);
			MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
			break;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			udp_connect(msg->conn->pcb.udp,msg->msg.bc.ipaddr,msg->msg.bc.port);
			MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
			break;
		case NETCONN_TCP:
			setuptcp(msg->conn);
			tcp_connect(msg->conn->pcb.tcp,msg->msg.bc.ipaddr,msg->msg.bc.port,do_connected);
			break;
		default:
			break;
	}
}
	
static void do_disconnect(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	switch(msg->conn->type) {
		case NETCONN_RAW:
			break;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			udp_disconnect(msg->conn->pcb.udp);
			break;
		case NETCONN_TCP:
			break;
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_listen(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp!=NULL) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: listen RAW: cannot listen for RAW.\n"));
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: listen UDP: cannot listen for UDP.\n"));
				break;
			case NETCONN_TCP:
				msg->conn->pcb.tcp = tcp_listen(msg->conn->pcb.tcp);
				if(msg->conn->pcb.tcp==NULL) 
					msg->conn->err = ERR_MEM;
				else {
					if(msg->conn->acceptmbox==NULL) {
						MQ_Init(&msg->conn->acceptmbox,MQBOX_SIZE);
						if(msg->conn->acceptmbox==NULL) {
							msg->conn->err = ERR_MEM;
							break;
						}
					}
					tcp_arg(msg->conn->pcb.tcp,msg->conn);
					tcp_accept(msg->conn->pcb.tcp,accept_func);
				}
				break;
			default:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_accept(struct apimsg_msg *msg)
{
	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: accept RAW: cannot accept for RAW.\n"));
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: accept UDP: cannot accept for UDP.\n"));
				break;
			case NETCONN_TCP:
				break;
		}
	}
}

static void do_send(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				raw_send(msg->conn->pcb.raw,msg->msg.p);
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				udp_send(msg->conn->pcb.udp,msg->msg.p);
				break;
			case NETCONN_TCP:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_recv(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp && msg->conn->type==NETCONN_TCP) {
		tcp_recved(msg->conn->pcb.tcp,msg->msg.len);
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_write(struct apimsg_msg *msg)
{
	err_t err;
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				msg->conn->err = ERR_VAL;
				break;
			case NETCONN_TCP:
				err = tcp_write(msg->conn->pcb.tcp,msg->msg.w.dataptr,msg->msg.w.len,msg->msg.w.copy);
				if(err==ERR_OK && (!msg->conn->pcb.tcp->unacked || (msg->conn->pcb.tcp->flags&TF_NODELAY))) {
					LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: TCP write: tcp_output.\n"));
					tcp_output(msg->conn->pcb.tcp);
				}
				msg->conn->err = err;
				if(msg->conn->callback) {
					if(err==ERR_OK) {
						if(tcp_sndbuf(msg->conn->pcb.tcp)<=TCP_SNDLOWAT) 
							(*msg->conn->callback)(msg->conn,NETCONN_EVTSENDMINUS,msg->msg.w.len);
					}
				}
				break;
			default:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_close(struct apimsg_msg *msg)
{
	u32 dummy = 0;
	err_t err = ERR_OK;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				break;
			case NETCONN_TCP:
				if(msg->conn->pcb.tcp->state==LISTEN)
					err = tcp_close(msg->conn->pcb.tcp);
				msg->conn->err = err;
				break;
			default:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void apimsg_input(struct api_msg *msg)
{
	decode[msg->type](&(msg->msg));
}

static void apimsg_post(struct api_msg *msg)
{
	net_apimsg(msg);
}

/* tcpip thread part */
static err_t net_input(struct pbuf *p,struct netif *inp)
{
	struct net_msg *msg = (struct net_msg*)malloc(sizeof(struct net_msg));

	if(msg==NULL) {
		pbuf_free(p);
		return ERR_MEM;
	}

	msg->type = NETMSG_INPUT;
	msg->msg.inp.p = p;
	msg->msg.inp.net = inp;
	MQ_Send(netthread_mbox,&msg,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void net_apimsg(struct api_msg *apimsg)
{
	struct net_msg *msg = (struct net_msg*)malloc(sizeof(struct net_msg));

	if(msg==NULL) {
		free(apimsg);
		return;
	}

	msg->type = NETMSG_API;
	msg->msg.apimsg = apimsg;
	MQ_Send(netthread_mbox,&msg,MQ_MSG_BLOCK);
}

static err_t net_callback(void (*f)(void *),void *ctx)
{
	struct net_msg *msg = (struct net_msg*)malloc(sizeof(struct net_msg));

	if(msg==NULL)
		return ERR_MEM;
	
	msg->type = NETMSG_CALLBACK;
	msg->msg.cb.f = f;
	msg->msg.cb.ctx = ctx;
	MQ_Send(netthread_mbox,&msg,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void* net_thread(void *arg)
{
	struct net_msg *msg;
	struct timespec tb;
	sem_t sem = (sem_t)arg;

	ip_init();
	udp_init();
	tcp_init();

	tb.tv_sec = 0;
	tb.tv_nsec = TCP_TMR_INTERVAL*1000*1000;
	net_tcp_ticks = timespec_to_interval(&tb);
	__lwp_wd_initialize(&tcp_time_cntrl,__tcp_timer,NULL);
	
	LWP_SemPost(sem);
	
#ifdef _NET_DEBUG
	printf("net_thread(%p)\n",arg);
#endif
	while(1) {
		MQ_Receive(netthread_mbox,(mqmsg)&msg,MQ_MSG_BLOCK);
		switch(msg->type) {
			case NETMSG_API:
			    LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread: API message %p\n", (void *)msg));
				apimsg_input(msg->msg.apimsg);
				break;
			case NETMSG_INPUT:
			    LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread: IP packet %p\n", (void *)msg));
				ip_input(msg->msg.inp.p,msg->msg.inp.net);
				break;
			case NETMSG_CALLBACK:
			    LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread: CALLBACK %p\n", (void *)msg));
				msg->msg.cb.f(msg->msg.cb.ctx);
				break;
			default:
				break;
		}
		free(msg);
	}
}

/* sockets part */
static u32 alloc_socket(struct netconn *conn)
{
	u32 i;
	
	LWP_SemWait(netsocket_sem);
	
	for(i=0;i<64;i++) {
		if(!sockets[i].conn) {
			sockets[i].conn = conn;
			sockets[i].lastdata = NULL;
			sockets[i].lastoffset = 0;
			sockets[i].rcvevt = 0;
			sockets[i].sendevt = 1;
			sockets[i].flags = 0;
			sockets[i].err = 0;
			LWP_SemPost(netsocket_sem);
			return i;
		}
	}

	LWP_SemPost(netsocket_sem);
	return -1;
}

static struct netsocket* get_socket(u32 s)
{
	struct netsocket *sock;
	if(s<0 || s>NUM_SOCKETS)
		return NULL;

	sock = &sockets[s];
	if(!sock->conn) return NULL;

	return sock;
}

static void evt_callback(struct netconn *conn,enum netconn_evt evt,u32 len)
{
	u32 s;
	struct netsocket *sock;
	struct netselect_cb *scb;
	
	if(conn) {
		s = conn->socket;
		if(s<0) {
			if(evt==NETCONN_EVTRCVPLUS)
				conn->socket--;
			return;
		}
		sock = get_socket(s);
		if(!sock) return;
	} else
		return;

	LWP_SemWait(sockselect_sem);
	switch(evt) {
		case NETCONN_EVTRCVPLUS:
			sock->rcvevt++;
			break;
		case NETCONN_EVTRCVMINUS:
			sock->rcvevt--;
			break;
		case NETCONN_EVTSENDPLUS:
			sock->sendevt = 1;
			break;
		case NETCONN_EVTSENDMINUS:
			sock->sendevt = 0;
			break;
	}
	LWP_SemPost(sockselect_sem);

	while(1) {
		LWP_SemWait(sockselect_sem);
		for(scb = selectcb_list;scb;scb = scb->next) {
			if(scb->sem_signaled==0) {
				if(scb->readset && FD_ISSET(s,scb->readset))
					if(sock->rcvevt) break;
				if(scb->writeset && FD_ISSET(s,scb->writeset))
					if(sock->sendevt) break;
			}
		}
		if(scb) {
			scb->sem_signaled = 1;
			LWP_SemPost(sockselect_sem);
			LWP_SemPost(scb->sem);
		} else {
			LWP_SemPost(sockselect_sem);
			break;
		}
	}
	
}

static void net_getipfrom(const s8 *pszIp,u32 ret_ip[])
{
	s32 cnt = 0;
	s8 *ptr = (s8*)pszIp;
	if(ptr) {
		while(*ptr!='\0') {
			if(*ptr=='.' && cnt<4) {
				ret_ip[cnt++] = atoi(pszIp);
				pszIp = ptr+1;
			}
			ptr++;
		}
		ret_ip[cnt] = atoi(pszIp);
	}
}

extern devoptab_t dotab_net;
u32 if_config(const char *pszIP,const char *pszGW,const char *pszMASK,boolean use_dhcp)
{
	u32 loc_ip[4] = {0,0,0,0};
	u32 gw_ip[4] = {0,0,0,0};
	u32 netmask_ip[4] = {0,0,0,0};
	struct ip_addr ipaddr, netmask, gw;
	struct netif *pnet;

	if(g_netinitiated) return 0;
	g_netinitiated = 1;

	devoptab_list[STD_NET] = &dotab_net;
#ifdef STATS
	stats_init();
#endif /* STATS */
	
	sys_init();
	mem_init();
	memp_init();
	pbuf_init();
	netif_init();

	// setup interface 
	if(!use_dhcp) {
		if(pszIP && pszGW && pszMASK) {
			net_getipfrom(pszIP,loc_ip);
			net_getipfrom(pszGW,gw_ip);
			net_getipfrom(pszMASK,netmask_ip);
		} else
			return -1;
	}

	IP4_ADDR(&gw,gw_ip[0],gw_ip[1],gw_ip[2],gw_ip[3]);
	IP4_ADDR(&ipaddr, loc_ip[0],loc_ip[1],loc_ip[2],loc_ip[3]);
	IP4_ADDR(&netmask,netmask_ip[0],netmask_ip[1],netmask_ip[2],netmask_ip[3]);

	pnet = netif_add(&g_hNetIF,&ipaddr, &netmask, &gw, NULL, bba_init, net_input);
	if(pnet) {
		netif_set_default(pnet);
#if (LWIP_DHCP)
		if(use_dhcp==TRUE) 
			dhcp_start(pnet);
#endif
	} else
		return -1;
	
	// setup loopinterface
	IP4_ADDR(&gw, 127,0,0,1);
	IP4_ADDR(&ipaddr, 127,0,0,1);
	IP4_ADDR(&netmask, 255,0,0,0);
	pnet = netif_add(&g_hLoopIF,&ipaddr,&netmask,&gw,NULL,loopif_init,net_input);

	return 0;
}
u32 net_init()
{
	u32 ret = -1;
	sem_t sem;

	if(tcpiplayer_inited) return 0;

	if(LWP_SemInit(&netsocket_sem,1,1)==-1) return -1;
	if(LWP_SemInit(&sockselect_sem,1,1)==-1) return -1;

	MQ_Init(&netthread_mbox,MQBOX_SIZE);
	if(!netthread_mbox) return -1;

	if(LWP_SemInit(&sem,0,1)==-1) {
		MQ_Close(netthread_mbox);
		return -1;
	}

	if((ret=LWP_CreateThread(&hnet_thread,net_thread,sem,netthread_stack,STACKSIZE,220))!=-1) {
		LWP_SemWait(sem);
		LWP_SemDestroy(sem);
		tcpiplayer_inited = 1;
		ret = 0;
	}
	return ret;
}

u32 net_socket(u32 domain,u32 type,u32 protocol)
{
	u32 i;
	struct netconn *conn;
	
	switch(type) {
		case SOCK_RAW:
			conn = netconn_new_with_proto_and_callback(NETCONN_RAW,protocol,evt_callback);
			break;
		case SOCK_DGRAM:
			conn = netconn_new_with_callback(NETCONN_UDP,evt_callback);
			break;
		case SOCK_STREAM:
			conn = netconn_new_with_callback(NETCONN_TCP,evt_callback);
			break;
		default:
			return -1;
	}
	if(!conn) return -1;
	
	i = alloc_socket(conn);
	if(i==-1) {
		netconn_delete(conn);
		return -1;
	}
	
	conn->socket = i;
	return i;
}

u32 net_accept(u32 s,struct sockaddr *addr,socklen_t *addrlen)
{
	struct netsocket *sock;
	struct netconn *newconn;
	struct ip_addr naddr;
	u16 port;
	u32 newsock;
	struct sockaddr_in sin;
	
	sock = get_socket(s);
	if(!sock) return -1;
#ifdef _NET_DEBUG
	printf("netconn_accept(%d)\n",s);
#endif
	newconn = netconn_accept(sock->conn);
#ifdef _NET_DEBUG
	printf("netconn_accept(%p)\n",newconn);
#endif
	
	netconn_peer(newconn,&naddr,&port);
	
	memset(&sin,0,sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = naddr.addr;
	
	if(*addrlen>sizeof(sin))
		*addrlen = sizeof(sin);
	memcpy(addr,&sin,*addrlen);
	
	newsock = alloc_socket(newconn);
	if(newsock==-1) {
		netconn_delete(newconn);
		return -1;
	}

	newconn->callback = evt_callback;
	sock = get_socket(newsock);
	
	LWP_SemWait(netsocket_sem);
	sock->rcvevt += -1 - newconn->socket;
	newconn->socket = newsock;
	LWP_SemPost(netsocket_sem);

	return newsock;
}

u32 net_bind(u32 s,struct sockaddr *name,socklen_t namelen)
{
	struct netsocket *sock;
	struct ip_addr loc_addr;
	u16 loc_port;
	err_t err;
	
	sock = get_socket(s);
	if(!sock) return -1;

	loc_addr.addr = ((struct sockaddr_in*)name)->sin_addr.s_addr;
	loc_port = ((struct sockaddr_in*)name)->sin_port;

	err = netconn_bind(sock->conn,&loc_addr,ntohs(loc_port));
	if(err!=ERR_OK) return -1;

	return 0;
}

u32 net_listen(u32 s,u32 backlog)
{
	struct netsocket *sock;
	err_t err;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_listen(%d, backlog=%d)\n", s, backlog));
	sock = get_socket(s);
	if(!sock) return -1;
	
	err = netconn_listen(sock->conn);
	if(err!=ERR_OK) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_listen(%d) failed, err=%d\n", s, err));
		return -1;
	}
	return 0;
}

u32 net_recvfrom(u32 s,void *mem,u32 len,u32 flags,struct sockaddr *from,socklen_t *fromlen)
{
	struct netsocket *sock;
	struct netbuf *buf;
	u16 buflen,copylen;
	struct ip_addr *addr;
	u16 port;
	
	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d, %p, %d, 0x%x, ..)\n", s, mem, len, flags));
	sock = get_socket(s);
	if(!sock) return -1;

	if(sock->lastdata)
		buf = sock->lastdata;
	else {
		if(((flags&MSG_DONTWAIT) || (sock->flags&O_NONBLOCK)) && !sock->rcvevt) {
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d): returning EWOULDBLOCK\n", s));
			return -1;
		}
		buf = netconn_recv(sock->conn);
		if(!buf) {
		    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d): buf == NULL!\n", s));
			return 0;
		}
	}
	
	buflen = netbuf_len(buf);
	buflen -= sock->lastoffset;

	if(len>buflen)
		copylen = buflen;
	else
		copylen = len;

	netbuf_copypartial(buf,mem,copylen,sock->lastoffset);

	if(from && fromlen) {
		struct sockaddr_in sin;

		addr = netbuf_fromaddr(buf);
		port = netbuf_fromport(buf);

		memset(&sin,0,sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = addr->addr;

		if(*fromlen>sizeof(sin))
			*fromlen = sizeof(sin);

		memcpy(from,&sin,*fromlen);

		LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d): addr=", s));
		ip_addr_debug_print(SOCKETS_DEBUG, addr);
		LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%u len=%u\n", port, copylen));
	}
	if(netconn_type(sock->conn)==NETCONN_TCP && (buflen-copylen)>0) {
		sock->lastdata = buf;
		sock->lastoffset += copylen;
	} else {
		sock->lastdata = NULL;
		sock->lastoffset = 0;
		netbuf_delete(buf);
	}
	return copylen;
}

u32 net_read(u32 s,void *mem,u32 len)
{
	return net_recvfrom(s,mem,len,0,NULL,NULL);
}

u32 net_recv(u32 s,void *mem,u32 len,u32 flags)
{
	return net_recvfrom(s,mem,len,flags,NULL,NULL);
}

u32 net_sendto(u32 s,void *data,u32 len,u32 flags,struct sockaddr *to,socklen_t tolen)
{
	struct netsocket *sock;
	struct ip_addr remote_addr, addr;
	u16_t remote_port, port;
	int ret,connected;

	sock = get_socket(s);
	if (!sock) return -1;

	/* get the peer if currently connected */
	connected = (netconn_peer(sock->conn, &addr, &port) == ERR_OK);

	remote_addr.addr = ((struct sockaddr_in *)to)->sin_addr.s_addr;
	remote_port = ((struct sockaddr_in *)to)->sin_port;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_sendto(%d, data=%p, size=%d, flags=0x%x to=", s, data, len, flags));
	ip_addr_debug_print(SOCKETS_DEBUG, &remote_addr);
	LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%u\n", ntohs(remote_port)));

	netconn_connect(sock->conn, &remote_addr, ntohs(remote_port));

	ret = net_send(s, data, len, flags);

	/* reset the remote address and port number
	of the connection */
	if (connected)
		netconn_connect(sock->conn, &addr, port);
	else
		netconn_disconnect(sock->conn);
	return ret;
}

u32 net_send(u32 s,void *data,u32 len,u32 flags)
{
	struct netsocket *sock;
	struct netbuf *buf;
	err_t err;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d, data=%p, size=%d, flags=0x%x)\n", s, data, len, flags));

	sock = get_socket(s);
	if(!sock) return -1;

	switch(netconn_type(sock->conn)) {
		case NETCONN_RAW:
		case NETCONN_UDP:
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
			buf = netbuf_new();
			if(!buf) {
				LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d) ENOBUFS\n", s));
				return -1;
			}
			netbuf_ref(buf,data,len);
			err = netconn_send(sock->conn,buf);
			netbuf_delete(buf);
			break;
		case NETCONN_TCP:
			err = netconn_write(sock->conn,data,len,NETCONN_COPY);
			break;
		default:
			err = ERR_ARG;
			break;
	}
	if(err!=ERR_OK) {
		LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d) err=%d\n", s, err));
		return -1;
	}

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d) ok size=%d\n", s, len));
	return len;
}

u32 net_write(u32 s,void *data,u32 size)
{
	return net_send(s,data,size,0);
}

u32 net_connect(u32 s,struct sockaddr *name,socklen_t namelen)
{
	struct netsocket *sock;
	err_t err;
	
	sock = get_socket(s);
	if(!sock) return -1;

	if(((struct sockaddr_in*)name)->sin_family==AF_UNSPEC) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d, AF_UNSPEC)\n", s));
		err = netconn_disconnect(sock->conn);
	} else {
		struct ip_addr remote_addr;
		u16 remote_port;

		remote_addr.addr = ((struct sockaddr_in*)name)->sin_addr.s_addr;
		remote_port = ((struct sockaddr_in*)name)->sin_port;
		
		LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d, addr=", s));
		ip_addr_debug_print(SOCKETS_DEBUG, &remote_addr);
		LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%u)\n", ntohs(remote_port)));

		err = netconn_connect(sock->conn,&remote_addr,ntohs(remote_port));
	}
	if(err!=ERR_OK) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d) failed, err=%d\n", s, err));
		return -1;
	}

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d) succeeded\n", s));
	return 0;
}

u32 net_close(u32 s)
{
	struct netsocket *sock;

	LWP_SemWait(netsocket_sem);
	
	sock = get_socket(s);
	if(!sock) {
		LWP_SemPost(netsocket_sem);
		return -1;
	}
	
	netconn_delete(sock->conn);
	if(sock->lastdata) netbuf_delete(sock->lastdata);
	
	sock->lastdata = NULL;
	sock->lastoffset = 0;
	sock->conn = NULL;
	
	LWP_SemPost(netsocket_sem);
	return 0;
}

static u32 net_selscan(u32 maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset)
{
	u32 i,nready = 0;
	fd_set lreadset,lwriteset,lexceptset;
	struct netsocket *sock;

	FD_ZERO(&lreadset);
	FD_ZERO(&lwriteset);
	FD_ZERO(&lexceptset);

	for(i=0;i<maxfdp1;i++) {
		if(FD_ISSET(i,readset)) {
			sock = get_socket(i);
			if(sock && (sock->lastdata || sock->rcvevt)) {
				FD_SET(i,&lreadset);
				nready++;
			}
		}
		if(FD_ISSET(i,writeset)) {
			sock = get_socket(i);
			if(sock && sock->sendevt) {
				FD_SET(i,&lwriteset);
				nready++;
			}
		}
	}
	*readset = lreadset;
	*writeset = lwriteset;
	FD_ZERO(exceptset);
	
	return nready;
}

u32 net_select(u32 maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset,struct timeval *timeout)
{
	u32 i,nready;
	fd_set lreadset,lwriteset,lexceptset;
	u32 msectimeout;
	struct netselect_cb sel_cb;
	struct netselect_cb *psel_cb;
	
	sel_cb.next = NULL;
	sel_cb.readset = readset;
	sel_cb.writeset = writeset;
	sel_cb.exceptset = exceptset;
	sel_cb.sem_signaled = 0;
	
	LWP_SemWait(sockselect_sem);
	
	if(readset)
		lreadset = *readset;
	else
		FD_ZERO(&lreadset);
	
	if(writeset)
		lwriteset = *writeset;
	else
		FD_ZERO(&lwriteset);

	if(exceptset)
		lexceptset = *exceptset;
	else
		FD_ZERO(&lexceptset);

	nready = net_selscan(maxfdp1,&lreadset,&lwriteset,&lexceptset);
	if(!nready) {
		if(timeout && timeout->tv_sec==0 && timeout->tv_usec==0) {
			LWP_SemPost(sockselect_sem);
			if(readset)
				FD_ZERO(readset);
			if(writeset)
				FD_ZERO(writeset);
			if(exceptset)
				FD_ZERO(exceptset);
			return 0;
		}

		LWP_SemInit(&sel_cb.sem,0,1);
		sel_cb.next = selectcb_list;
		selectcb_list = &sel_cb;

		LWP_SemPost(sockselect_sem);
		if(timeout==NULL)
			msectimeout = 0;
		else
			msectimeout = ((timeout->tv_sec*1000)+((timeout->tv_usec+500)/1000));
		
		i = LWP_SemWait(sel_cb.sem);			//todo: implement watchdog on lower level

		LWP_SemWait(sockselect_sem);
		if(selectcb_list==&sel_cb)
			selectcb_list = sel_cb.next;
		else {
			for(psel_cb = selectcb_list;psel_cb;psel_cb = psel_cb->next) {
				if(psel_cb->next==&sel_cb) {
					psel_cb->next = sel_cb.next;
					break;
				}
			}
		}
		LWP_SemPost(sockselect_sem);
		
		LWP_SemDestroy(sel_cb.sem);
		if(i==-1) {
			if(readset)
				FD_ZERO(readset);
			if(writeset)
				FD_ZERO(writeset);
			if(exceptset)
				FD_ZERO(exceptset);
			return 0;
		}
		
		if(readset)
			lreadset = *readset;
		else
			FD_ZERO(&lreadset);
		
		if(writeset)
			lwriteset = *writeset;
		else
			FD_ZERO(&lwriteset);

		if(exceptset)
			lexceptset = *exceptset;
		else
			FD_ZERO(&lexceptset);

		nready = net_selscan(maxfdp1,&lreadset,&lwriteset,&lexceptset);
	} else
		LWP_SemPost(sockselect_sem);

	if(readset)
		*readset = lreadset;
	if(writeset)
		*writeset = lwriteset;
	if(exceptset)
		*exceptset = lexceptset;

	return nready;
}

u32 net_setsockopt(u32 s,u32 level,u32 optname,const void *optval,socklen_t optlen)
{
	u32 err = 0;
	struct netsocket *sock;

	sock = get_socket(s);
	if(sock==NULL) return -1;
	if(optval==NULL) return -1;

	switch(level) {
		case SOL_SOCKET:
		{
			switch(optname) {
				case SO_BROADCAST:
				case SO_KEEPALIVE:
				case SO_REUSEADDR:
				case SO_REUSEPORT:
					if(optlen<sizeof(u32)) err = EINVAL;
					break;
				default:
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, SOL_SOCKET, UNIMPL: optname=0x%x, ..)\n", s, optname));
					err = ENOPROTOOPT;
			}
		}
		break;

		case IPPROTO_IP:
		{
			switch(optname) {
				case IP_TTL:
				case IP_TOS:
					if(optlen<sizeof(u32)) err = EINVAL;
					break;					
				default:
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_IP, UNIMPL: optname=0x%x, ..)\n", s, optname));
					err = ENOPROTOOPT;
			}
		}
		break;

		case  IPPROTO_TCP:
		{
			if(optlen<sizeof(u32)) {
				err = EINVAL;
				break;
			}
			if(sock->conn->type!=NETCONN_TCP) return 0;
			
			switch(optname) {
				case TCP_NODELAY:
				case TCP_KEEPALIVE:
					break;
				default:
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_TCP, UNIMPL: optname=0x%x, ..)\n", s, optname));
					err = ENOPROTOOPT;
			}
		}
		break;

		default:
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, level=0x%x, UNIMPL: optname=0x%x, ..)\n", s, level, optname));
			err = ENOPROTOOPT;
	}
	if(err!=0) return -1;

	switch(level) {
		case SOL_SOCKET:
		{
			switch(optname) {
				case SO_BROADCAST:
				case SO_KEEPALIVE:
				case SO_REUSEADDR:
				case SO_REUSEPORT:
					if(*(u32*)optval)
						sock->conn->pcb.tcp->so_options |= optname;
					else
						sock->conn->pcb.tcp->so_options &= ~optname;
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, SOL_SOCKET, optname=0x%x, ..) -> %s\n", s, optname, (*(u32*)optval?"on":"off")));
					break;
			}
		}
		break;

		case IPPROTO_IP:
		{
			switch(optname) {
				case IP_TTL:
					sock->conn->pcb.tcp->ttl = (u8)(*(u32*)optval);
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_IP, IP_TTL, ..) -> %u\n", s, sock->conn->pcb.tcp->ttl));
					break;
				case IP_TOS:
					sock->conn->pcb.tcp->tos = (u8)(*(u32*)optval);
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_IP, IP_TOS, ..)-> %u\n", s, sock->conn->pcb.tcp->tos));
					break;
			}
		}
		break;

		case  IPPROTO_TCP:
		{
			switch(optname) {
				case TCP_NODELAY:
					if(*(u32*)optval)
						sock->conn->pcb.tcp->flags |= TF_NODELAY;
					else
						sock->conn->pcb.tcp->flags &= ~TF_NODELAY;
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_TCP, TCP_NODELAY) -> %s\n", s, (*(u32*)optval)?"on":"off") );
					break;
				case TCP_KEEPALIVE:
					sock->conn->pcb.tcp->keepalive = (u32)(*(u32*)optval);
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_TCP, TCP_KEEPALIVE) -> %u\n", s, sock->conn->pcb.tcp->keepalive));
					break;
			}
		}
	}
	return err?-1:0;
}
