
add_library(mad STATIC
	mp3player.c
	bit.c
	decoder.c
	fixed.c
	frame.c
	huffman.c
	layer12.c
	layer3.c
	stream.c
	synth.c
	timer.c
	version.c
)

target_link_libraries(mad PRIVATE libogc_inc)

libogc_install_lib(mad)
