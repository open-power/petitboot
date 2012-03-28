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

#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
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
"Usage: petitboot-nc [-d, --dry-run] [-h, --help] [-l, --log log-file]\n"
"                    [-s, --start-daemon] [-V, --version]\n");
}

/**
 * enum opt_value - Tri-state options variables.
 */

enum opt_value {opt_undef = 0, opt_yes, opt_no};

/**
 * struct opts - Values from command line options.
 */

struct opts {
	enum opt_value dry_run;
	enum opt_value show_help;
	const char *log_file;
	enum opt_value start_daemon;
	enum opt_value show_version;
};

/**
 * opts_parse - Parse the command line options.
 */

static int opts_parse(struct opts *opts, int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"dry-run",      no_argument,       NULL, 'd'},
		{"help",         no_argument,       NULL, 'h'},
		{"log",          required_argument, NULL, 'l'},
		{"start-daemon", no_argument,       NULL, 's'},
		{"version",      no_argument,       NULL, 'V'},
		{ NULL,          0,                 NULL, 0},
	};
	static const char short_options[] = "dhl:sV";
	static const struct opts default_values = {
		.log_file = "/var/log/petitboot/petitboot-nc.log",
	};

	*opts = default_values;

	while (1) {
		int c = getopt_long(argc, argv, short_options, long_options,
			NULL);

		if (c == EOF)
			break;

		switch (c) {
		case 'd':
			opts->dry_run = opt_yes;
			break;
		case 'h':
			opts->show_help = opt_yes;
			break;
		case 'l':
			opts->log_file = optarg;
			break;
		case 's':
			opts->start_daemon = opt_yes;
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

/**
 * struct pb_cui - Main cui program instance.
 * @mm: Main menu.
 * @svm: Set video mode menu.
 */

struct pb_cui {
	struct pmenu *mm;
	struct cui *cui;
	struct opts opts;
};

static struct pb_cui *pb_from_cui(struct cui *cui)
{
	struct pb_cui *pb;

	assert(cui->c_sig == pb_cui_sig);
	pb = cui->platform_info;
	assert(pb->cui->c_sig == pb_cui_sig);
	return pb;
}

/**
 * pb_kexec_cb - The kexec callback.
 */

static int pb_kexec_cb(struct cui *cui, struct cui_opt_data *cod)
{
	struct pb_cui *pb = pb_from_cui(cui);

	pb_log("%s: %s\n", __func__, cod->name);

	assert(pb->cui->current == &pb->cui->main->scr);

	return pb_run_kexec(cod->kd, pb->opts.dry_run);
}

/**
 * pb_mm_init - Setup the main menu instance.
 */

static struct pmenu *pb_mm_init(struct pb_cui *pb_cui)
{
	int result;
	struct pmenu *m;
	struct pmenu_item *i;

	m = pmenu_init(pb_cui->cui, 1, cui_on_exit);

	if (!m) {
		pb_log("%s: failed\n", __func__);
		return NULL;
	}

	m->on_open = cui_on_open;

	m->scr.frame.title = talloc_strdup(m, "Petitboot");
	m->scr.frame.help = talloc_strdup(m,
		"ESC=exit, Enter=accept, e=edit, o=open");
	m->scr.frame.status = talloc_strdup(m, "Welcome to Petitboot");

	i = pmenu_item_init(m, 0, "Exit to Shell");
	i->on_execute = pmenu_exit_cb;

	result = pmenu_setup(m);

	if (result) {
		pb_log("%s:%d: pmenu_setup failed: %s\n", __func__, __LINE__,
			strerror(errno));
		goto fail_setup;
	}

	menu_opts_off(m->ncm, O_SHOWDESC);
	set_menu_mark(m->ncm, " *");
	set_current_item(m->ncm, i->nci);

	return m;

fail_setup:
	talloc_free(m);
	return NULL;
}

static struct pb_cui pb;

static void sig_handler(int signum)
{
	DBGS("%d\n", signum);

	switch (signum) {
	case SIGALRM:
		if (pb.cui)
			ui_timer_sigalrm(&pb.cui->timer);
		break;
	case SIGWINCH:
		if (pb.cui)
			cui_resize(pb.cui);
		break;
	default:
		assert(0 && "unknown sig");
		/* fall through */
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		if (pb.cui)
			cui_abort(pb.cui);
		break;
	}
}

/**
 * main - cui bootloader main routine.
 */

int main(int argc, char *argv[])
{
	static struct sigaction sa;
	int result;
	int cui_result;

	result = opts_parse(&pb.opts, argc, argv);

	if (result) {
		print_usage();
		return EXIT_FAILURE;
	}

	if (pb.opts.show_help == opt_yes) {
		print_usage();
		return EXIT_SUCCESS;
	}

	if (pb.opts.show_version == opt_yes) {
		print_version();
		return EXIT_SUCCESS;
	}

	if (strcmp(pb.opts.log_file, "-")) {
		FILE *log = fopen(pb.opts.log_file, "a");

		assert(log);
		pb_log_set_stream(log);
	} else
		pb_log_set_stream(stderr);

#if defined(DEBUG)
	pb_log_always_flush(1);
#endif

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

	pb.cui = cui_init(&pb, pb_kexec_cb, NULL, pb.opts.start_daemon);

	if (!pb.cui)
		return EXIT_FAILURE;

	pb.mm = pb_mm_init(&pb);
	ui_timer_disable(&pb.cui->timer);

	cui_result = cui_run(pb.cui, pb.mm, 0);

	pmenu_delete(pb.mm);

	talloc_free(pb.cui);

	pb_log("--- end ---\n");

	return cui_result ? EXIT_FAILURE : EXIT_SUCCESS;
}
