cmake_minimum_required(VERSION 3.13)

set(DKP_PLATFORM_BOOTSTRAP TRUE)

project(libogc
	LANGUAGES C ASM
)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${DEVKITPRO}/libogc" CACHE PATH "" FORCE)
endif()

include(GNUInstallDirs)

set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/${CATNIP_PRESET}")

add_library(libogc_inc INTERFACE)

target_include_directories(libogc_inc INTERFACE
	${CATNIP_BUILD_DIR}
	gc
	libogc
	gc/netif
	gc/ipv4
	gc/ogc
	gc/ogc/machine
	gc/modplay
	gc/bte
	gc/sdcard
	gc/wiiuse
	gc/di
	external/bsd/include
)

target_compile_options(libogc_inc INTERFACE
	-msdata=eabi
	-Wall -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
	-fno-strict-aliasing
)

if(NINTENDO_GAMECUBE)
	target_compile_definitions(libogc_inc INTERFACE HW_DOL)
elseif(NINTENDO_WII)
	target_compile_definitions(libogc_inc INTERFACE HW_RVL)
endif()

macro(libogc_install_lib target)
	install(
		TARGETS ${target}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
endmacro()

add_subdirectory(tuxedo)
add_subdirectory(libogc)
add_subdirectory(libmodplay)
add_subdirectory(libmad)
add_subdirectory(libdb)
add_subdirectory(libtinysmb)
add_subdirectory(libasnd)
add_subdirectory(libaesnd)
add_subdirectory(libiso9660)

if(NINTENDO_GAMECUBE)
	add_subdirectory(lwip)
elseif(NINTENDO_WII)
	add_subdirectory(lwbt)
	add_subdirectory(wiiuse)
	add_subdirectory(libdi)
	add_subdirectory(libwiikeyboard)
endif()

install(
	FILES LICENSE
	DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(
	FILES ${CATNIP_BUILD_DIR}/libversion.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ogc
)

install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/gc/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp"
		PATTERN "*.inc"
)

install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/external/bsd/include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING
		PATTERN "*.h"
)
