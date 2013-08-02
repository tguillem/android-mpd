/*
 * Copyright (C) 2013 Thomas Guillem
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * OpenSL ES plugin for MPD
 * Freely inspired by VLC (modules/audio_output/opensles_android.c)
 */

#include "config.h"
#include "opensles_android_output_plugin.h"
#include "output_api.h"
#include "mixer_list.h"

#include <dlfcn.h>
#include <pthread.h>
#include <glib.h>
#include <math.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

typedef SLresult (*slCreateEngine_t)(
		SLObjectItf		*pEngine,
		SLuint32		numOptions,
		const SLEngineOption	*pEngineOptions,
		SLuint32		numInterfaces,
		const SLInterfaceID	*pInterfaceIds,
		const SLboolean		*pInterfaceRequired
);

static struct {
	pthread_mutex_t		lock;
	void			*handle;
	slCreateEngine_t	slCreateEngine;
	SLInterfaceID		SL_IID_ENGINE;
	SLInterfaceID		SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
	SLInterfaceID		SL_IID_PLAY;
	SLInterfaceID           SL_IID_VOLUME;
} symbols = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.handle = NULL
};

struct opensles_android_output {
	struct audio_output base;

	int volume;

	pthread_mutex_t	lock;
	pthread_cond_t	cond;

	bool		playing;
	uint8_t		*buf;
	size_t		buf_unit_size;
	size_t		buf_done;
	unsigned int	buf_in_idx;
	unsigned int	buf_out_idx;
	unsigned int	buf_count;
	int		rate;

	/* OpenSL */
	SLObjectItf			engineObject;
	SLEngineItf			engineEngine;
	SLObjectItf			outputMixObject;
	SLObjectItf			playerObject;
	SLPlayItf			playerPlay;
	SLVolumeItf			playerVolume;
	SLAndroidSimpleBufferQueueItf	playerBufferQueue;
	SLmillibel			maxVolumeLevel;
};

#define OPENSLES_BUFNB 32 /* maximum number of buffers */
#define OPENSLES_BUFLATENCY 20  /* buffer latency, in ms */

#define RESULT_CHECK(msg) do { \
	if (result != SL_RESULT_SUCCESS) { \
		g_set_error(error, opensles_android_quark(), 0, \
			    msg": (%lu)", result); \
		goto err; \
	} \
} while(0)

#define SYM_slCreateEngine "slCreateEngine"

#define SYM_SL_IID_ANDROIDSIMPLEBUFFERQUEUE "SL_IID_ANDROIDSIMPLEBUFFERQUEUE"
#define SYM_SL_IID_ENGINE "SL_IID_ENGINE"
#define SYM_SL_IID_PLAY "SL_IID_PLAY"
#define SYM_SL_IID_VOLUME "SL_IID_VOLUME"

#define SLDestroy(a) (*a)->Destroy(a);
#define SLSetPlayState(a, b) (*a)->SetPlayState(a, b)
#define SLRegisterCallback(a, b, c) (*a)->RegisterCallback(a, b, c)
#define SLGetInterface(a, b, c) (*a)->GetInterface(a, b, c)
#define SLRealize(a, b) (*a)->Realize(a, b)
#define SLCreateOutputMix(a, b, c, d, e) (*a)->CreateOutputMix(a, b, c, d, e)
#define SLCreateAudioPlayer(a, b, c, d, e, f, g) (*a)->CreateAudioPlayer(a, b, c, d, e, f, g)
#define SLEnqueue(a, b, c) (*a)->Enqueue(a, b, c)
#define SLClear(a) (*a)->Clear(a)
#define SLGetState(a, b) (*a)->GetState(a, b)
#define SLSetVolumeLevel(a, b) (*a)->SetVolumeLevel(a, b)
#define SLGetMaxVolumeLevel(a, b) (*a)->GetMaxVolumeLevel(a, b)

int opensles_android_set_volume(struct opensles_android_output *ao, unsigned volume);

static inline GQuark
opensles_android_quark(void)
{
        return g_quark_from_static_string("opensles_android_output");
}

