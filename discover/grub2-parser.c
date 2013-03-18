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
#include "resource.h"

struct grub2_state {
	struct discover_boot_option *opt;
	char *desc_image;
	char *desc_initrd;
	const char *const *known_names;
};

static void grub2_finish(struct conf_context *conf)
{
	struct device *dev = conf->dc->device->device;
	struct grub2_state *state = conf->parser_info;
	struct boot_option *opt;

	if (!state->desc_image) {
		pb_log("%s: %s: no image found\n", __func__, dev->id);
		return;
	}

	assert(state->opt);
	opt = state->opt->option;

	assert(opt);
	assert(opt->name);
	assert(opt->boot_args);

	opt->description = talloc_asprintf(opt, "%s %s %s",
		state->desc_image,
		(state->desc_initrd ? state->desc_initrd : ""),
		opt->boot_args);

	talloc_free(state->desc_initrd);
	state->desc_initrd = NULL;

	conf_strip_str(opt->boot_args);
	conf_strip_str(opt->description);

	/* opt is persistent, so must be associated with device */

	discover_context_add_boot_option(conf->dc, state->opt);

	state->opt = discover_boot_option_create(conf->dc, conf->dc->device);
	opt = state->opt->option;
	opt->boot_args = talloc_strdup(opt, "");

	talloc_free(state->desc_image);
	state->desc_image = NULL;
}

static void grub2_process_pair(struct conf_context *conf, const char *name,
		char *value)
{
	struct device *dev = conf->dc->device->device;
	struct grub2_state *state = conf->parser_info;
	struct discover_boot_option *opt = state->opt;

	if (!name || !conf_param_in_list(state->known_names, name))
		return;

	if (streq(name, "menuentry")) {
		char *sep;

		grub2_finish(conf);

		/* Then start the new image. */

		sep = strchr(value, '\'');

		if (sep)
			*sep = 0;

		opt->option->id = talloc_asprintf(opt->option,
					"%s#%s", dev->id, value);
		opt->option->name = talloc_strdup(opt->option, value);

		return;
	}

	if (streq(name, "linux")) {
		char *sep;

		sep = strchr(value, ' ');

		if (sep)
			*sep = 0;

		opt->boot_image = create_devpath_resource(opt,
					conf->dc->device, value);
		state->desc_image = talloc_strdup(opt, value);

		if (sep)
			opt->option->boot_args = talloc_strdup(opt, sep + 1);

		return;
	}

	if (streq(name, "initrd")) {
		opt->initrd = create_devpath_resource(opt,
					conf->dc->device, value);
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

static int grub2_parse(struct discover_context *dc, char *buf, int len)
{
	struct conf_context *conf;
	struct grub2_state *state;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		return 0;

	conf->dc = dc;
	conf_init_global_options(conf);
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = grub2_process_pair;
	conf->finish = grub2_finish;
	conf->parser_info = state = talloc_zero(conf, struct grub2_state);

	state->known_names = grub2_known_names;

	/* opt is persistent, so must be associated with device */

	state->opt = discover_boot_option_create(dc, dc->device);
	state->opt->option->boot_args = talloc_strdup(state->opt->option, "");

	conf_parse_buf(conf, buf, len);

	talloc_free(conf);
	return 1;
}

static struct parser grub2_parser = {
	.name		= "grub2",
	.parse		= grub2_parse,
	.filenames	= grub2_conf_files,
};

register_parser(grub2_parser);
