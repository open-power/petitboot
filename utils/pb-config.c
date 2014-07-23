
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include <log/log.h>
#include <pb-config/pb-config.h>
#include <types/types.h>
#include <waiter/waiter.h>
#include <process/process.h>
#include <talloc/talloc.h>

extern struct config *config_get(void);
extern int platform_init(void);

static const struct option options[] = {
	{
		.name = "list",
		.val = 'l',
	},
	{ 0 },
};

static void usage(const char *progname)
{
	fprintf(stderr, "Usage:\t%1$s <var>\n"
			      "\t%1$s --list\n", progname);
}

static void print_one_config(void *ctx, const char *req, const char *name,
		const char *fmt, ...)
{
	bool use_prefix = !req;
	char *val, *sep;
	va_list ap;

	if (req && strcmp(req, name))
		return;

	va_start(ap, fmt);
	val = talloc_vasprintf(ctx, fmt, ap);
	va_end(ap);

	if (!strcmp(val, "(null)")) {
		talloc_free(val);
		if (!use_prefix)
			return;
		val = talloc_strdup(ctx, "");
	}

	sep = use_prefix ? ": " : "";

	printf("%s%s%s\n", use_prefix ? name : "", sep, val);

	talloc_free(val);
}

static void print_config(void *ctx, struct config *config, const char *var)
{
	print_one_config(ctx, var, "bootdev", "%s", config->boot_device);
	print_one_config(ctx, var, "autoboot", "%s",
			config->autoboot_enabled ? "enabled" : "disabled");
	print_one_config(ctx, var, "timeout", "%d",
			config->autoboot_timeout_sec);
	print_one_config(ctx, var, "safe-mode", "%s",
			config->safe_mode ? "enabled" : "disabled");
	print_one_config(ctx, var, "debug", "%s",
			config->debug ? "enabled" : "disabled");
}

int main(int argc, char **argv)
{
	struct waitset *waitset;
	struct config *config;
	void *ctx;
	bool list;

	list = false;

	for (;;) {
		int opt = getopt_long(argc, argv, "l", options, NULL);
		if (opt == -1)
			break;

		switch (opt) {
		case 'l':
			list = true;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	ctx = talloc_new(NULL);

	waitset = waitset_create(ctx);

	process_init(ctx, waitset, false);

	platform_init();

	pb_log_init(stderr);

	config = config_get();

	if (list) {
		print_config(ctx, config, NULL);
	} else {
		int i;

		for (i = optind; i < argc; i++)
			print_config(ctx, config, argv[i]);
	}

	talloc_free(ctx);

	return EXIT_SUCCESS;
}