static bool symbols_init(GError **error)
{
	bool ret;
	void *handle;

	pthread_mutex_lock(&symbols.lock);
	if (symbols.handle)
		goto err;

	handle = dlopen("libOpenSLES.so", RTLD_NOW);
	if (!handle) {
		g_set_error(error, opensles_android_quark(), 0,
			    "libOpenSLES.so not found");
		goto err;
	}
	symbols.slCreateEngine = dlsym(handle, SYM_slCreateEngine);
	if (!symbols.slCreateEngine) {
		g_set_error(error, opensles_android_quark(), 0,
			    "dlsym failed: slCreateEngine");
		goto err;
	}
#define DLSYM_IID(name) \
	do { \
		const SLInterfaceID *sym = dlsym(handle, SYM_##name); \
		if (!sym) { \
			g_set_error(error, opensles_android_quark(), 0, \
				    "dlsym failed: %s", #name); \
			goto err; \
		} \
		symbols.name = *sym; \
} while (0)
	DLSYM_IID(SL_IID_ENGINE);
	DLSYM_IID(SL_IID_ANDROIDSIMPLEBUFFERQUEUE);
	DLSYM_IID(SL_IID_PLAY);
	DLSYM_IID(SL_IID_VOLUME);
#undef DLSYM
	symbols.handle = handle;
err:
	ret = symbols.handle ? true : false;
	pthread_mutex_unlock(&symbols.lock);
	return ret;
}

static void
opensles_android_finish(struct audio_output *audio_output)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;

	if (ao->outputMixObject)	{
		SLDestroy(ao->outputMixObject);
		ao->outputMixObject = NULL;
	}
	if (ao->engineObject) {
		SLDestroy(ao->engineObject);
		ao->engineObject = NULL;
	}

	pthread_mutex_destroy(&ao->lock);
	g_free(ao);
}

static struct audio_output *
opensles_android_init(const struct config_param *param, GError **error)
{
	struct opensles_android_output *ao;
	SLresult result;
	
	if (!symbols_init(error))
		return NULL;

	ao = g_new0(struct opensles_android_output, 1);
	if (!ao)
		return NULL;

	if (!ao_base_init(&ao->base, &opensles_android_output_plugin, param, error)) {
		g_free(ao);
		return NULL;
	}

	pthread_mutex_init(&ao->lock, NULL);
	pthread_cond_init(&ao->cond, NULL);
	ao->volume = 100;

	result = symbols.slCreateEngine(&ao->engineObject, 0, NULL, 0, NULL, NULL);
	RESULT_CHECK("Failed to create engine");

	result = SLRealize(ao->engineObject, SL_BOOLEAN_FALSE);
	RESULT_CHECK("Failed to realize engine");

	result = SLGetInterface(ao->engineObject, symbols.SL_IID_ENGINE, &ao->engineEngine);
	RESULT_CHECK("Failed to get the engine interface");

	result = SLCreateOutputMix(ao->engineEngine, &ao->outputMixObject, 0, NULL, NULL);
	RESULT_CHECK("Failed to create output mix");

	result = SLRealize(ao->outputMixObject, SL_BOOLEAN_FALSE);
	RESULT_CHECK("Failed to realize output mix");

	return &ao->base;
err:
	if (ao)
		opensles_android_finish(&ao->base);
	return NULL;
}

static void
androidSimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
	struct opensles_android_output *ao = pContext;

	pthread_mutex_lock(&ao->lock);
	--ao->buf_count;
	pthread_cond_signal(&ao->cond);
	pthread_mutex_unlock(&ao->lock);
}

