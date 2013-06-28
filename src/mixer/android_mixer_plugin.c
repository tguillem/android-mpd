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
#include "output/android_output_plugin.h"
#include "output_internal.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "android_mixer"

struct android_mixer {
	struct mixer base;
	struct android_output *output;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
android_mixer_quark(void)
{
	return g_quark_from_static_string("android_mixer");
}

static struct mixer *
android_mixer_init(void *data, G_GNUC_UNUSED const struct config_param *param,
		 GError **error_r)
{
	struct android_mixer *am;
	struct android_output *ao = data;

	if (ao == NULL) {
		g_set_error(error_r, android_mixer_quark(), 0,
			    "The android mixer cannot work without the audio output");
		return false;
	}

	am = g_new(struct android_mixer,1);
	mixer_init(&am->base, &android_mixer_plugin);

	am->output = ao;

	return &am->base;
}

static void
android_mixer_finish(struct mixer *data)
{
	struct android_mixer *am = (struct android_mixer *) data;

	g_free(am);
}

static int
android_mixer_get_volume(struct mixer *mixer, G_GNUC_UNUSED GError **error)
{
	struct android_mixer *am = (struct android_mixer *) mixer;

	return android_get_volume(am->output);
}

static bool
android_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error)
{
	struct android_mixer *am = (struct android_mixer *) mixer;

	return android_set_volume(am->output, volume) == 0 ? true : false;
}

const struct mixer_plugin android_mixer_plugin = {
	.init = android_mixer_init,
	.finish = android_mixer_finish,
	.get_volume = android_mixer_get_volume,
	.set_volume = android_mixer_set_volume,
};
