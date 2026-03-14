/*-------------------------------------------------------------

Copyright (C) 2008-2026
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)
Zarithya
Alberto Mardegan (mardy)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include "speaker.h"

#include <stdlib.h>
#include <string.h>

#define WENCMIN(a,b)		((a)>(b)?(b):(a))
#define ABS(x)				((s32)(x)>0?(s32)(x):-((s32)(x)))

static const int yamaha_indexscale[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	230, 230, 230, 230, 307, 409, 512, 614
};

static const int yamaha_difflookup[] = {
	1, 3, 5, 7, 9, 11, 13, 15,
	-1, -3, -5, -7, -9, -11, -13, -15
};

static const uint8_t s_speaker_defconf[7] = {
	0x00, 0x00, 0xD0, 0x07, 0x40, 0x0C, 0x0E
};

static inline int16_t wenc_clip_short(int a)
{
	if ((a + 32768) & ~65535) return (a >> 31) ^ 32767;
	else return a;
}

static inline int wenc_clip(int a, int amin, int amax)
{
	if (a < amin) return amin;
	else if (a > amax) return amax;
	else return a;
}

static uint8_t wencdata(WENCStatus *info, short sample)
{
	if (!info->step) {
		info->predictor = 0;
		info->step = 127;
	}

	int delta = sample - info->predictor;
	int nibble = WENCMIN(7, (ABS(delta) * 4) / info->step) + (delta < 0) * 8;

	info->predictor += (info->step * yamaha_difflookup[nibble]) / 8;
	info->predictor = wenc_clip_short(info->predictor);
	info->step = (info->step * yamaha_indexscale[nibble]) >> 8;
	info->step = wenc_clip(info->step, 127, 24576);

	return nibble;
}

void _wpad2_speaker_encode(WENCStatus *status, uint32_t flag,
                           const int16_t *pcm_samples, int num_samples, uint8_t *out)
{
	if (!(flag & WPAD_ENC_CONT)) status->step = 0;

	for (int n = (num_samples + 1) / 2; n > 0; n--) {
		uint8_t nibble;
		nibble = (wencdata(status, pcm_samples[0])) << 4;
		nibble |= (wencdata(status, pcm_samples[1]));
		*out++ = nibble;
		pcm_samples += 2;
	}
}

bool _wpad2_device_speaker_step(WpadDevice *device)
{
	if (device->speaker_enabled == device->speaker_requested) {
		/* Nothing to do */
		return false;
	}

	if (device->state == STATE_SPEAKER_DISABLING) {
		device->state = STATE_READY;
		return false;
	}

	uint8_t buf = 0x04;
	_wpad2_device_send_command(device, WM_CMD_SPEAKER_MUTE, &buf, 1);

	if (!device->speaker_requested) {
		device->state = STATE_SPEAKER_DISABLING;
		WPAD2_DEBUG("Disabling speaker for wiimote id %d", device->unid);

		buf = 0x01;
		_wpad2_device_write_data(device, WM_REG_SPEAKER_REG1, &buf, 1);

		buf = 0x00;
		_wpad2_device_write_data(device, WM_REG_SPEAKER_REG3, &buf, 1);

		buf = 0x00;
		_wpad2_device_send_command(device, WM_CMD_SPEAKER_ENABLE, &buf, 1);
		device->speaker_enabled = false;
		return true;
	}

	if (device->state == STATE_SPEAKER_FIRST) {
		WPAD2_DEBUG("Enabling speaker for wiimote id %d", device->unid);
		device->state = STATE_SPEAKER_ENABLING_1;
		buf = 0x04;
		_wpad2_device_send_command(device, WM_CMD_SPEAKER_ENABLE, &buf, 1);
	} else if (device->state == STATE_SPEAKER_ENABLING_1) {
		device->state = STATE_SPEAKER_ENABLING_2;
		buf = 0x01;
		_wpad2_device_write_data(device, WM_REG_SPEAKER_REG3, &buf, 1);
	} else if (device->state == STATE_SPEAKER_ENABLING_2) {
		device->state = STATE_SPEAKER_ENABLING_3;
		buf = 0x08;
		_wpad2_device_write_data(device, WM_REG_SPEAKER_REG1, &buf, 1);
	} else if (device->state == STATE_SPEAKER_ENABLING_3) {
		device->state = STATE_SPEAKER_ENABLING_4;
		uint8_t conf[7];
		memcpy(conf, s_speaker_defconf, 7);
		conf[4] = device->speaker_volume;
		_wpad2_device_write_data(device, WM_REG_SPEAKER_BLOCK, conf, 7);
	} else if (device->state == STATE_SPEAKER_ENABLING_4) {
		device->state = STATE_SPEAKER_ENABLING_5;
		buf = 0x01;
		_wpad2_device_write_data(device, WM_REG_SPEAKER_REG2, &buf, 1);
	} else if (device->state == STATE_SPEAKER_ENABLING_5) {
		device->state = STATE_SPEAKER_ENABLING_6;
		buf = 0x00;
		_wpad2_device_send_command(device, WM_CMD_SPEAKER_MUTE, &buf, 1);
	} else if (device->state == STATE_SPEAKER_ENABLING_6) {
		device->state = STATE_READY;
		_wpad2_device_request_status(device);
	}

	return true;
}

void _wpad2_speaker_play_part(WpadDevice *device)
{
	WPAD2_DEBUG("Playing at offset %d", device->sound->offset);
	WpadSoundInfo *sound = device->sound;

	if (sound->offset < sound->len) {
		int size = sound->len - sound->offset;
		if (size > 20) size = 20;
		_wpad2_device_stream_data(device, sound->samples + sound->offset, size);
		sound->offset += size;
	}
	if (sound->offset >= sound->len) {
		device->sound = NULL;
		free(sound);
		_wpad2_device_set_timer(device, 0);
	}
}

void _wpad2_speaker_play(WpadDevice *device, WpadSoundInfo *sound)
{
	/* If currently playing, stop */
	if (device->sound) {
		free(device->sound);
		device->sound = NULL;
		_wpad2_device_set_timer(device, 0);
	}

	if (!sound) return;

	device->sound = sound;
	_wpad2_device_set_timer(device, 6667);
}
