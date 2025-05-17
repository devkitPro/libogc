
add_library(iso9660 STATIC iso9660.c)

target_link_libraries(iso9660 PRIVATE libogc_inc)

libogc_install_lib(iso9660)
