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

struct yaboot_state {
	struct discover_boot_option *opt;
	const char *desc_image;
	char *desc_initrd;
	int globals_done;
	const char *const *known_names;
};

static void yaboot_finish(struct conf_context *conf)
{
	struct yaboot_state *state = conf->parser_info;
	struct device *dev = conf->dc->device->device;
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
	state->opt->option->boot_args = talloc_strdup(state->opt->option, "");
}

static void yaboot_process_pair(struct conf_context *conf, const char *name,
		char *value)
{
	struct yaboot_state *state = conf->parser_info;
	struct boot_option *opt = state->opt->option;
	struct fixed_pair {
		const char *image;
		const char *initrd;
	};
	static const struct fixed_pair suse_fp32 = {
		.image = "/suseboot/vmlinux32",
		.initrd = "/suseboot/initrd32",
	};
	static const struct fixed_pair suse_fp64 = {
		.image = "/suseboot/vmlinux64",
		.initrd = "/suseboot/initrd64",
	};
	const struct fixed_pair *suse_fp;

	/* fixup for bare values */

	if (!name)
		name = value;

	if (!state->globals_done && conf_set_global_option(conf, name, value))
		return;

	if (!conf_param_in_list(state->known_names, name))
		return;

	state->globals_done = 1;

	/* image */

	if (streq(name, "image")) {
		const char *g_boot = conf_get_global_option(conf, "boot");
		const char *g_part = conf_get_global_option(conf, "partition");

		/* First finish any previous image. */

		if (opt->boot_image_file)
			yaboot_finish(conf);

		/* Then start the new image. */

		if (g_boot && g_part) {
			char* dev = talloc_asprintf(NULL, "%s%s", g_boot,
				g_part);

			opt->boot_image_file = resolve_path(opt,
				value, dev);
			state->desc_image = talloc_asprintf(opt,
				"%s%s", dev, value);
			talloc_free(dev);
		} else if (g_boot) {
			opt->boot_image_file = resolve_path(opt,
				value, g_boot);
			state->desc_image = talloc_asprintf(opt,
				"%s%s", g_boot, value);
		} else {
			opt->boot_image_file = resolve_path(opt,
				value, conf->dc->device->device_path);
			state->desc_image = talloc_strdup(opt, value);
		}

		return;
	}

	/* Special processing for SUSE install CD. */

	if (streq(name, "image[32bit]"))
		suse_fp = &suse_fp32;
	else if (streq(name, "image[64bit]"))
		suse_fp = &suse_fp64;
	else
		suse_fp = NULL;

	if (suse_fp) {
		/* First finish any previous image. */

		if (opt->boot_image_file)
			yaboot_finish(conf);

		/* Then start the new image. */

		if (*value == '/') {
			opt->boot_image_file = resolve_path(opt,
				value, conf->dc->device->device_path);
			state->desc_image = talloc_strdup(opt, value);
		} else {
			opt->boot_image_file = resolve_path(opt,
				suse_fp->image, conf->dc->device->device_path);
			state->desc_image = talloc_strdup(opt,
				suse_fp->image);

			opt->initrd_file = resolve_path(opt,
				suse_fp->initrd, conf->dc->device->device_path);
			state->desc_initrd = talloc_asprintf(state, "initrd=%s",
				suse_fp->initrd);
		}

		return;
	}

	if (!opt->boot_image_file) {
		pb_log("%s: unknown name: %s\n", __func__, name);
		return;
	}

	/* initrd */

	if (streq(name, "initrd")) {
		const char *g_boot = conf_get_global_option(conf, "boot");
		const char *g_part = conf_get_global_option(conf, "partition");

		if (g_boot && g_part) {
			char* dev = talloc_asprintf(NULL, "%s%s", g_boot,
				g_part);

			opt->initrd_file = resolve_path(opt,
				value, dev);
			state->desc_initrd = talloc_asprintf(state,
				"initrd=%s%s", dev, value);
			talloc_free(dev);
		} else if (g_boot) {
			opt->initrd_file = resolve_path(opt,
				value, g_boot);
			state->desc_initrd = talloc_asprintf(state,
				"initrd=%s%s", g_boot, value);
		} else {
			opt->initrd_file = resolve_path(opt,
				value, conf->dc->device->device_path);
			state->desc_initrd = talloc_asprintf(state, "initrd=%s",
				value);
		}
		return;
	}

	/* label */

	if (streq(name, "label")) {
		opt->id = talloc_asprintf(opt, "%s#%s",
			conf->dc->device->device->id, value);
		opt->name = talloc_strdup(opt, value);
		return;
	}

	/* args */

	if (streq(name, "append")) {
		opt->boot_args = talloc_asprintf_append(
			opt->boot_args, "%s ", value);
		return;
	}

	if (streq(name, "initrd-size")) {
		opt->boot_args = talloc_asprintf_append(
			opt->boot_args, "ramdisk_size=%s ", value);
		return;
	}

	if (streq(name, "literal")) {
		if (*opt->boot_args) {
			pb_log("%s: literal over writes '%s'\n", __func__,
				opt->boot_args);
			talloc_free(opt->boot_args);
		}
		talloc_asprintf(opt, "%s ", value);
		return;
	}

	if (streq(name, "ramdisk")) {
		opt->boot_args = talloc_asprintf_append(
			opt->boot_args, "ramdisk=%s ", value);
		return;
	}

	if (streq(name, "read-only")) {
		opt->boot_args = talloc_asprintf_append(
			opt->boot_args, "ro ");
		return;
	}

	if (streq(name, "read-write")) {
		opt->boot_args = talloc_asprintf_append(
			opt->boot_args, "rw ");
		return;
	}

	if (streq(name, "root")) {
		opt->boot_args = talloc_asprintf_append(
			opt->boot_args, "root=%s ", value);
		return;
	}

	pb_log("%s: unknown name: %s\n", __func__, name);
}

