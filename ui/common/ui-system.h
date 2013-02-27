/*
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

#if !defined(_PB_UI_SYSTEM_H)
#define _PB_UI_SYSTEM_H

#include <stdint.h>

#include "system/system.h"
#include "types/types.h"
#include "ui/common/timer.h"

#include <signal.h>

struct pb_boot_data {
	char *image;
	char *initrd;
	char *args;
};

int pb_boot(const struct pb_boot_data *bd, int dry_run);
int pb_start_daemon(void);

unsigned int pb_elf_hash(const char *str);
unsigned int pb_cat_hash(const char *a, const char *b);

static inline uint32_t pb_opt_hash(const struct device *dev,
	const struct boot_option *opt)
{
	return pb_cat_hash(dev->name, opt->name);
}

struct pb_opt_data {
	const char *name;
	struct pb_boot_data *bd;

	/* optional data */
	const struct device *dev;
	const struct boot_option *opt;
	uint32_t opt_hash;
};

struct pb_signal_data {
	sig_atomic_t abort;
	sig_atomic_t resize;
	struct ui_timer timer;
};

#endif
