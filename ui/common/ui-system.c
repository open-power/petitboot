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

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "log/log.h"
#include <system/system.h>
#include "talloc/talloc.h"
#include "ui-system.h"

/**
 * pb_start_daemon - start the pb-discover daemon.
 */

int pb_start_daemon(void)
{
	int result;
	const char *argv[2];
	char *name = talloc_asprintf(NULL, "%s/sbin/pb-discover",
		pb_system_apps.prefix);

	argv[0] = name;
	argv[1] =  NULL;

	result = pb_run_cmd(argv, 0, 0);

	talloc_free(name);

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	return result;
}

/**
 * pb_elf_hash - Standard elf hash routine.
 */

unsigned int pb_elf_hash(const char *str)
{
	unsigned int h = 0, g;

	while (*str) {
		h = (h << 4) + *str++;
		g = h & 0xf0000000;
		if (g)
			h ^= g >> 24;
		h &= ~g;
	}
	pb_log("%s: %x\n", __func__, h);
	return h;
}

/**
 * pb_cat_hash - Hashes concatenation of two strings.
 */

unsigned int pb_cat_hash(const char *a, const char *b)
{
	unsigned int h;
	char *s;

	s = talloc_asprintf(NULL, "%s%s", a, b);
	h = pb_elf_hash(s);
	talloc_free(s);

	return h;
}
