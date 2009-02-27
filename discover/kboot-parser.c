#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <talloc/talloc.h>
#include <log/log.h>

#include "pb-protocol/pb-protocol.h"
#include "paths.h"
#include "params.h"
#include "parser-utils.h"
#include "device-handler.h"

#define buf_size 1024

struct kboot_context {
	struct discover_context *discover;

	char *buf;

	struct global_option {
		char *name;
		char *value;
	} *global_options;
	int n_global_options;
};

static int param_is_ignored(const char *param)
{
	static const char *ignored_options[] =
		{ "message", "timeout", "default", NULL };
	const char **str;

	for (str = ignored_options; *str; str++)
		if (streq(*str, param))
			return 1;
	return 0;
}

/**
 * Splits a name=value pair, with value terminated by @term (or nul). if there
 * is no '=', then only the value is populated, and *name is set to NULL. The
 * string is modified in place.
 *
 * Returns the next byte to process, or null if we've hit the end of the
 * string.
 *
 * */
static char *get_param_pair(char *str, char **name_out, char **value_out,
		char terminator)
{
	char *sep, *tmp, *name, *value;

	/* terminate the value */
	tmp = strchr(str, terminator);
	if (tmp)
		*tmp = 0;
	else
		tmp = NULL;

	sep = strchr(str, '=');
	if (!sep) {
		*name_out = NULL;
		*value_out = str;
		return tmp ? tmp + 1 : NULL;
	}

	/* terminate the name */
	*sep = 0;

	/* remove leading spaces */
	for (name = str; isspace(*name); name++);
	for (value = sep + 1; isspace(*value); value++);

	/* .. and trailing ones.. */
	for (sep--; isspace(*sep); sep--)
		*sep = 0;
	for (sep = value + strlen(value) - 1; isspace(*sep); sep--)
		*sep = 0;

	*name_out = name;
	*value_out = value;

	return tmp ? tmp + 1 : NULL;
}

static struct global_option global_options[] = {
	{ .name = "root" },
	{ .name = "initrd" },
	{ .name = "video" },
	{ .name = NULL }
};

/*
 * Check if an option (name=value) is a global option. If so, store it in
 * the global options table, and return 1. Otherwise, return 0.
 */
static int check_for_global_option(struct kboot_context *ctx,
		const char *name, const char *value)
{
	int i;

	for (i = 0; i < ctx->n_global_options; i++) {
		if (!strcmp(name, ctx->global_options[i].name)) {
			global_options[i].value = strdup(value);
			break;
		}
	}
	return 0;
}

static char *get_global_option(
		struct kboot_context *ctx __attribute__((unused)),
		const char *name)
{
	int i;

	for (i = 0; global_options[i].name ;i++)
		if (!strcmp(name, global_options[i].name))
			return global_options[i].value;

	return NULL;
}

