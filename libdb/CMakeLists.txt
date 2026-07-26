
add_library(db STATIC
	debug.c
	debug_supp.c
	geckousb.c
)

if(NINTENDO_GAMECUBE)
	target_sources(db PRIVATE
		tcpip.c
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
	)
endif()

target_link_libraries(db PRIVATE libogc_inc)

libogc_install_lib(db)