static bool
opensles_android_open(struct audio_output *audio_output, struct audio_format *audio_format,
	     GError **error)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;
	int bytes_per_sample;
	SLresult result;
	SLuint16 bitsPerSample;

	switch (audio_format->format) {
	case SAMPLE_FORMAT_S8:
		bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_8;
		bytes_per_sample = audio_format->channels;
		break;
	case SAMPLE_FORMAT_S16:
		bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
		bytes_per_sample = 2 * audio_format->channels;
		break;
	default:
		audio_format->format = SAMPLE_FORMAT_S16;
		bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
		bytes_per_sample = 2 * audio_format->channels;
		break;
	}

	SLDataLocator_AndroidSimpleBufferQueue bufferQueue = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		OPENSLES_BUFNB
	};

	SLDataFormat_PCM pcm;
	pcm.formatType    = SL_DATAFORMAT_PCM;
	pcm.numChannels	  = audio_format->channels;;
	pcm.samplesPerSec = ((SLuint32) audio_format->sample_rate * 1000); // (in milliHertz) 
	pcm.bitsPerSample = bitsPerSample;
	pcm.containerSize = bitsPerSample;
	pcm.channelMask   = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	pcm.endianness    = SL_BYTEORDER_LITTLEENDIAN;
	
	SLDataSource audioSource = {&bufferQueue, &pcm};

	SLDataLocator_OutputMix locator_outputmix = {
		SL_DATALOCATOR_OUTPUTMIX,
		ao->outputMixObject
	};
	SLDataSink audioSink = {&locator_outputmix, NULL};

	const SLInterfaceID ids2[] = { symbols.SL_IID_ANDROIDSIMPLEBUFFERQUEUE, symbols.SL_IID_VOLUME };
	static const SLboolean req2[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
	result = SLCreateAudioPlayer(ao->engineEngine, &ao->playerObject, &audioSource,
					&audioSink, sizeof(ids2) / sizeof(*ids2), ids2, req2);
	RESULT_CHECK("failed to create audio player");

	result = SLRealize(ao->playerObject, SL_BOOLEAN_FALSE);
	RESULT_CHECK("failed to realize audio player");

	result = SLGetInterface(ao->playerObject, symbols.SL_IID_PLAY, &ao->playerPlay);
	RESULT_CHECK("failed to get player itf");

	result = SLGetInterface(ao->playerObject, symbols.SL_IID_VOLUME, &ao->playerVolume);
	RESULT_CHECK("failed to get volume itf");

	result = SLGetInterface(ao->playerObject, symbols.SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &ao->playerBufferQueue);
	RESULT_CHECK("failed to get buffer queue itf");

	result = SLRegisterCallback(ao->playerBufferQueue, androidSimpleBufferQueueCallback, ao);
	RESULT_CHECK("failed to register buffer queue itf");

	result = SLGetMaxVolumeLevel(ao->playerVolume, &ao->maxVolumeLevel);
	if (result != SL_RESULT_SUCCESS) // Not critical
		ao->maxVolumeLevel = 0;

	ao->buf_unit_size = OPENSLES_BUFLATENCY *
		audio_format->sample_rate / 1000 *
		bytes_per_sample;
	ao->buf = malloc(OPENSLES_BUFNB * ao->buf_unit_size);
	if (!ao->buf) {
		g_set_error(error, opensles_android_quark(), 0,
			    "buffer malloc failed");
		goto err;
	}

	opensles_android_set_volume(ao, ao->volume);

	return true;
err:
	return false;
}

static void
opensles_android_close(struct audio_output *audio_output)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;

	if (ao->playerPlay) {
		SLSetPlayState(ao->playerPlay, SL_PLAYSTATE_STOPPED);
		ao->playerPlay = NULL;
	}
	if (ao->playerBufferQueue) {
		SLClear(ao->playerBufferQueue);
		ao->playerBufferQueue = NULL;
	}
	ao->playerVolume = NULL;
	if (ao->playerObject) {
		SLDestroy(ao->playerObject);
		ao->playerObject = NULL;
	}
	if (ao->buf) {
		free(ao->buf);
		ao->buf = NULL;
	}
	ao->playing = false;
	ao->buf_count = 0;
	ao->buf_in_idx = ao->buf_out_idx = 0;
	ao->buf_unit_size = 0;
}

static unsigned
opensles_android_delay(struct audio_output *audio_output)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;

	return ao->base.pause && !ao->playing ? 1000 : 0;
}

