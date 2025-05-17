
add_library(bba STATIC
	network.c
	netio.c
	arch/gc/netif/gcif.c
	core/inet.c
	core/mem.c
	core/dhcp.c
	core/raw.c
	core/memp.c
	core/netif.c
	core/pbuf.c
	core/stats.c
	core/sys.c
	core/tcp.c
	core/tcp_in.c
	core/tcp_out.c
	core/udp.c
	core/ipv4/icmp.c
	core/ipv4/ip.c
	core/ipv4/ip_frag.c
	core/ipv4/ip_addr.c
	netif/etharp.c
	netif/loopif.c
)

target_link_libraries(bba PRIVATE libogc_inc)

libogc_install_lib(bba)
