/*
 * Petitboot generic ncurses bootloader UI
 *
 *  Copyright (C) 2009 Sony Computer Entertainment Inc.
 *  Copyright 2009 Sony Corp.
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

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <libintl.h>
#include <locale.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "i18n/i18n.h"
#include "ui/common/discover-client.h"
#include "nc-cui.h"

static void print_version(void)
{
	printf("petitboot-nc (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

static void print_usage(void)
{
	print_version();
	printf(
"%s: petitboot-nc [-h, --help] [-l, --log log-file] [-s, --start-daemon]\n"
"                    [-t, --timeout] [-v, --verbose] [-V, --version]\n",
			_("Usage"));
}

/**
 * enum opt_value - Tri-state options variables.
 */

enum opt_value {opt_undef = 0, opt_yes, opt_no};

/**
 * struct opts - Values from command line options.
 */

struct opts {
	enum opt_value show_help;
	const char *log_file;
	enum opt_value start_daemon;
	enum opt_value timeout;
	enum opt_value verbose;
	enum opt_value show_version;
};

/**
 * opts_parse - Parse the command line options.
 */

static int opts_parse(struct opts *opts, int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help",         no_argument,       NULL, 'h'},
		{"log",          required_argument, NULL, 'l'},
		{"start-daemon", no_argument,       NULL, 's'},
		{"timeout",	 no_argument,	    NULL, 't'},
		{"verbose",      no_argument,       NULL, 'v'},
		{"version",      no_argument,       NULL, 'V'},
		{ NULL,          0,                 NULL, 0},
	};
	static const char short_options[] = "dhl:stvV";
	static const struct opts default_values = { 0 };

	*opts = default_values;

	while (1) {
		int c = getopt_long(argc, argv, short_options, long_options,
			NULL);

		if (c == EOF)
			break;

		switch (c) {
		case 'h':
			opts->show_help = opt_yes;
			break;
		case 'l':
			opts->log_file = optarg;
			break;
		case 's':
			opts->start_daemon = opt_yes;
			break;
		case 't':
			opts->timeout = opt_yes;
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

	return 0;
}

static char *default_log_filename(void)
{
	const char *base = "/var/log/petitboot/petitboot-nc";
	static char name[PATH_MAX];
	char *tty;
	int i;

	tty = ttyname(STDIN_FILENO);

	/* strip /dev/ */
	if (tty && !strncmp(tty, "/dev/", 5))
		tty += 5;

	/* change slashes to hyphens */
	for (i = 0; tty && tty[i]; i++)
		if (tty[i] == '/')
			tty[i] = '-';

	if (!tty || !*tty)
		tty = "unknown";

	snprintf(name, sizeof(name), "%s.%s.log", base, tty);

	return name;
}

static struct cui *cui;

/*
 * struct pb_cui - Main cui program instance.
 * @mm: Main menu.
 * @svm: Set video mode menu.
 */

static void sig_handler(int signum)
{
	DBGS("%d\n", signum);

	switch (signum) {
	case SIGWINCH:
		if (cui)
			cui_resize(cui);
		break;
	default:
		assert(0 && "unknown sig");
		/* fall through */
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		if (cui)
			cui_abort(cui);
		break;
	}
}

/**
 * main - cui bootloader main routine.
 */

int main(int argc, char *argv[])
{
	static struct sigaction sa;
	const char *log_filename;
	int result;
	int cui_result;
	struct opts opts;
	FILE *log;

	result = opts_parse(&opts, argc, argv);

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (result) {
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

	if (opts.log_file)
		log_filename = opts.log_file;
	else
		log_filename = default_log_filename();

	log = stderr;
	if (strcmp(log_filename, "-")) {
		log = fopen(log_filename, "a");

		if (!log)
			log = fopen("/dev/null", "a");
	}

	pb_log_init(log);

	if (opts.verbose == opt_yes)
		pb_log_set_debug(true);

	pb_log("--- petitboot-nc ---\n");

	sa.sa_handler = sig_handler;
	result = sigaction(SIGALRM, &sa, NULL);
	result += sigaction(SIGHUP, &sa, NULL);
	result += sigaction(SIGINT, &sa, NULL);
	result += sigaction(SIGTERM, &sa, NULL);
	result += sigaction(SIGWINCH, &sa, NULL);

	if (result) {
		pb_log("%s sigaction failed.\n", __func__);
		return EXIT_FAILURE;
	}

	cui = cui_init(NULL, NULL, opts.start_daemon, opts.timeout);
	if (!cui)
		return EXIT_FAILURE;

	cui_result = cui_run(cui);

	talloc_free(cui);

	pb_log("--- end ---\n");

	return cui_result ? EXIT_FAILURE : EXIT_SUCCESS;
}
