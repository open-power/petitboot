#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <i18n/i18n.h>
#include <libgen.h>

#include "log/log.h"
#include "list/list.h"
#include "file/file.h"
#include "talloc/talloc.h"
#include "types/types.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "resource.h"
#include "paths.h"

struct syslinux_boot_option {
	char *label;
	char *image;
	char *append;
	char *initrd;
	struct list_item list;
};

/* by spec 16 is allowed */
#define INCLUDE_NEST_LIMIT 16

struct syslinux_options {
	struct list processed_options;
	struct syslinux_boot_option *current_option;
	int include_nest_level;
	char *cfg_dir;
};

struct conf_file_stat {
	char *name;
	struct stat stat;
	struct list_item list;
};

static const char *const syslinux_conf_files[] = {
	"/boot/syslinux/syslinux.cfg",
	"/syslinux/syslinux.cfg",
	"/syslinux.cfg",
	"/BOOT/SYSLINUX/SYSLINUX.CFG",
	"/SYSLINUX/SYSLINUX.CFG",
	"/SYSLINUX.CFG",
	NULL
};

static const char *const syslinux_kernel_unsupported_extensions[] = {
	".0", /* eventually support PXE here? */
	".bin",
	".bs",
	".bss",
	".c32",
	".cbt",
	".com",
	".img",
	NULL
};

static const char *const syslinux_ignored_names[] = {
	"config",
	"sysapend",
	"localboot",
	"ui",
	"prompt",
	"noescape",
	"nocomplete",
	"allowoptions",
	"timeout",
	"totaltimeout",
	"ontimeout",
	"onerror",
	"serial",
	"nohalt",
	"console",
	"font",
	"kbdmap",
	"say",
	"display",
	"f1",
	"f2",
	"f3",
	"f4",
	"f5",
	"f6",
	"f7",
	"f8",
	"f9",
	"f10",
	"f11",
	"f12",
	NULL
};

static const char *const syslinux_unsupported_boot_names[] = {
	"boot",
	"bss",
	"pxe",
	"fdimage",
	"comboot",
	"com32",
	NULL
};

static struct conf_global_option syslinux_global_options[] = {
	{ .name = "default" },
	{ .name = "implicit" },
	{ .name = "append" },
	{ .name = NULL }
};


static void finish_boot_option(struct syslinux_options *state,
			       bool free_if_unused)
{
	/*
	 * in the normal this function signals a new image block which means
	 * move the current block to the list of processed items
	 * the special case is a label before an image block which we need to
	 * know whether to keep it for further processing or junk it
	 */
	if (state->current_option) {
		if (state->current_option->image) {
			list_add(&state->processed_options,
				 &state->current_option->list);
			state->current_option = NULL;
		} else if (free_if_unused) {
			talloc_free(state->current_option);
			state->current_option = NULL;
		}
	}
}

static bool start_new_option(struct syslinux_options *state)
{
	bool ret = false;

	finish_boot_option(state, false);
	if (!state->current_option)
		state->current_option = talloc_zero(state, struct syslinux_boot_option);

	if (state->current_option)
		ret = true;

	return ret;
}

