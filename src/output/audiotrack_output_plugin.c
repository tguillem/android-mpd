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
#include "audiotrack_output_plugin.h"
#include "android_audio.h"
#include "output_api.h"
#include "mixer_list.h"

#include <dlfcn.h>
#include <pthread.h>
#include <glib.h>


struct audio_track {
	int valid;
	uint8_t obj[];
};

struct audiotrack_output {
	struct audio_output base;
	bool sync;
	struct audio_track *track;
	int volume;
	int volume_new;
	struct audiotrack_mixer *mixer;
	pthread_mutex_t mixer_mtx;
};

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

#define NO_ERROR 0

typedef int (*AudioTrack_getMinFrameCount)(int *, int, unsigned int);
static const char *SYM_AudioTrack_getMinFrameCount[] = {
	"_ZN7android10AudioTrack16getMinFrameCountEPj19audio_stream_type_tj",
	"_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj",
	"_ZN7android10AudioTrack16getMinFrameCountEPiij",
	NULL
};

typedef void (*AudioTrack_constructor)(void *, int, uint32_t, int, int, int, int, void (*)(int, void *, void *), void *, int, int);
static const char *SYM_AudioTrack_constructor[] = {
	"_ZN7android10AudioTrackC1E19audio_stream_type_tj14audio_format_tji20audio_output_flags_tPFviPvS4_ES4_ii",
	"_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii",
	NULL
};

typedef void (*AudioTrack_destructor)(void *);
static const char *SYM_AudioTrack_destructor[] = {
	"_ZN7android10AudioTrackD1Ev",
	NULL
};

typedef void (*AudioTrack_start)(void *);
static const char *SYM_AudioTrack_start[] = {
	"_ZN7android10AudioTrack5startEv",
	NULL
};

typedef void (*AudioTrack_stop)(void *);
static const char *SYM_AudioTrack_stop[] = {
	"_ZN7android10AudioTrack4stopEv",
	NULL
};

typedef bool (*AudioTrack_stopped)(void *);
static const char *SYM_AudioTrack_stopped[] = {
	"_ZNK7android10AudioTrack7stoppedEv",
	NULL
};

typedef int (*AudioTrack_write)(void *, void  const*, unsigned int);
static const char *SYM_AudioTrack_write[] = {
	"_ZN7android10AudioTrack5writeEPKvj",
	NULL
};

typedef void (*AudioTrack_flush)(void *);
static const char *SYM_AudioTrack_flush[] = {
	"_ZN7android10AudioTrack5flushEv",
	NULL
};

typedef int (*AudioTrack_initCheck)(void *);
static const char *SYM_AudioTrack_initCheck[] = {
	"_ZNK7android10AudioTrack9initCheckEv",
	NULL
};

typedef int (*AudioTrack_setVolume)(void *, float, float);
static const char *SYM_AudioTrack_setVolume[] = {
	"_ZN7android10AudioTrack9setVolumeEff",
	NULL,
};

static struct {
	pthread_mutex_t	lock;
	void *handle;
	int valid;
	AudioTrack_getMinFrameCount getMinFrameCount;
	AudioTrack_constructor constructor;
	AudioTrack_destructor destructor;
	AudioTrack_start start;
	AudioTrack_stop stop;
	AudioTrack_stopped stopped;
	AudioTrack_write write;
	AudioTrack_flush flush;
	AudioTrack_initCheck initCheck;
	AudioTrack_setVolume setVolume;
} symbols = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.handle = NULL,
	.valid = -1,
};

static inline GQuark
audiotrack_quark(void)
{
        return g_quark_from_static_string("audiotrack_output");
}

