
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>
#include <string.h>

#include <waiter/waiter.h>
#include <log/log.h>
#include <process/process.h>
#include <talloc/talloc.h>
#include <i18n/i18n.h>

#include "discover-server.h"
#include "device-handler.h"
#include "sysinfo.h"
#include "platform.h"

static void print_version(void)
{
	printf("pb-discover (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

static void print_usage(void)
{
	print_version();
	printf(
"Usage: pb-discover [-a, --no-autoboot] [-h, --help] [-l, --log log-file]\n"
"                   [-n, --dry-run] [-v, --verbose] [-V, --version]\n");
}

/**
 * enum opt_value - Tri-state options variables.
 */

enum opt_value {opt_undef = 0, opt_yes, opt_no};

/**
 * struct opts - Values from command line options.
 */

struct opts {
	enum opt_value no_autoboot;
	enum opt_value show_help;
	const char *log_file;
	enum opt_value dry_run;
	enum opt_value show_version;
	enum opt_value verbose;
};

/**
 * opts_parse - Parse the command line options.
 */

static int opts_parse(struct opts *opts, int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"no-autoboot",    no_argument,       NULL, 'a'},
		{"help",           no_argument,       NULL, 'h'},
		{"log",            required_argument, NULL, 'l'},
		{"dry-run",        no_argument,       NULL, 'n'},
		{"verbose",        no_argument,       NULL, 'v'},
		{"version",        no_argument,       NULL, 'V'},
		{ NULL, 0, NULL, 0},
	};
	static const char short_options[] = "ahl:nvV";
	static const struct opts default_values = {
		.no_autoboot = opt_no,
		.log_file = "/var/log/petitboot/pb-discover.log",
		.dry_run = opt_no,
		.verbose = opt_no,
	};

	*opts = default_values;

	while (1) {
		int c = getopt_long(argc, argv, short_options, long_options,
			NULL);

		if (c == EOF)
			break;

		switch (c) {
		case 'a':
			opts->no_autoboot = opt_yes;
			break;
		case 'h':
			opts->show_help = opt_yes;
			break;
		case 'l':
			opts->log_file = optarg;
			break;
		case 'n':
			opts->dry_run = opt_yes;
			break;
		case 'v':
			opts->verbose = opt_yes;
			break;
		case 'V':
			opts->show_version = opt_yes;
			break;
		default:
			opts->show_help = opt_yes;
			return -1;
		}
	}

	return optind != argc;
}

static int running;

static void sigint_handler(int __attribute__((unused)) signum)
{
	running = 0;
}

int main(int argc, char *argv[])
{
	struct device_handler *handler;
	struct discover_server *server;
	struct waitset *waitset;
	struct procset *procset;
	struct opts opts;
	FILE *log;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (opts_parse(&opts, argc, argv)) {
		print_usage();
		return EXIT_FAILURE;
	}

	if (opts.show_help == opt_yes) {
		print_usage();
		return EXIT_SUCCESS;
	}

	if (opts.show_version == opt_yes) {
		print_version();
		return EXIT_SUCCESS;
	}

	log = stderr;
	if (strcmp(opts.log_file, "-")) {
		log = fopen(opts.log_file, "a");
		if (!log) {
			fprintf(stderr, "can't open log file %s, logging to "
					"stderr\n", opts.log_file);
			log = stderr;
		}
	}
	pb_log_init(log);

	if (opts.verbose == opt_yes)
		pb_log_set_debug(true);

	pb_log("--- pb-discover ---\n");

	/* we look for closed sockets when we write, so ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, sigint_handler);

	waitset = waitset_create(NULL);

	server = discover_server_init(waitset);
	if (!server)
		return EXIT_FAILURE;

	procset = process_init(server, waitset, opts.dry_run == opt_yes);
	if (!procset)
		return EXIT_FAILURE;

	platform_init(NULL);
	if (opts.no_autoboot == opt_yes)
		config_set_autoboot(false);

	if (config_get()->lang)
		setlocale(LC_ALL, config_get()->lang);

	if (config_get()->debug)
		pb_log_set_debug(true);

	if (platform_restrict_clients())
		discover_server_set_auth_mode(server, true);

	system_info_init(server);

	handler = device_handler_init(server, waitset, opts.dry_run == opt_yes);
	if (!handler)
		return EXIT_FAILURE;

	discover_server_set_device_source(server, handler);

	for (running = 1; running;) {
		if (waiter_poll(waitset))
			break;
	}

	device_handler_destroy(handler);
	discover_server_destroy(server);
	platform_fini();
	talloc_free(waitset);

	pb_log("--- end ---\n");

	if (log != stderr)
		fclose(log);

	return EXIT_SUCCESS;
}