static void syslinux_process_pair(struct conf_context *conf, const char *name, char *value)
{
	struct syslinux_options *state = conf->parser_info;
	char *buf, *pos, *path;
	int len, rc;

	/* ignore bare values */
	if (!name)
		return;

	if (conf_param_in_list(syslinux_ignored_names, name))
		return;

	/* a new boot entry needs to terminate any prior one */
	if (conf_param_in_list(syslinux_unsupported_boot_names, name)) {
		finish_boot_option(state, true);
		return;
	}

	if (streq(name, "label")) {
		finish_boot_option(state, true);
		state->current_option = talloc_zero(state,
					    struct syslinux_boot_option);
		if (state->current_option)
			state->current_option->label = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "linux")) {

		if (start_new_option(state)) {
			state->current_option->image = talloc_strdup(state, value);
			if (!state->current_option->image) {
				talloc_free(state->current_option);
				state->current_option = NULL;
			}
		}

		return;
	}

	if (streq(name, "kernel")) {

		if (start_new_option(state)) {
		/*
		 * by spec a linux image can not have any of these
		 * extensions, it can have no extension or anything not
		 * in this list
		 */
			pos = strrchr(value, '.');
			if (!pos ||
			!conf_param_in_list(syslinux_kernel_unsupported_extensions, pos)) {
				state->current_option->image = talloc_strdup(state, value);
				if (!state->current_option->image) {
					talloc_free(state->current_option);
					state->current_option = NULL;
				}
			} else	/* clean up any possible trailing label */
				finish_boot_option(state, true);
		}
		return;
	}



	/* APPEND can be global and/or local so need special handling */
	if (streq(name, "append")) {
		if (state->current_option) {
			/* by spec only take last if multiple APPENDs */
			if (state->current_option->append)
				talloc_free(state->current_option->append);
			state->current_option->append = talloc_strdup(state, value);
			if (!state->current_option->append) {
				talloc_free(state->current_option);
				state->current_option = NULL;
			}
		} else {
			finish_boot_option(state, true);
			conf_set_global_option(conf, name, value);
		}
		return;
	}

	/* now the general globals */
	if (conf_set_global_option(conf, name, value)) {
		finish_boot_option(state, true);
		return;
	}

	if (streq(name, "initrd")) {
		if (state->current_option) {
			state->current_option->initrd = talloc_strdup(state, value);
			if (!state->current_option->initrd) {
				talloc_free(state->current_option);
				state->current_option = NULL;
			}
		}
		return;
	}

	if (streq(name, "include")) {
		if (state->include_nest_level < INCLUDE_NEST_LIMIT) {
			state->include_nest_level++;

			/* if absolute in as-is */
			if (value[0] == '/')
				path = talloc_strdup(state, value);
			else /* otherwise relative to the root config file */
				path = join_paths(state, state->cfg_dir, value);

			rc = parser_request_file(conf->dc, conf->dc->device, path, &buf, &len);
			if (!rc) {
				conf_parse_buf(conf, buf, len);

				device_handler_status_dev_info(conf->dc->handler, conf->dc->device,
				_("Parsed nested syslinux configuration from %s"), value);
				talloc_free(buf);
			} else {
				device_handler_status_dev_info(conf->dc->handler, conf->dc->device,
				_("Failed to parse nested syslinux configuration from %s"), value);
			}

			talloc_free(path);

			state->include_nest_level--;
		} else {
			device_handler_status_dev_err(conf->dc->handler, conf->dc->device,
				_("Nested syslinux INCLUDE exceeds limit...ignored"));
		}
		return;
	}

	pb_debug("%s: unknown name: %s\n", __func__, name);
}

static void syslinux_finalize(struct conf_context *conf)
{
	struct syslinux_options *state = conf->parser_info;
	struct syslinux_boot_option *syslinux_opt, *tmp;
	struct discover_context *dc = conf->dc;
	struct discover_boot_option *d_opt;
	bool implicit_image = true;
	char *args_sigfile_default;
	const char *global_default;
	const char *global_append;
	struct boot_option *opt;
	const char *image;
	const char *label;

	/* clean up any lingering boot entries */
	finish_boot_option(state, true);

	global_append  = conf_get_global_option(conf, "append");
	global_default = conf_get_global_option(conf, "default");

	/*
	 * by spec '0' means disable
	 * note we set the default to '1' (which is by spec) in syslinux_parse
	 */
	if (conf_get_global_option(conf, "implicit"), "0")
		implicit_image = false;

	list_for_each_entry(&state->processed_options, syslinux_opt, list) {
		/* need a valid image */
		if (!syslinux_opt->image)
			continue;

		image = syslinux_opt->image;
		label = syslinux_opt->label;

		/* if implicit is disabled we must have a label */
		if (!label && !implicit_image)
			continue;

		d_opt = discover_boot_option_create(dc, dc->device);
		if (!d_opt)
			continue;
		if (!d_opt->option)
			goto fail;

		opt = d_opt->option;

		if (syslinux_opt->append) {
			/* '-' can signal do not use global APPEND */
			if (!strcmp(syslinux_opt->append, "-"))
				opt->boot_args = talloc_strdup(opt, "");
			else
				opt->boot_args = talloc_asprintf(opt, "%s %s",
								 global_append,
								 syslinux_opt->append);
		} else
			opt->boot_args = talloc_strdup(opt, global_append);

		if (!opt->boot_args)
			goto fail;

		opt->id = talloc_asprintf(opt, "%s#%s", dc->device->device->id, image);

		if (!opt->id)
			goto fail;

		if (label) {
			opt->name = talloc_strdup(opt, label);
			if (!strcmp(label, global_default))
				opt->is_default = true;

			opt->description = talloc_asprintf(opt, "(%s) %s", label, image);
		} else {
			opt->name = talloc_strdup(opt, image);
			opt->description = talloc_strdup(opt, image);
		}

		if (!opt->name)
			goto fail;

		d_opt->boot_image = create_devpath_resource(d_opt, dc->device, image);

		if(!d_opt->boot_image)
			goto fail;

		if (syslinux_opt->initrd) {
			d_opt->initrd = create_devpath_resource(d_opt, dc->device,
								syslinux_opt->initrd);
			opt->description = talloc_asprintf_append(opt->description, 
								  " initrd=%s",
								  syslinux_opt->initrd);

			if (!d_opt->initrd)
				goto fail;
		}

		opt->description = talloc_asprintf_append(opt->description,
							  " args=%s",
							  opt->boot_args);

		if (!opt->description)
			goto fail;

		args_sigfile_default = talloc_asprintf(d_opt, "%s.cmdline.sig",
						       image);

		if (!args_sigfile_default)
			goto fail;

		d_opt->args_sig_file = create_devpath_resource(d_opt, dc->device,
							       args_sigfile_default);

		if (!d_opt->args_sig_file)
			goto fail;

		talloc_free(args_sigfile_default);

		conf_strip_str(opt->boot_args);
		conf_strip_str(opt->description);

		discover_context_add_boot_option(dc, d_opt);
		d_opt = NULL;
		continue;

fail:
		talloc_free(d_opt);
	}

	list_for_each_entry_safe(&state->processed_options, syslinux_opt, tmp, list)
		talloc_free(syslinux_opt);
	list_init(&state->processed_options);
}

