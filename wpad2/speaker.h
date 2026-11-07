#ifndef WPAD2_SPEAKER_H
#define WPAD2_SPEAKER_H

#include "device.h"

typedef struct {
	s32 predictor;
	s16 step_index;
	s32 step;
	s32 prev_sample;
	s16 sample1;
	s16 sample2;
	s32 coeff1;
	s32 coeff2;
	s32 idelta;
} WENCStatus;

void _wpad2_speaker_encode(WENCStatus *info, uint32_t flag,
                           const int16_t *pcm_samples, int num_samples, uint8_t *out);
bool _wpad2_device_speaker_step(WpadDevice *device);

void _wpad2_speaker_play_part(WpadDevice *device);
void _wpad2_speaker_play(WpadDevice *device, WpadSoundInfo *sound);

#endif /* WPAD2_SPEAKER_H */
