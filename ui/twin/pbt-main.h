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

#if !defined(_PBT_MAIN_H)
#define _PBT_MAIN_H

#include "pbt-scr.h"

/**
 * enum opt_value - Tri-state options variables.
 */

enum pbt_opt_value {pbt_opt_undef = 0, pbt_opt_yes, pbt_opt_no};

/**
 * struct opts - Values from command line options.
 */

struct pbt_opts {
	enum pbt_twin_backend backend;
	enum pbt_opt_value show_help;
	const char *log_file;
	enum pbt_opt_value reset_defaults;
	enum pbt_opt_value start_daemon;
	enum pbt_opt_value use_timeout;
	enum pbt_opt_value show_version;
};


void pbt_print_version(void);
void pbt_print_usage(void);
int pbt_opts_parse(struct pbt_opts *opts, int argc, char *argv[]);

#endif
