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

static void kboot_process_pair(struct conf_context *conf, const char *name,
		char *value)
{
	const char *const *ignored_names = conf->parser_info;
	struct discover_boot_option *d_opt;
	struct boot_option *opt;
	char *pos;
	char *args;
	const char *initrd;
	const char *root;
	const char *dtb;

	/* ignore bare values */

	if (!name)
		return;

	if (conf_param_in_list(ignored_names, name))
		return;

	if (conf_set_global_option(conf, name, value))
		return;

	/* opt must be associated with dc */

	d_opt = talloc_zero(conf->dc, struct discover_boot_option);
	d_opt->device = conf->dc->device;
	opt = talloc_zero(d_opt, struct boot_option);

	if (!opt)
		return;

	opt->id = talloc_asprintf(opt, "%s#%s", conf->dc->device->device->id,
			name);
	opt->name = talloc_strdup(opt, name);
	d_opt->option = opt;

	args = talloc_strdup(opt, "");
	initrd = conf_get_global_option(conf, "initrd");
	root = conf_get_global_option(conf, "root");
	dtb = conf_get_global_option(conf, "dtb");

	pos = strchr(value, ' ');

	/* if there's no space, it's only a kernel image with no params */

	if (!pos)
		goto out_add;
	*pos = 0;

	for (pos++; pos;) {
		char *cl_name, *cl_value;

		pos = conf_get_pair_equal(conf, pos, &cl_name, &cl_value, ' ');

		if (!cl_name) {
			args = talloc_asprintf_append(args, "%s ", cl_value);
			continue;
		}

		if (streq(cl_name, "initrd")) {
			initrd = cl_value;
			continue;
		}

		if (streq(cl_name, "root")) {
			root = cl_value;
			continue;
		}

		if (streq(cl_name, "dtb")) {
			dtb = cl_value;
			continue;
		}

		args = talloc_asprintf_append(args, "%s=%s ", cl_name,
			cl_value);
	}

out_add:
	d_opt->boot_image = create_devpath_resource(d_opt,
				conf->dc->device, value);

	if (root) {
		opt->boot_args = talloc_asprintf(opt, "root=%s %s", root, args);
		talloc_free(args);
	} else
		opt->boot_args = args;

	opt->description = talloc_asprintf(opt, "%s %s", value,
			opt->boot_args);

	if (initrd) {
		d_opt->initrd = create_devpath_resource(d_opt,
				conf->dc->device, initrd);
		opt->description = talloc_asprintf_append(opt->description,
				" initrd=%s", initrd);
	}

	if (dtb) {
		d_opt->dtb = create_devpath_resource(d_opt,
				conf->dc->device, dtb);
		opt->description = talloc_asprintf_append(opt->description,
				" dtb=%s", dtb);
	}

	conf_strip_str(opt->boot_args);
	conf_strip_str(opt->description);

	discover_context_add_boot_option(conf->dc, d_opt);
}

static struct conf_global_option kboot_global_options[] = {
	{ .name = "dtb" },
	{ .name = "initrd" },
	{ .name = "root" },
	{ .name = "video" },
	{ .name = NULL }
};

static const char *const kboot_conf_files[] = {
	"/kboot.conf",
	"/kboot.cnf",
	"/etc/kboot.conf",
	"/etc/kboot.cnf",
	"/KBOOT.CONF",
	"/KBOOT.CNF",
	"/ETC/KBOOT.CONF",
	"/ETC/KBOOT.CNF",
	NULL
};

static const char *const kboot_ignored_names[] = {
	"default",
	"delay",
	"message",
	"timeout",
	NULL
};

static int kboot_parse(struct discover_context *dc, char *buf, int len)
{
	struct conf_context *conf;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		return 0;

	conf->dc = dc;
	conf->global_options = kboot_global_options,
	conf_init_global_options(conf);
	conf->get_pair = conf_get_pair_equal;
	conf->process_pair = kboot_process_pair;
	conf->parser_info = (void *)kboot_ignored_names,

	conf_parse_buf(conf, buf, len);

	talloc_free(conf);
	return 1;
}

static struct parser kboot_parser = {
	.name			= "kboot",
	.method			= CONF_METHOD_LOCAL_FILE,
	.parse			= kboot_parse,
	.filenames		= kboot_conf_files,
	.resolve_resource	= resolve_devpath_resource,
};

register_parser(kboot_parser);
