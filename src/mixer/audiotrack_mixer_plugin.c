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
#include "mixer_api.h"
#include "output/audiotrack_output_plugin.h"
#include "output_internal.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "audiotrack_mixer"

struct audiotrack_mixer {
	struct mixer base;
	struct audiotrack_output *output;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
audiotrack_mixer_quark(void)
{
	return g_quark_from_static_string("audiotrack_mixer");
}

static struct mixer *
audiotrack_mixer_init(void *data, G_GNUC_UNUSED const struct config_param *param,
		 GError **error_r)
{
	struct audiotrack_mixer *am;
	struct audiotrack_output *ao = data;

	if (ao == NULL) {
		g_set_error(error_r, audiotrack_mixer_quark(), 0,
			    "The android mixer cannot work without the audio output");
		return false;
	}

	am = g_new(struct audiotrack_mixer,1);
	mixer_init(&am->base, &audiotrack_mixer_plugin);

	am->output = ao;

	audiotrack_set_mixer(am->output, am);

	return &am->base;
}

static void
audiotrack_mixer_finish(struct mixer *data)
{
	struct audiotrack_mixer *am = (struct audiotrack_mixer *) data;

	g_free(am);
}

static int
audiotrack_mixer_get_volume(struct mixer *mixer, G_GNUC_UNUSED GError **error)
{
	struct audiotrack_mixer *am = (struct audiotrack_mixer *) mixer;

	return audiotrack_get_volume(am->output);
}

static bool
audiotrack_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error)
{
	struct audiotrack_mixer *am = (struct audiotrack_mixer *) mixer;

	return audiotrack_set_volume(am->output, volume) == 0 ? true : false;
}

const struct mixer_plugin audiotrack_mixer_plugin = {
	.init = audiotrack_mixer_init,
	.finish = audiotrack_mixer_finish,
	.get_volume = audiotrack_mixer_get_volume,
	.set_volume = audiotrack_mixer_set_volume,
};
