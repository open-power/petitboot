/*
 *  Copyright Geoff Levand <geoff@infradead.org>
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
#include <stdio.h>
#include <getopt.h>

#include "pbt-main.h"

void pbt_print_version(void)
{
	printf("petitboot-twin (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

void pbt_print_usage(void)
{
	pbt_print_version();
	printf(
"Usage: petitboot-twin [-d, --dry-run] [-h, --help] [-l, --log log-file]\n"
"                      [-r, --reset-defaults] [-s, --start-daemon]\n"
"                      [-t, --timeout] [-V, --version]\n"
"                      [[-f --fbdev] | [-x --x11]]\n");
}

/**
 * pbt_opts_parse - Parse the command line options.
 */

int pbt_opts_parse(struct pbt_opts *opts, int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"dry-run",        no_argument,       NULL, 'd'},
		{"fbdev",          no_argument,       NULL, 'f'},
		{"help",           no_argument,       NULL, 'h'},
		{"log",            required_argument, NULL, 'l'},
		{"reset-defaults", no_argument,       NULL, 'r'},
		{"start-daemon",   no_argument,       NULL, 's'},
		{"timeout",        no_argument,       NULL, 't'},
		{"version",        no_argument,       NULL, 'V'},
		{"x11",            no_argument,       NULL, 'x'},
		{ NULL, 0, NULL, 0},
	};
	static const char short_options[] = "dfhl:strVx";
	static const struct pbt_opts default_values = {
		.backend = pbt_twin_x11,
		.log_file = "/var/log/petitboot/petitboot-twin.log",
	};

	*opts = default_values;

	while (1) {
		int c = getopt_long(argc, argv, short_options, long_options,
			NULL);

		if (c == EOF)
			break;

		switch (c) {
		case 'd':
			opts->dry_run = pbt_opt_yes;
			break;
		case 'f':
			opts->backend = pbt_twin_fbdev;
			break;
		case 'h':
			opts->show_help = pbt_opt_yes;
			break;
		case 'l':
			opts->log_file = optarg;
			break;
		case 's':
			opts->start_daemon = pbt_opt_yes;
			break;
		case 't':
			opts->use_timeout = pbt_opt_yes;
			break;
		case 'r':
			opts->reset_defaults = pbt_opt_yes;
			break;
		case 'V':
			opts->show_version = pbt_opt_yes;
			break;
		case 'x':
			opts->backend = pbt_twin_x11;
			break;
		default:
			opts->show_help = pbt_opt_yes;
			return -1;
		}
	}

	return optind != argc;
}
