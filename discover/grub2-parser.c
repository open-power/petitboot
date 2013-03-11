/*
 *  Copyright Geoff Levand <geoff@infradead.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "types/types.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "paths.h"

struct grub2_state {
	struct boot_option *opt;
	char *desc_image;
	char *desc_initrd;
	const char *const *known_names;
};

static void grub2_finish(struct conf_context *conf)
{
	struct device *dev = conf->dc->device->device;
	struct grub2_state *state = conf->parser_info;

	if (!state->desc_image) {
		pb_log("%s: %s: no image found\n", __func__, dev->id);
		return;
	}

	assert(state->opt);
	assert(state->opt->name);
	assert(state->opt->boot_args);

	state->opt->description = talloc_asprintf(state->opt, "%s %s %s",
		state->desc_image,
		(state->desc_initrd ? state->desc_initrd : ""),
		state->opt->boot_args);

	talloc_free(state->desc_initrd);
	state->desc_initrd = NULL;

	conf_strip_str(state->opt->boot_args);
	conf_strip_str(state->opt->description);

	/* opt is persistent, so must be associated with device */

	discover_context_add_boot_option(conf->dc, state->opt);

	state->opt = talloc_zero(conf->dc, struct boot_option);
	state->opt->boot_args = talloc_strdup(state->opt, "");

	talloc_free(state->desc_image);
	state->desc_image = NULL;
}

static void grub2_process_pair(struct conf_context *conf, const char *name,
		char *value)
{
	struct device *dev = conf->dc->device->device;
	struct grub2_state *state = conf->parser_info;

	if (!name || !conf_param_in_list(state->known_names, name))
		return;

	if (streq(name, "menuentry")) {
		char *sep;

		grub2_finish(conf);

		/* Then start the new image. */

		sep = strchr(value, '\'');

		if (sep)
			*sep = 0;

		state->opt->id = talloc_asprintf(state->opt, "%s#%s",
			dev->id, value);
		state->opt->name = talloc_strdup(state->opt, value);

		return;
	}

	if (streq(name, "linux")) {
		char *sep;

		sep = strchr(value, ' ');

		if (sep)
			*sep = 0;

		state->opt->boot_image_file = resolve_path(state->opt,
			value, conf->dc->device->device_path);
		state->desc_image = talloc_strdup(state->opt, value);

		if (sep)
			state->opt->boot_args = talloc_strdup(state->opt,
				sep + 1);

		return;
	}

	if (streq(name, "initrd")) {
		state->opt->initrd_file = resolve_path(state->opt,
			value, conf->dc->device->device_path);
		state->desc_initrd = talloc_asprintf(state, "initrd=%s",
			value);
		return;
	}

	pb_log("%s: unknown name: %s\n", __func__, name);
}

static const char *const grub2_conf_files[] = {
	"/grub.cfg",
	"/menu.lst",
	"/grub/grub.cfg",
	"/grub/menu.lst",
	"/boot/grub/grub.cfg",
	"/boot/grub/menu.lst",
	"/GRUB.CFG",
	"/MENU.LST",
	"/GRUB/GRUB.CFG",
	"/GRUB/MENU.LST",
	"/BOOT/GRUB/GRUB.CFG",
	"/BOOT/GRUB/MENU.LST",
	NULL
};

static const char *grub2_known_names[] = {
	"menuentry",
	"linux",
	"initrd",
	NULL
};

static int grub2_parse(struct discover_context *dc)
{
	struct conf_context *conf;
	struct grub2_state *state;
	int rc;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		return 0;

	conf->dc = dc;
	conf_init_global_options(conf);
	conf->conf_files = grub2_conf_files,
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = grub2_process_pair;
	conf->finish = grub2_finish;
	conf->parser_info = state = talloc_zero(conf, struct grub2_state);

	state->known_names = grub2_known_names;

	/* opt is persistent, so must be associated with device */

	state->opt = talloc_zero(conf->dc->device, struct boot_option);
	state->opt->boot_args = talloc_strdup(state->opt, "");

	rc = conf_parse(conf);

	talloc_free(conf);
	return rc;
}

define_parser(grub2, grub2_parse);