static int syslinux_parse(struct discover_context *dc)
{
	struct conf_file_stat *confcmp, *confdat;
	struct list processed_conf_files;
	struct syslinux_options *state;
	const char * const *filename;
	struct conf_context *conf;
	struct stat statbuf;
	char *cfg_dir;
	int len, rc;
	char *buf;

	/* Support block device boot only at present */
	if (dc->event)
		return -1;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		return -1;

	conf->dc = dc;
	conf->global_options = syslinux_global_options,
	conf_init_global_options(conf);
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = syslinux_process_pair;

	conf->parser_info = state = talloc_zero(conf, struct syslinux_options);
	list_init(&state->processed_options);

	list_init(&processed_conf_files);

	/*
	 * set the global defaults
	 * by spec 'default' defaults to 'linux' and
	 * 'implicit' defaults to '1', we also just set
	 * and empty string in 'append' to make it easier
	 * in syslinux_finish
	 */
	conf_set_global_option(conf, "default", "linux");
	conf_set_global_option(conf, "implicit", "1");
	conf_set_global_option(conf, "append", "");

	for (filename = syslinux_conf_files; *filename; filename++) {
		/*
		 * guard against duplicate entries in case-insensitive
		 * filesystems, mainly vfat boot partitions
		 */
		rc = parser_stat_path(dc, dc->device, *filename, &statbuf);
		if (rc)
			continue;

		rc = 0;

		list_for_each_entry(&processed_conf_files, confcmp, list) {
			if (confcmp->stat.st_ino == statbuf.st_ino) {
				pb_log("conf file %s is a path duplicate of %s..skipping\n",
				       *filename, confcmp->name);
				rc = 1;
				break;
			}
		}

		if (rc)
			continue;

		rc = parser_request_file(dc, dc->device, *filename, &buf, &len);
		if (rc)
			continue;

		confdat = talloc_zero(conf, struct conf_file_stat);
		confdat->stat = statbuf;
		confdat->name = talloc_strdup(confdat, *filename);
		list_add(&processed_conf_files, &confdat->list);

		/*
		 * save location of root config file for possible
		 * INCLUDE directives later
		 *
		 * dirname can overwrite so need local copy to work on
		 */
		cfg_dir = talloc_strdup(conf, *filename);
		state->cfg_dir = talloc_strdup(state, dirname(cfg_dir));
		talloc_free(cfg_dir);

		conf_parse_buf(conf, buf, len);
		device_handler_status_dev_info(dc->handler, dc->device,
				_("Parsed syslinux configuration from %s"),
				*filename);
		talloc_free(buf);

		syslinux_finalize(conf);
	}

	talloc_free(conf);
	return 0;
}

static struct parser syslinux_parser = {
	.name			= "syslinux",
	.parse			= syslinux_parse,
	.resolve_resource	= resolve_devpath_resource,
};

register_parser(syslinux_parser);
