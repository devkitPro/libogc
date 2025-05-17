
add_library(asnd STATIC asndlib.c)

ogc_add_dsp_binary(asnd_dsp_mixer dsp_mixer/dsp_mixer.s)
dkp_add_embedded_binary_library(asnd_dsp_mixer_embed asnd_dsp_mixer)

target_link_libraries(asnd PRIVATE libogc_inc asnd_dsp_mixer_embed)

libogc_install_lib(asnd)
