
add_library(tuxedo OBJECT
	common_crt0.S
	early_init.S
	default_stacks.c
	cache.c
	exception.c
	exception_asm.S
	interrupt.c
	interrupt_asm.S
	thread.c
	sync.c
	tick.c
	mailbox.c
)

target_include_directories(tuxedo PRIVATE ../gc)

if(WANT_STANDALONE_TUXEDO)
	add_library(tuxedo_standalone STATIC)
	set_target_properties(tuxedo_standalone PROPERTIES OUTPUT_NAME tuxedo)
	target_link_libraries(tuxedo_standalone PRIVATE tuxedo)
	libogc_install_lib(tuxedo_standalone)
endif()

install(
	FILES ${OGC_MACHINE}.specs
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
)
