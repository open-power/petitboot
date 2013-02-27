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
#include "loader.h"
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
 * kexec_load - kexec load helper.
 * @l_image: The local image file for kexec to execute.
 * @l_initrd: Optional local initrd file for kexec --initrd, can be NULL.
 * @args: Optional command line args for kexec --append, can be NULL.
 */

static int kexec_load(const char *l_image, const char *l_initrd,
	const char *args, int dry_run)
{
	int result;
	const char *argv[6];
	const char **p;
	char *s_initrd = NULL;
	char *s_args = NULL;

	p = argv;
	*p++ = pb_system_apps.kexec;	/* 1 */
	*p++ = "-l";			/* 2 */

	if (l_initrd) {
		s_initrd = talloc_asprintf(NULL, "--initrd=%s", l_initrd);
		assert(s_initrd);
		*p++ = s_initrd;	 /* 3 */
	}

	if (args) {
		s_args = talloc_asprintf(NULL, "--append=%s", args);
		assert(s_args);
		*p++ = s_args;		/* 4 */
	}

	*p++ = l_image;			/* 5 */
	*p++ = NULL;			/* 6 */

	result = pb_run_cmd(argv, 1, dry_run);

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	talloc_free(s_initrd);
	talloc_free(s_args);

	return result;
}

/**
 * kexec_reboot - Helper to boot the new kernel.
 *
 * Must only be called after a successful call to kexec_load().
 */

static int kexec_reboot(int dry_run)
{
	int result = 0;
	const char *argv[4];
	const char **p;

	/* First try running shutdown.  Init scripts should run 'exec -e' */

	p = argv;
	*p++ = pb_system_apps.shutdown;	/* 1 */
	*p++ =  "-r";			/* 2 */
	*p++ =  "now";			/* 3 */
	*p++ =  NULL;			/* 4 */

	result = pb_run_cmd(argv, 1, dry_run);

	/* On error, force a kexec with the -e option */

	if (result) {
		p = argv;
		*p++ = pb_system_apps.kexec;	/* 1 */
		*p++ = "-e";			/* 2 */
		*p++ = NULL;			/* 3 */

		result = pb_run_cmd(argv, 1, 0);
	}

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	return result;
}

/**
 * pb_boot - Run kexec with the supplied boot options.
 */

int pb_boot(const struct pb_boot_data *bd, int dry_run)
{
	int result;
	char *l_image = NULL;
	char *l_initrd = NULL;
	unsigned int clean_image = 0;
	unsigned int clean_initrd = 0;

	pb_log("%s: image:   '%s'\n", __func__, bd->image);
	pb_log("%s: initrd:  '%s'\n", __func__, bd->initrd);
	pb_log("%s: args:    '%s'\n", __func__, bd->args);

	result = -1;

	if (bd->image) {
		l_image = pb_load_file(NULL, bd->image, &clean_image);
		if (!l_image)
			goto no_load;
	}

	if (bd->initrd) {
		l_initrd = pb_load_file(NULL, bd->initrd, &clean_initrd);
		if (!l_initrd)
			goto no_load;
	}

	if (!l_image && !l_initrd)
		goto no_load;

	result = kexec_load(l_image, l_initrd, bd->args, dry_run);

no_load:
	if (clean_image)
		unlink(l_image);
	if (clean_initrd)
		unlink(l_initrd);

	talloc_free(l_image);
	talloc_free(l_initrd);

	if (!result)
		result = kexec_reboot(dry_run);

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
