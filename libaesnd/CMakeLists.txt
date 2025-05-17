
add_library(aesnd STATIC aesndlib.c)

ogc_add_dsp_binary(aesnd_dsp_mixer dspcode/dspmixer.s)
dkp_add_embedded_binary_library(aesnd_dsp_mixer_embed aesnd_dsp_mixer)

target_link_libraries(aesnd PRIVATE libogc_inc aesnd_dsp_mixer_embed)

libogc_install_lib(aesnd)