static int symbols_init()
{
	int ret;

	pthread_mutex_lock(&symbols.lock);

	if (symbols.valid != -1)
		goto err;

	symbols.handle = dlopen("libmedia.so", RTLD_NOW);
	if (!symbols.handle) {
		g_message("symbols_init: dlopen failed: libmedia.so\n");
		symbols.valid = 0;
		goto err;
	}

#define CRITICAL 1
#define NOT_CRITICAL 0
#define DLSYM(class, method, critical) \
	do { \
            const char **p;\
            for (p = SYM_##class##_##method; *p != NULL; ++p) { \
                symbols.method = (class##_##method) dlsym(RTLD_DEFAULT, *p); \
                if (symbols.method) { \
                    break; \
                } \
            } \
            if (!symbols.method && critical == CRITICAL) { \
		g_message("symbols_init: dlsym failed: %s\n", #method); \
		symbols.valid = 0; \
		goto err; \
            } \
	} while (0)


	DLSYM(AudioTrack, getMinFrameCount, CRITICAL);
	DLSYM(AudioTrack, constructor, CRITICAL);
	DLSYM(AudioTrack, destructor, CRITICAL);
	DLSYM(AudioTrack, start, CRITICAL);
	DLSYM(AudioTrack, stop, CRITICAL);
	DLSYM(AudioTrack, stopped, CRITICAL);
	DLSYM(AudioTrack, write, CRITICAL);
	DLSYM(AudioTrack, flush, CRITICAL);
	DLSYM(AudioTrack, initCheck, NOT_CRITICAL);
	DLSYM(AudioTrack, setVolume, NOT_CRITICAL);
#undef DLSYM
#undef CRITICAL
#undef NOT_CRITICAL

	symbols.valid = 1;
err:
	ret = symbols.valid == 1;
	pthread_mutex_unlock(&symbols.lock);
	return ret;
}

static void
audiotrack_obj_new(struct audio_track *track, int streamtype,
		    unsigned int samplerate, int format, int channelmask,
		    int framecount, unsigned int flags)
{
	symbols.constructor(track->obj, streamtype, samplerate, format, channelmask,
		    framecount, flags, NULL, NULL, 0, 0);
	if (symbols.initCheck && symbols.initCheck(track->obj) != 0) {
		symbols.destructor(track->obj);
		track->valid = 0;
	} else {
		track->valid = 1;
	}
}

static void
audiotrack_obj_delete(struct audio_track *track)
{
	symbols.destructor(track->obj);
	track->valid = 0;
}

static struct audio_output *
audiotrack_init(const struct config_param *param, GError **error)
{
	struct audiotrack_output *ao;
	struct audio_track *track;

	if (symbols_init() != 1)
		return NULL;

	ao = g_new0(struct audiotrack_output, 1);
	if (!ao)
		return NULL;

	if (!ao_base_init(&ao->base, &audiotrack_output_plugin, param, error)) {
		g_free(ao);
		return NULL;
	}

	ao->sync = config_get_block_bool(param, "sync", true);

	ao->track = (struct audio_track *)calloc(1, sizeof(struct audio_track) + AUDIOTRACK_SIZE);
	if (!ao->track) {
		g_set_error(error, audiotrack_quark(), 0, "calloc failed");
		g_free(ao);
		return NULL;
	}

	ao->volume_new = -1;
	ao->volume = 100;
	pthread_mutex_init(&ao->mixer_mtx, NULL);
	return &ao->base;
}

static void
audiotrack_finish(struct audio_output *audio_output)
{
	struct audiotrack_output *ao = (struct audiotrack_output *)audio_output;

	if (ao->track) {
		if (ao->track->valid)
			audiotrack_obj_delete(ao->track);
		free(ao->track);
	}
	pthread_mutex_destroy(&ao->mixer_mtx);
	g_free(ao);
}

static bool
audiotrack_open(struct audio_output *audio_output, struct audio_format *audio_format,
	     GError **error)
{
	struct audiotrack_output *ao = (struct audiotrack_output *)audio_output;
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
	case SAMPLE_FORMAT_S32:
		format = AUDIO_FORMAT_PCM_32_BIT;
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

	status = symbols.getMinFrameCount(&framecount, AUDIO_STREAM_MUSIC,
					     audio_format->sample_rate);
	if (status != NO_ERROR || framecount <= 0) {
		g_set_error(error, audiotrack_quark(), 0,
			    "cannot compute frame count");
                return false;
        }

	if (ao->track->valid)
		audiotrack_obj_delete(ao->track);

	audiotrack_obj_new(ao->track, AUDIO_STREAM_MUSIC,
			   audio_format->sample_rate, format, chanmask,
			   framecount, AUDIO_OUTPUT_FLAG_NONE);

	if (!ao->track->valid) {
		g_set_error(error, audiotrack_quark(), 0, "audiotrack_obj_new failed");
		return false;
        }
	if (ao->mixer)
		ao->volume_new = ao->volume;
	return true;
}

static void
audiotrack_close(struct audio_output *audio_output)
{
	struct audiotrack_output *ao = (struct audiotrack_output *)audio_output;

	if (ao->track->valid)
		audiotrack_obj_delete(ao->track);
}

static size_t
audiotrack_play(struct audio_output *audio_output, const void *chunk, size_t size,
	  GError **error)
{
	struct audiotrack_output *ao = (struct audiotrack_output *)audio_output;
	size_t ret;

	if (!ao->sync)
		return size;

	pthread_mutex_lock(&ao->mixer_mtx);

	if (ao->mixer && ao->volume_new != -1) {
		float left, right;
		left = right = ao->volume_new / 100.0;

		symbols.setVolume(ao->track->obj, left, right);

		ao->volume_new = -1;
		ao->volume = ((left + right) / 2.0 + 0.005) * 100;
	}
	pthread_mutex_unlock(&ao->mixer_mtx);

        if (symbols.stopped(ao->track->obj))
                symbols.start(ao->track->obj);
	ret = symbols.write(ao->track->obj, chunk, size);

	return ret;
}

static void
audiotrack_cancel(struct audio_output *audio_output)
{
	struct audiotrack_output *ao = (struct audiotrack_output *)audio_output;

	if (!symbols.stopped(ao->track->obj))
		symbols.stop(ao->track->obj);
        symbols.flush(ao->track->obj);
}

int
audiotrack_set_volume(struct audiotrack_output *ao, unsigned volume)
{
	pthread_mutex_lock(&ao->mixer_mtx);
	if (ao->mixer)
		ao->volume_new = volume;
	pthread_mutex_unlock(&ao->mixer_mtx);

	return 0;
}

int
audiotrack_get_volume(struct audiotrack_output *ao)
{
	int ret = -1;

	pthread_mutex_lock(&ao->mixer_mtx);
	if (ao->mixer)
		ret = ao->volume_new != -1 ? ao->volume_new : ao->volume;
	pthread_mutex_unlock(&ao->mixer_mtx);

	return ret;
}

int
audiotrack_set_mixer(struct audiotrack_output *ao, struct audiotrack_mixer *am)
{
	pthread_mutex_lock(&ao->mixer_mtx);
	if (symbols.setVolume) {
		ao->mixer = am;
		ao->volume_new = ao->volume;
	}
	pthread_mutex_unlock(&ao->mixer_mtx);
	return 0;
}

const struct audio_output_plugin audiotrack_output_plugin = {
	.name = "audiotrack",
	.init = audiotrack_init,
	.finish = audiotrack_finish,
	.open = audiotrack_open,
	.close = audiotrack_close,
	.play = audiotrack_play,
	.cancel = audiotrack_cancel,
	.mixer_plugin = &audiotrack_mixer_plugin,
};