static struct conf_global_option yaboot_global_options[] = {
	{ .name = "boot" },
	{ .name = "initrd" },
	{ .name = "partition" },
	{ .name = "video" },
	{ .name = NULL },
};

static const char *const yaboot_conf_files[] = {
	"/yaboot.conf",
	"/yaboot.cnf",
	"/etc/yaboot.conf",
	"/etc/yaboot.cnf",
	"/suseboot/yaboot.cnf",
	"/YABOOT.CONF",
	"/YABOOT.CNF",
	"/ETC/YABOOT.CONF",
	"/ETC/YABOOT.CNF",
	"/SUSEBOOT/YABOOT.CNF",
	NULL
};

static const char *yaboot_known_names[] = {
	"append",
	"image",
	"image[64bit]", /* SUSE extension */
	"image[32bit]", /* SUSE extension */
	"initrd",
	"initrd-size",
	"label",
	"literal",
	"ramdisk",
	"read-only",
	"read-write",
	"root",
	NULL
};

static int yaboot_parse(struct discover_context *dc, char *buf, int len)
{
	struct conf_context *conf;
	struct yaboot_state *state;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		return 0;

	conf->dc = dc;
	conf->global_options = yaboot_global_options,
	conf_init_global_options(conf);
	conf->get_pair = conf_get_pair_equal;
	conf->process_pair = yaboot_process_pair;
	conf->finish = yaboot_finish;
	conf->parser_info = state = talloc_zero(conf, struct yaboot_state);

	state->known_names = yaboot_known_names;

	/* opt is persistent, so must be associated with device */

	state->opt = discover_boot_option_create(conf->dc, conf->dc->device);
	state->opt->option->boot_args = talloc_strdup(state->opt->option, "");

	conf_parse_buf(conf, buf, len);

	talloc_free(conf);
	return 1;
}

struct parser __yaboot_parser = {
	.name		= "yaboot",
	.parse		= yaboot_parse,
	.filenames	= yaboot_conf_files,
};
