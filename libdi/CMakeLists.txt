
add_library(di STATIC di.c)

target_link_libraries(di PRIVATE libogc_inc)

libogc_install_lib(di)
