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

#include "config.h"
#include "android_output_plugin.h"
#include "android_audio.h"
#include "output_api.h"
#include "mixer_list.h"

#include <dlfcn.h>
#include <pthread.h>
#include <glib.h>

/*
 * Use Android AudioTrack class in C:
 * link with libmedia.so via dlopen.
 * Access all AudioTrack symbols addresses in C via dlsym.
 */

/*
 * We can't know for sure the size of an AudioTrack object since it depends on
 * class size specified by AudioTrack.h that is hardware dependant. So, alloc
 * a big size that will be always enough on all android devices.
 * Yes this is UGLY.
 */
#define AUDIOTRACK_SIZE 512

/*
 * AudioTrack symbols:
 */

#define SYM_AudioTrack_getMinFrameCount "_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj"
#define SYM_AudioTrack_getMinFrameCount_legacy "_ZN7android10AudioTrack16getMinFrameCountEPiij"
typedef int (*AudioTrack_getMinFrameCount)(int *, int, unsigned int);

#define SYM_AudioTrack_ctor "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"
typedef void (*AudioTrack_ctor)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int, int);

#define SYM_AudioTrack_dtor "_ZN7android10AudioTrackD1Ev"
typedef void (*AudioTrack_dtor)(void *);

#define SYM_AudioTrack_initCheck "_ZNK7android10AudioTrack9initCheckEv"
typedef int (*AudioTrack_initCheck)(void *);

#define SYM_AudioTrack_start "_ZN7android10AudioTrack5startEv"
typedef void (*AudioTrack_start)(void *);

#define SYM_AudioTrack_stop "_ZN7android10AudioTrack4stopEv"
typedef void (*AudioTrack_stop)(void *);

#define SYM_AudioTrack_stopped "_ZNK7android10AudioTrack7stoppedEv"
typedef bool (*AudioTrack_stopped)(void *);

#define SYM_AudioTrack_write "_ZN7android10AudioTrack5writeEPKvj"
typedef int (*AudioTrack_write)(void *, void  const*, unsigned int);

#define SYM_AudioTrack_flush "_ZN7android10AudioTrack5flushEv"
typedef void (*AudioTrack_flush)(void *);

#define SYM_AudioTrack_latency "_ZNK7android10AudioTrack7latencyEv"
typedef uint32_t (*AudioTrack_latency)(void *);

#define SYM_AudioTrack_frameCount "_ZNK7android10AudioTrack10frameCountEv"
typedef uint32_t (*AudioTrack_frameCount)(void *);

#define SYM_AudioTrack_frameSize "_ZNK7android10AudioTrack9frameSizeEv"
typedef size_t (*AudioTrack_frameSize)(void *);

#define SYM_AudioTrack_channelCount "_ZNK7android10AudioTrack12channelCountEv"
typedef int (*AudioTrack_channelCount)(void *);

#define SYM_AudioTrack_getSessionId "_ZNK7android10AudioTrack12getSessionIdEv"
#define SYM_AudioTrack_getSessionId_legacy "_ZN7android10AudioTrack12getSessionIdEv"
typedef int (*AudioTrack_getSessionId)(void *);

#define SYM_AudioTrack_setVolume "_ZN7android10AudioTrack9setVolumeEff"
typedef int (*AudioTrack_setVolume)(void *, float, float);

#define SYM_AudioTrack_getVolume "_ZNK7android10AudioTrack9getVolumeEPfS1_"
#define SYM_AudioTrack_getVolume_legacy "_ZN7android10AudioTrack9getVolumeEPfS1_"
typedef int (*AudioTrack_getVolume)(void *, float *, float *);

struct audio_track {
	void *dlhandle;
	AudioTrack_getMinFrameCount getMinFrameCount;
	AudioTrack_ctor ctor;
	AudioTrack_dtor dtor;
	AudioTrack_initCheck initCheck;
	AudioTrack_start start;
	AudioTrack_stop stop;
	AudioTrack_stopped stopped;
	AudioTrack_write write;
	AudioTrack_flush flush;
	AudioTrack_latency latency;
	AudioTrack_frameCount frameCount;
	AudioTrack_frameSize frameSize;
	AudioTrack_channelCount channelCount;
	AudioTrack_getSessionId getSessionId;
	AudioTrack_setVolume setVolume;
	AudioTrack_getVolume getVolume;
	int valid;
	uint8_t obj[];
};

