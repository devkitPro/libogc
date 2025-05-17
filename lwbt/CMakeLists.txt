
add_library(bte STATIC
	bte.c
	hci.c
	l2cap.c
	btmemb.c
	btmemr.c
	btpbuf.c
	physbusif.c
)

target_link_libraries(bte PRIVATE libogc_inc)

libogc_install_lib(bte)
