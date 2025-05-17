
add_library(tinysmb STATIC
	des.c
	md4.c
	ntlm.c
	smb.c
	smb_devoptab.c
)

target_link_libraries(tinysmb PRIVATE libogc_inc)

libogc_install_lib(tinysmb)
