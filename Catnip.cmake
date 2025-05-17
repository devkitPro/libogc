catnip_package(libogc DEFAULT cube wii)

if(DEFINED ENV{LIBOGC_VER})
	set(LIBOGC_VER "$ENV{LIBOGC_VER}")
else()
	find_package(Git QUIET REQUIRED)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tags
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		OUTPUT_VARIABLE LIBOGC_VER
		OUTPUT_STRIP_TRAILING_WHITESPACE
		COMMAND_ERROR_IS_FATAL ANY
	)
endif()

if(LIBOGC_VER MATCHES "^v([0-9]+)\.")
	set(LIBOGC_MAJOR "${CMAKE_MATCH_1}")
endif()

if(LIBOGC_VER MATCHES "^v[0-9]+\.([0-9]+)")
	set(LIBOGC_MINOR "${CMAKE_MATCH_1}")
endif()

if(LIBOGC_VER MATCHES "^v[0-9]+\.[0-9]+\.([0-9]+)")
	set(LIBOGC_PATCH "${CMAKE_MATCH_1}")
endif()

configure_file(libversion.h.in "${CATNIP_BUILD_DIR}/libversion.h" @ONLY)

catnip_add_preset(cube TOOLSET GameCube BUILD_TYPE Release)
catnip_add_preset(wii  TOOLSET Wii      BUILD_TYPE Release)
