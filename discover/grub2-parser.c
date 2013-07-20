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
#include "resource.h"

struct grub2_root {
	char *uuid;
};

struct grub2_state {
	struct discover_boot_option *opt;
	int default_idx;
	int cur_idx;
	char *desc_image;
	char *desc_initrd;
	const char *const *known_names;
	struct grub2_root *root;
};

struct grub2_resource_info {
	struct grub2_root *root;
	char *path;
};

/* we use slightly different resources for grub2 */
static struct resource *create_grub2_resource(void *ctx,
		struct discover_device *orig_device,
		struct grub2_root *root, const char *path)
{
	struct grub2_resource_info *info;
	struct resource *res;

	res = talloc(ctx, struct resource);

	if (root) {
		info = talloc(res, struct grub2_resource_info);
		info->root = root;
		talloc_reference(info, root);
		info->path = talloc_strdup(info, path);

		res->resolved = false;
		res->info = info;

	} else
		resolve_resource_against_device(res, orig_device, path);

	return res;
}

static bool resolve_grub2_resource(struct device_handler *handler,
		struct resource *res)
{
	struct grub2_resource_info *info = res->info;
	struct discover_device *dev;

	assert(!res->resolved);

	dev = device_lookup_by_uuid(handler, info->root->uuid);

	if (!dev)
		return false;

	resolve_resource_against_device(res, dev, info->path);
	talloc_free(info);

	return true;
}

static bool current_option_is_default(struct grub2_state *state)
{
	if (state->default_idx < 0)
		return false;
	return state->cur_idx == state->default_idx;
}

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

	state->opt->option->is_default = current_option_is_default(state);

	discover_context_add_boot_option(conf->dc, state->opt);

	state->opt = NULL;
	state->cur_idx++;
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
		/* complete any existing option... */
		if (state->opt)
			grub2_finish(conf);

		/* ... then start the new one */
		opt = discover_boot_option_create(conf->dc, conf->dc->device);
		opt->option->boot_args = talloc_strdup(opt->option, "");

		value = strtok(value, "\'{\"");

		opt->option->id = talloc_asprintf(opt->option,
					"%s#%s", dev->id, value);
		opt->option->name = talloc_strdup(opt->option, value);
		opt->option->boot_args = talloc_strdup(opt, "");

		state->opt = opt;

		return;
	}

	if (streq(name, "linux") || streq(name, "linux16")) {
		char *sep;

		sep = strchr(value, ' ');

		if (sep)
			*sep = 0;

		opt->boot_image = create_grub2_resource(opt, conf->dc->device,
					state->root, value);
		state->desc_image = talloc_strdup(opt, value);

		if (sep)
			opt->option->boot_args = talloc_strdup(opt, sep + 1);

		return;
	}

	if (streq(name, "initrd")) {
		opt->initrd = create_grub2_resource(opt, conf->dc->device,
					state->root, value);
		state->desc_initrd = talloc_asprintf(state, "initrd=%s",
			value);
		return;
	}

	if (streq(name, "search")) {
		struct grub2_root *root;
		char *uuid;

		if (!strstr(value, "--set=root")) {
			pb_log("%s: no root\n", __func__);
			return;
		}

		/* The UUID should be the last argument to the search command.
		 * FIXME: this is a little fragile; would be nice to have some
		 * parser helpers to deal with "command args" parsing
		 */
		uuid = strrchr(value, ' ');
		if (!uuid)
			return;

		uuid++;

		if (state->root)
			talloc_unlink(state, state->root);

		root = talloc(state, struct grub2_root);
		root->uuid = talloc_strdup(root, uuid);
		state->root = root;
		return;
	}

	if (streq(name, "set")) {
		char *sep, *var_name, *var_value;

		/* this is pretty nasty, but works until we implement a proper
		 * parser... */

		sep = strchr(value, '=');
		if (!sep)
			return;

		*sep = '\0';

		var_name = value;
		var_value = sep + 1;
		if (var_value[0] == '"' || var_value[0] == '\'')
			var_value++;

		if (!strlen(var_name) || !strlen(var_value))
			return;

		if (streq(var_name, "default"))
			state->default_idx = atoi(var_value);

		return;
	}

	pb_log("%s: unknown name: %s\n", __func__, name);
}

static const char *const grub2_conf_files[] = {
	"/grub.cfg",
	"/menu.lst",
	"/grub/grub.cfg",
	"/grub2/grub.cfg",
	"/grub/menu.lst",
	"/boot/grub/grub.cfg",
	"/boot/grub2/grub.cfg",
	"/boot/grub/menu.lst",
	"/GRUB.CFG",
	"/MENU.LST",
	"/GRUB/GRUB.CFG",
	"/GRUB2/GRUB.CFG",
	"/GRUB/MENU.LST",
	"/BOOT/GRUB/GRUB.CFG",
	"/BOOT/GRUB/MENU.LST",
	NULL
};

static const char *grub2_known_names[] = {
	"menuentry",
	"linux",
	"linux16",
	"initrd",
	"search",
	"set",
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
	state->default_idx = -1;

	conf_parse_buf(conf, buf, len);

	talloc_free(conf);
	return 1;
}

static struct parser grub2_parser = {
	.name			= "grub2",
	.method			= CONF_METHOD_LOCAL_FILE,
	.parse			= grub2_parse,
	.filenames		= grub2_conf_files,
	.resolve_resource	= resolve_grub2_resource,
};

register_parser(grub2_parser);