static size_t
opensles_android_play(struct audio_output *audio_output, const void *chunk, size_t size,
	  GError **error)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;
	size_t data_done = 0;
	const uint8_t *data = chunk;
	int nb_play_buf;
	SLresult result;

	pthread_mutex_lock(&ao->lock);

	if (!ao->playing) {
		result = SLSetPlayState(ao->playerPlay, SL_PLAYSTATE_PLAYING);
		RESULT_CHECK("failed to set play state");
		ao->playing = true;
	}

	nb_play_buf = size / ao->buf_unit_size + 1;

	while (nb_play_buf >= OPENSLES_BUFNB - ao->buf_count)
		pthread_cond_wait(&ao->cond, &ao->lock);

	while (data_done < size) {
		size_t cur = size - data_done;

		if (cur > ao->buf_unit_size - ao->buf_done)
			cur = ao->buf_unit_size - ao->buf_done;

		memcpy(&ao->buf[ao->buf_unit_size * (ao->buf_in_idx % OPENSLES_BUFNB) + ao->buf_done], data + data_done, cur);
		data_done += cur;
		ao->buf_done += cur;

		if (ao->buf_done == ao->buf_unit_size) {
			++ao->buf_in_idx;
			ao->buf_done = 0;
		}
	}

	while (ao->buf_in_idx > ao->buf_out_idx) {
		result = SLEnqueue(ao->playerBufferQueue, &ao->buf[ao->buf_unit_size * (ao->buf_out_idx % OPENSLES_BUFNB)], ao->buf_unit_size);
		RESULT_CHECK("failed to enqueue buffer");

		++ao->buf_out_idx;
		++ao->buf_count;
	}

	pthread_mutex_unlock(&ao->lock);
	return size;
err:
	pthread_mutex_unlock(&ao->lock);
	return 0;
}

static void
opensles_android_cancel(struct audio_output *audio_output)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;

	pthread_mutex_lock(&ao->lock);

	SLSetPlayState(ao->playerPlay, SL_PLAYSTATE_STOPPED);
	SLClear(ao->playerBufferQueue);

	ao->playing = false;

	ao->buf_in_idx = ao->buf_out_idx = 0;
	ao->buf_count = 0;
	ao->buf_done = 0;

	pthread_mutex_unlock(&ao->lock);
}

static bool
opensles_android_pause(struct audio_output *audio_output)
{
	struct opensles_android_output *ao = (struct opensles_android_output *)audio_output;
	bool ret = true;

	pthread_mutex_lock(&ao->lock);
	if (ao->playing) {
		SLresult result = SLSetPlayState(ao->playerPlay, SL_PLAYSTATE_STOPPED);
		if (result != SL_RESULT_SUCCESS)
			ret = false;
		ao->playing = false;
	}
	pthread_mutex_unlock(&ao->lock);
	return ret;
}

int
opensles_android_set_volume(struct opensles_android_output *ao, unsigned volume)
{
	double fvol;
	SLmillibel level;
	SLresult result;

	pthread_mutex_lock(&ao->lock);

	ao->volume = volume;

	if (volume != 0) {
		fvol = volume / (double)100.f;
		fvol = fvol * fvol * fvol;
		level = lround(2000.f * log10(fvol));

		if (level < SL_MILLIBEL_MIN)
			level = SL_MILLIBEL_MIN;
		else if (level > ao->maxVolumeLevel)
			level = ao->maxVolumeLevel;
	} else {
		level = SL_MILLIBEL_MIN;
	}

	result = SLSetVolumeLevel(ao->playerVolume, level);

	pthread_mutex_unlock(&ao->lock);

	return result == SL_RESULT_SUCCESS ? 0 : -1;
}

int
opensles_android_get_volume(struct opensles_android_output *ao)
{
	int ret = -1;

	pthread_mutex_lock(&ao->lock);
	ret = ao->volume;
	pthread_mutex_unlock(&ao->lock);

	return ret;
}

const struct audio_output_plugin opensles_android_output_plugin = {
	.name = "opensles_android",
	.init = opensles_android_init,
	.finish = opensles_android_finish,
	.open = opensles_android_open,
	.close = opensles_android_close,
	.delay = opensles_android_delay,
	.play = opensles_android_play,
	.cancel = opensles_android_cancel,
	.pause = opensles_android_pause,
	.mixer_plugin = &opensles_android_mixer_plugin,
};
