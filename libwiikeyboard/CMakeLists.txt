
add_library(wiikeyboard STATIC
	usbkeyboard.c
	keyboard.c
	ukbdmap.c
	wskbdutil.c
)

target_link_libraries(wiikeyboard PRIVATE libogc_inc)

libogc_install_lib(wiikeyboard)
