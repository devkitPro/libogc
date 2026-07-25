
add_library(ogc_sockets OBJECT
	soc_init.c
	soc_util.c
	soc_socket.c
	soc_bind.c
	soc_listen.c
	soc_send.c
	soc_recv.c
	soc_connect.c
	soc_shutdown.c
	soc_accept.c
	soc_poll.c
	soc_getsockname.c
	soc_fcntl.c
	soc_getsockopt.c
	soc_setsockopt.c
	soc_gethostbyname.c
)

target_link_libraries(ogc_sockets PRIVATE libogc_inc)

if(NINTENDO_WII)
	target_sources(ogc_sockets PRIVATE
		soc_common.c
	)
	target_link_libraries(ogc PRIVATE ogc_sockets)
endif()
