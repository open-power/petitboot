
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <waiter/waiter.h>
#include <log/log.h>
#include <pb-config/pb-config.h>

#include "udev.h"
#include "user-event.h"
#include "discover-server.h"
#include "device-handler.h"
#include "network.h"

static void print_version(void)
{
	printf("pb-discover (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

static void print_usage(void)
{
	print_version();
	printf(
"Usage: pb-discover [-a, --no-autoboot] [-h, --help] [-l, --log log-file]\n"
"                   [-n, --dry-run] [-V, --version]\n");
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
		{"version",        no_argument,       NULL, 'V'},
		{ NULL, 0, NULL, 0},
	};
	static const char short_options[] = "ahl:nV";
	static const struct opts default_values = {
		.no_autoboot = opt_no,
		.log_file = "/var/log/petitboot/pb-discover.log",
		.dry_run = opt_no,
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
	struct network *network;
	struct waitset *waitset;
	struct opts opts;
	struct pb_udev *udev;
	struct user_event *uev;

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

	if (strcmp(opts.log_file, "-")) {
		FILE *log = fopen(opts.log_file, "a");

		assert(log);
		pb_log_set_stream(log);
	} else
		pb_log_set_stream(stderr);

#if defined(DEBUG)
	pb_log_always_flush(1);
#endif
	pb_log("--- pb-discover ---\n");

	/* we look for closed sockets when we write, so ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, sigint_handler);

	config_init(NULL);

	if (opts.no_autoboot == opt_yes)
		config_set_autoboot(false);

	waitset = waitset_create(NULL);

	server = discover_server_init(waitset);
	if (!server)
		return EXIT_FAILURE;

	network = network_init(server, waitset, opts.dry_run == opt_yes);
	if (!network)
		return EXIT_FAILURE;

	handler = device_handler_init(server, waitset, opts.dry_run == opt_yes);
	if (!handler)
		return EXIT_FAILURE;

	discover_server_set_device_source(server, handler);

	udev = udev_init(waitset, handler);
	if (!udev)
		return EXIT_FAILURE;

	uev = user_event_init(waitset, handler);
	if (!uev)
		return EXIT_FAILURE;

	for (running = 1; running;) {
		if (waiter_poll(waitset))
			break;
	}

	device_handler_destroy(handler);
	udev_destroy(udev);
	config_fini();

	pb_log("--- end ---\n");

	return EXIT_SUCCESS;
}