static int parse_option(struct kboot_context *kboot_ctx, char *opt_name,
		char *config)
{
	char *pos, *name, *value, *root, *initrd, *cmdline, *tmp;
	struct boot_option *opt;

	root = initrd = cmdline = NULL;

	/* remove quotes around the value */
	while (*config == '"' || *config == '\'')
		config++;

	pos = config + strlen(config) - 1;
	while (*pos == '"' || *pos == '\'')
		*(pos--) = 0;

	if (!strlen(pos))
		return 0;

	pos = strchr(config, ' ');

	opt = talloc_zero(kboot_ctx, struct boot_option);
	opt->id = talloc_asprintf(opt, "%s#%s",
			kboot_ctx->discover->device->id, opt_name);
	opt->name = talloc_strdup(opt, opt_name);

	/* if there's no space, it's only a kernel image with no params */
	if (!pos) {
		opt->boot_image_file = resolve_path(opt, config,
				kboot_ctx->discover->device_path);
		opt->description = talloc_strdup(opt, config);
		goto out_add;
	}

	*pos = 0;
	opt->boot_image_file = resolve_path(opt, config,
			kboot_ctx->discover->device_path);

	cmdline = talloc_array(opt, char, buf_size);
	*cmdline = 0;

	for (pos++; pos;) {
		pos = get_param_pair(pos, &name, &value, ' ');

		if (!name) {
			strcat(cmdline, " ");
			strcat(cmdline, value);

		} else if (streq(name, "initrd")) {
			initrd = value;

		} else if (streq(name, "root")) {
			root = value;

		} else {
			strcat(cmdline, " ");
			*(value - 1) = '=';
			strcat(cmdline, name);
		}
	}

	if (!root)
		root = get_global_option(kboot_ctx, "root");
	if (!initrd)
		initrd = get_global_option(kboot_ctx, "initrd");

	if (initrd) {
		tmp = talloc_asprintf(opt, "initrd=%s %s", initrd, cmdline);
		talloc_free(cmdline);
		cmdline = tmp;

		opt->initrd_file = resolve_path(opt, initrd,
				kboot_ctx->discover->device_path);
	}

	if (root) {
		tmp = talloc_asprintf(opt, "root=%s %s", root, cmdline);
		talloc_free(cmdline);
		cmdline = tmp;

	} else if (initrd) {
		/* if there's an initrd but no root, fake up /dev/ram0 */
		tmp = talloc_asprintf(opt, "root=/dev/ram0 %s", cmdline);
		talloc_free(cmdline);
		cmdline = tmp;
	}

	opt->boot_args = cmdline;

	opt->description = talloc_asprintf(opt, "%s %s",
			config, opt->boot_args);

out_add:
	device_add_boot_option(kboot_ctx->discover->device, opt);
	return 1;
}

static void parse_buf(struct kboot_context *kboot_ctx)
{
	char *pos, *name, *value;

	for (pos = kboot_ctx->buf; pos;) {
		pos = get_param_pair(pos, &name, &value, '\n');

		if (name == NULL || param_is_ignored(name))
			continue;

		if (*name == '#')
			continue;

		if (check_for_global_option(kboot_ctx, name, value))
			continue;

		parse_option(kboot_ctx, name, value);
	}
}


static int kboot_parse(struct discover_context *ctx)
{
	static const char *const conf_names[] = {
		"/etc/kboot.conf",
		"/etc/kboot.cnf",
	};
	struct kboot_context *kboot_ctx;
	int fd, len, rc;
	unsigned int i;
	struct stat stat;

	rc = 0;
	fd = -1;

	kboot_ctx = talloc_zero(ctx, struct kboot_context);
	kboot_ctx->discover = ctx;

	for (i = 0, len = 0; i < sizeof(conf_names) / sizeof(conf_names[0]);
		i++) {
		char *filepath = resolve_path(kboot_ctx, conf_names[i],
			ctx->device_path);

		pb_log("%s: try: %s\n", __func__, filepath);

		fd = open(filepath, O_RDONLY);
		if (fd < 0) {
			pb_log("%s: open failed: %s : %s\n", __func__, filepath,
				strerror(errno));
			continue;
		}
		if (fstat(fd, &stat)) {
			pb_log("%s: fstat failed: %s : %s\n", __func__,
				filepath, strerror(errno));
			continue;
		}

		kboot_ctx->buf = talloc_array(kboot_ctx, char,
			stat.st_size + 1);

		len = read(fd, kboot_ctx->buf, stat.st_size);
		if (len < 0) {
			pb_log("%s: read failed: %s : %s\n", __func__, filepath,
				strerror(errno));
			continue;
		}
		kboot_ctx->buf[len] = 0;
	}

	if (len <= 0)
		goto out;

	if (!ctx->device->icon_file)
		ctx->device->icon_file = talloc_strdup(ctx,
				generic_icon_file(guess_device_type(ctx)));

	parse_buf(kboot_ctx);

	rc = 1;

out:
	pb_log("%s: %s\n", __func__, (rc ? "ok" : "failed"));

	if (fd >= 0)
		close(fd);
	talloc_free(kboot_ctx);
	return rc;
}

define_parser(kboot, 98, kboot_parse);
