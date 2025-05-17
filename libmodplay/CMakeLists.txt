
add_library(modplay STATIC
	freqtab.c
	mixer.c
	modplay.c
	semitonetab.c
	gcmodplay.c
)

target_link_libraries(modplay PRIVATE libogc_inc)

libogc_install_lib(modplay)
