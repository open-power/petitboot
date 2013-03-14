#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>

#include "discover/device-handler.h"
#include "discover/parser.h"
#include "discover/parser-utils.h"
#include "discover/paths.h"

static FILE *testf;

struct device *discover_context_device(struct discover_context *ctx)
{
	return ctx->device->device;
}

struct discover_boot_option *discover_boot_option_create(
		struct discover_context *ctx,
		struct discover_device *dev)
{
	struct discover_boot_option *opt;

	opt = talloc_zero(ctx, struct discover_boot_option);
	opt->option = talloc(opt, struct boot_option);
	opt->device = dev;

	return opt;
}

void discover_context_add_boot_option(struct discover_context *ctx,
		struct discover_boot_option *boot_option)
{
	struct boot_option *opt = boot_option->option;

	fprintf(testf, "%s: %s\n", __func__, ctx->device->device->id);
	fprintf(testf, " id     '%s'\n", opt->id);
	fprintf(testf, " name   '%s'\n", opt->name);
	fprintf(testf, " descr  '%s'\n", opt->description);
	fprintf(testf, " icon   '%s'\n", opt->icon_file);
	fprintf(testf, " image  '%s'\n", opt->boot_image_file);
	fprintf(testf, " initrd '%s'\n", opt->initrd_file);
	fprintf(testf, " args   '%s'\n", opt->boot_args);
	fflush(testf);
}

const char *generic_icon_file(
	enum generic_icon_type __attribute__((unused)) type)
{
	return "tester.png";
}

enum generic_icon_type guess_device_type(
	struct discover_context __attribute__((unused)) *ctx)
{
	return ICON_TYPE_UNKNOWN;
}

int main(int argc, char **argv)
{
	struct discover_context *ctx;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <basedir> <devname>\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Default to test on stdout, pb_log on stderr. */

	testf = stdout;

	pb_log_set_stream(stderr);
	pb_log_always_flush(1);
	pb_log("--- parser-test ---\n");

	ctx = talloc_zero(NULL, struct discover_context);

	ctx->device = talloc_zero(ctx, struct discover_device);
	ctx->device->device = talloc_zero(ctx->device, struct device);
	ctx->device->device_path = talloc_asprintf(ctx, "%s/%s",
							argv[1], argv[2]);
	ctx->device->device->id = talloc_strdup(ctx->device->device, argv[2]);

	iterate_parsers(ctx);

	pb_log("--- end ---\n");

	return EXIT_SUCCESS;
}