struct android_output {
	struct audio_output base;
	bool sync;
	struct audio_track *track;
	int volume;
	int volume_new;
	pthread_mutex_t volume_mtx;
};

static inline GQuark
android_quark(void)
{
        return g_quark_from_static_string("android_output");
}

static void
audio_track_obj_new(struct audio_track *track, int streamtype,
		    unsigned int samplerate, int format, int channelmask,
		    int framecount, unsigned int flags)
{
	track->ctor(track->obj, streamtype, samplerate, format, channelmask,
		    framecount, flags, NULL, NULL, 0, 0);
	if (track->initCheck(track->obj) != 0) {
		track->dtor(track->obj);
		track->valid = 0;
	} else {
		track->valid = 1;
	}
}

static void
audio_track_obj_delete(struct audio_track *track)
{
	track->dtor(track->obj);
	track->valid = 0;
}

static struct audio_track *
audio_track_class_init(GError **error)
{
#define DLSYM(method, sym) track->method = (AudioTrack_##method) dlsym(track->dlhandle, sym)
#define DLSYM_CHECK(method) \
	do { \
			DLSYM(method, SYM_AudioTrack_##method); \
		        if (!track->method) { \
				g_set_error(error, android_quark(), 0, "dlsym failed: %s", #method); \
				goto err; \
			} \
	} while (0)
#define DLSYM_LEGACY(method) \
	do { \
			DLSYM(method, SYM_AudioTrack_##method); \
		        if (!track->method) { \
				DLSYM(method, SYM_AudioTrack_##method##_legacy); \
				if (!track->method) { \
					g_set_error(error, android_quark(), 0, "dlsym failed: %s", #method); \
					goto err; \
				} \
			} \
	} while (0)

	struct audio_track *track;

	track = (struct audio_track *)calloc(1, sizeof(struct audio_track) + AUDIOTRACK_SIZE);
	if (!track) {
		g_set_error(error, android_quark(), 0, "calloc failed");
		goto err;
	}
	track->dlhandle = dlopen("libmedia.so", RTLD_NOW);
	if (!track->dlhandle) {
		g_set_error(error, android_quark(), 0, "dlopen(\"libmedia.so\") failed");
		goto err;
	}

	DLSYM_LEGACY(getMinFrameCount);

	DLSYM_CHECK(ctor);
	DLSYM_CHECK(dtor);
	DLSYM_CHECK(initCheck);
	DLSYM_CHECK(start);
	DLSYM_CHECK(stop);
	DLSYM_CHECK(stopped);
	DLSYM_CHECK(write);
	DLSYM_CHECK(flush);
	DLSYM_CHECK(latency);
	DLSYM_CHECK(frameCount);
	DLSYM_CHECK(frameSize);
	DLSYM_CHECK(channelCount);

	DLSYM_LEGACY(getSessionId);
	DLSYM_CHECK(setVolume);
	DLSYM_LEGACY(getVolume);

	return track;
err:
	if (track)
		free(track);
	return NULL;
}

static void
audio_track_class_finish(struct audio_track *track)
{
	if (track->valid)
		audio_track_obj_delete(track);
	dlclose(track->dlhandle);
	free(track);
}

static struct audio_output *
android_init(const struct config_param *param, GError **error)
{
	struct android_output *ao = g_new(struct android_output, 1);

	if (!ao)
		return NULL;

	if (!ao_base_init(&ao->base, &android_output_plugin, param, error)) {
		g_free(ao);
		return NULL;
	}

	ao->sync = config_get_block_bool(param, "sync", true);

	ao->track = audio_track_class_init(error);
	if (!ao->track) {
		g_free(ao);
		return NULL;
	}
	ao->volume_new = ao->volume = 100;
	pthread_mutex_init(&ao->volume_mtx, NULL);
	return &ao->base;
}

static void
android_finish(struct audio_output *audio_output)
{
	struct android_output *ao = (struct android_output *)audio_output;

	if (ao->track)
		audio_track_class_finish(ao->track);
	pthread_mutex_destroy(&ao->volume_mtx);
	g_free(ao);
}

static bool
android_open(struct audio_output *audio_output, struct audio_format *audio_format,
	     GError **error)
{
	struct android_output *ao = (struct android_output *)audio_output;
	uint32_t chanmask;
	audio_format_t format;
	int framecount = 0, status;

	switch (audio_format->format) {
	case SAMPLE_FORMAT_S8:
		format = AUDIO_FORMAT_PCM_8_BIT;
		break;
	case SAMPLE_FORMAT_S16:
		format = AUDIO_FORMAT_PCM_16_BIT;
		break;
	default:
		audio_format->format = SAMPLE_FORMAT_S16;
		format = AUDIO_FORMAT_PCM_16_BIT;
		break;
	}

        switch (audio_format->channels) {
        case 1:
                chanmask = AUDIO_CHANNEL_OUT_MONO;
		break;
        case 2:
                chanmask = AUDIO_CHANNEL_OUT_STEREO;
		break;
        case 6:
                chanmask = AUDIO_CHANNEL_OUT_5POINT1;
		break;
        case 8:
                chanmask = AUDIO_CHANNEL_OUT_7POINT1;
		break;
	default:
		audio_format->channels = 2;
		chanmask = AUDIO_CHANNEL_OUT_STEREO;
		break;
        }

	status = ao->track->getMinFrameCount(&framecount, AUDIO_STREAM_MUSIC,
					     audio_format->sample_rate);
	if (status != NO_ERROR || framecount <= 0) {
		g_set_error(error, android_quark(), 0,
			    "cannot compute frame count");
                return false;
        }

	if (ao->track->valid)
		audio_track_obj_delete(ao->track);

	audio_track_obj_new(ao->track, AUDIO_STREAM_MUSIC,
			   audio_format->sample_rate, format, chanmask,
			   framecount, AUDIO_OUTPUT_FLAG_NONE);

	if (!ao->track->valid) {
		g_set_error(error, android_quark(), 0, "audio_track_obj_new failed");
		return false;
        }
	ao->volume_new = ao->volume;
	return true;
}

static void
android_close(struct audio_output *audio_output)
{
	struct android_output *ao = (struct android_output *)audio_output;

	if (ao->track->valid)
		audio_track_obj_delete(ao->track);
}

static size_t
android_play(struct audio_output *audio_output, const void *chunk, size_t size,
	  GError **error)
{
	struct android_output *ao = (struct android_output *)audio_output;
	size_t ret;

	if (!ao->sync)
		return size;

	pthread_mutex_lock(&ao->volume_mtx);

	if (ao->volume_new != -1) {
		float left, right;
		left = right = ao->volume_new / 100.0;

		ao->track->setVolume(ao->track->obj, left, right);
		ao->track->getVolume(ao->track->obj, &left, &right);

		ao->volume_new = -1;
		ao->volume = ((left + right) / 2.0 + 0.005) * 100;
	}
	pthread_mutex_unlock(&ao->volume_mtx);

        if (ao->track->stopped(ao->track->obj))
                ao->track->start(ao->track->obj);
	ret = ao->track->write(ao->track->obj, chunk, size);

	return ret;
}

static void
android_cancel(struct audio_output *audio_output)
{
	struct android_output *ao = (struct android_output *)audio_output;

	if (!ao->track->stopped(ao->track->obj))
		ao->track->stop(ao->track->obj);
        ao->track->flush(ao->track->obj);
}

int
android_set_volume(struct android_output *ao, unsigned volume)
{
	pthread_mutex_lock(&ao->volume_mtx);
	ao->volume_new = volume;
	pthread_mutex_unlock(&ao->volume_mtx);

	return 0;
}

int
android_get_volume(struct android_output *ao)
{
	int ret = -1;

	pthread_mutex_lock(&ao->volume_mtx);
	ret = ao->volume_new != -1 ? ao->volume_new : ao->volume;
	pthread_mutex_unlock(&ao->volume_mtx);

	return ret;
}

const struct audio_output_plugin android_output_plugin = {
	.name = "android",
	.init = android_init,
	.finish = android_finish,
	.open = android_open,
	.close = android_close,
	.play = android_play,
	.cancel = android_cancel,
	.mixer_plugin = &android_mixer_plugin,
};
