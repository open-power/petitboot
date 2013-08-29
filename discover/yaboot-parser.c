#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "types/types.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "resource.h"

struct yaboot_state {
	int globals_done;
	const char *const *known_names;

	/* current option data */
	struct discover_boot_option *opt;
	const char *device;
	const char *partition;
	const char *boot_image;
	const char *initrd;
	const char *initrd_size;
	const char *literal;
	const char *ramdisk;
	const char *root;
	bool read_only;
	bool read_write;
};

static struct discover_boot_option *state_start_new_option(
		struct conf_context *conf,
		struct yaboot_state *state)
{
	state->opt = discover_boot_option_create(conf->dc, conf->dc->device);
	state->opt->option->boot_args = talloc_strdup(state->opt->option, "");

	/* old allocated values will get freed with the state */
	state->device = conf_get_global_option(conf, "device");
	state->partition = conf_get_global_option(conf, "partition");
	state->initrd_size = conf_get_global_option(conf, "initrd_size");
	state->literal = conf_get_global_option(conf, "literal");
	state->ramdisk = conf_get_global_option(conf, "ramdisk");
	state->root = conf_get_global_option(conf, "root");

	return state->opt;
}

static struct resource *create_yaboot_devpath_resource(
		struct yaboot_state *state,
		struct conf_context *conf,
		const char *path)
{
	struct discover_boot_option *opt = state->opt;
	const char *dev, *part, *devpos;
	struct resource *res;
	char *devpath, *devstr;

	dev = state->device;
	part = state->partition;

	if (!dev)
		dev = conf_get_global_option(conf, "device");
	if (!part)
		part = conf_get_global_option(conf, "partition");

	if (strchr(path, ':')) {
		devpath = talloc_strdup(conf, path);

	} else if (dev && part) {
		devpos = &dev[strlen(dev) - 1];
		if (isdigit(*devpos)) {
			while (isdigit(*devpos))
				devpos--;

			devstr = talloc_strndup(conf, dev, devpos - dev + 1);
			devpath = talloc_asprintf(conf, "%s%s:%s", devstr,
					part, path);
			talloc_free(devstr);
		} else {
			devpath = talloc_asprintf(conf,
					"%s%s:%s", dev, part, path);
		}
	} else if (dev) {
		devpath = talloc_asprintf(conf, "%s:%s", dev, path);
	} else {
		devpath = talloc_strdup(conf, path);
	}

	res = create_devpath_resource(opt, conf->dc->device, devpath);

	talloc_free(devpath);

	return res;
}

static void yaboot_finish(struct conf_context *conf)
{
	struct yaboot_state *state = conf->parser_info;
	const char *default_label;
	struct boot_option *opt;

	assert(state->opt);

	opt = state->opt->option;
	assert(opt);
	assert(opt->name);
	assert(opt->boot_args);

	/* populate the boot option from state data */
	state->opt->boot_image = create_yaboot_devpath_resource(state,
				conf, state->boot_image);
	if (state->initrd) {
		state->opt->initrd = create_yaboot_devpath_resource(state,
				conf, state->initrd);
	}

	if (state->initrd_size) {
		opt->boot_args = talloc_asprintf(opt, "ramdisk_size=%s %s",
					state->initrd_size, opt->boot_args);
	}

	if (state->ramdisk) {
		opt->boot_args = talloc_asprintf(opt, "ramdisk=%s %s",
					state->initrd_size, opt->boot_args);
	}

	if (state->root) {
		opt->boot_args = talloc_asprintf(opt, "root=%s %s",
					state->root, opt->boot_args);
	}

	if (state->read_only && state->read_write) {
		pb_log("boot option %s specified both 'ro' and 'rw', "
				"using 'rw'\n", opt->name);
		state->read_only = false;
	}

	if (state->read_only || state->read_write) {
		opt->boot_args = talloc_asprintf(opt, "%s %s",
					state->read_only ? "ro" : "rw",
					opt->boot_args);
	}

	if (state->literal) {
		opt->boot_args = talloc_strdup(opt, state->literal);
	}

	opt->description = talloc_asprintf(opt, "%s %s %s",
		state->boot_image,
		(state->initrd ? state->initrd : ""),
		opt->boot_args ? opt->boot_args : "");

	conf_strip_str(opt->boot_args);
	conf_strip_str(opt->description);

	default_label = conf_get_global_option(conf, "default");
	if (default_label &&
			!strcasecmp(state->opt->option->name, default_label))
		state->opt->option->is_default = true;

	discover_context_add_boot_option(conf->dc, state->opt);
}

static void yaboot_process_pair(struct conf_context *conf, const char *name,
		char *value)
{
	struct yaboot_state *state = conf->parser_info;
	struct discover_boot_option *opt = state->opt;
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

		/* First finish any previous image. */
		if (opt)
			yaboot_finish(conf);

		/* Then start the new image. */
		opt = state_start_new_option(conf, state);

		state->boot_image = talloc_strdup(state, value);

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
		if (opt)
			yaboot_finish(conf);

		/* Then start the new image. */
		opt = state_start_new_option(conf, state);

		if (*value == '/') {
			state->boot_image = talloc_strdup(state, value);
		} else {
			state->boot_image = talloc_strdup(state,
							suse_fp->image);
			state->initrd = talloc_strdup(state, suse_fp->initrd);
		}

		return;
	}

	/* all other processing requires an image */
	if (!opt) {
		pb_log("%s: unknown name: %s\n", __func__, name);
		return;
	}

	/* initrd */
	if (streq(name, "initrd")) {
		state->initrd = talloc_strdup(state, value);
		return;
	}

	/* label */
	if (streq(name, "label")) {
		opt->option->id = talloc_asprintf(opt->option, "%s#%s",
			conf->dc->device->device->id, value);
		opt->option->name = talloc_strdup(opt->option, value);
		return;
	}

	/* args */
	if (streq(name, "device")) {
		printf("option device : %s", value);
		state->device = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "parititon")) {
		state->partition = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "append")) {
		opt->option->boot_args = talloc_asprintf_append(
			opt->option->boot_args, "%s ", value);
		return;
	}

	if (streq(name, "initrd-size")) {
		state->initrd_size = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "literal")) {
		state->literal = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "ramdisk")) {
		state->ramdisk = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "read-only")) {
		state->read_only = true;
		return;
	}

	if (streq(name, "read-write")) {
		state->read_write = true;
		return;
	}

	if (streq(name, "root")) {
		state->root = talloc_strdup(state, value);
		return;
	}

	pb_log("%s: unknown name: %s\n", __func__, name);
}

static struct conf_global_option yaboot_global_options[] = {
	{ .name = "root" },
	{ .name = "device" },
	{ .name = "partition" },
	{ .name = "initrd" },
	{ .name = "initrd_size" },
	{ .name = "video" },
	{ .name = "literal" },
	{ .name = "ramdisk" },
	{ .name = "default" },
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
	"device",
	"partition",
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

	state->opt = NULL;

	conf_parse_buf(conf, buf, len);

	talloc_free(conf);
	return 1;
}

static struct parser yaboot_parser = {
	.name			= "yaboot",
	.method			= CONF_METHOD_LOCAL_FILE,
	.parse			= yaboot_parse,
	.filenames		= yaboot_conf_files,
	.resolve_resource	= resolve_devpath_resource,
};

register_parser(yaboot_parser);
