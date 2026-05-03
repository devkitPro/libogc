add_library(wpad2 STATIC
    classic.c
    device.c
    dynamics.c
    event.c
    guitar_hero_3.c
    ir.c
    motion_plus.c
    nunchuk.c
    speaker.c
    wii_board.c
    worker.c
    wpad.c
)

target_include_directories(wpad2 PRIVATE ../bt-embedded)
target_link_libraries(wpad2 PRIVATE libogc_inc)

libogc_install_lib(wpad2)
