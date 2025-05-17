
add_library(db STATIC
	uIP/uip_ip.c
	uIP/uip_tcp.c
	uIP/uip_pbuf.c
	uIP/uip_netif.c
	uIP/uip_arp.c
	uIP/uip_arch.c
	uIP/uip_icmp.c
	uIP/memb.c
	uIP/memr.c
	uIP/bba.c
	tcpip.c
	debug.c
	debug_handler.S
	debug_supp.c
	geckousb.c
)

target_link_libraries(db PRIVATE libogc_inc)

libogc_install_lib(db)
