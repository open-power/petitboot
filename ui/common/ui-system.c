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
 * run_kexec_local - Final kexec helper.
 * @l_image: The local image file for kexec to execute.
 * @l_initrd: Optional local initrd file for kexec --initrd, can be NULL.
 * @args: Optional command line args for kexec --append, can be NULL.
 */

static int run_kexec_local(const char *l_image, const char *l_initrd,
	const char *args)
{
	int result;
	const char *argv[6];
	const char **p;
	char *s_initrd = NULL;
	char *s_args = NULL;

	p = argv;
	*p++ = pb_system_apps.kexec;		/* 1 */

	if (l_initrd) {
		s_initrd = talloc_asprintf(NULL, "--initrd=%s", l_initrd);
		assert(s_initrd);
		*p++ = s_initrd;		 /* 2 */
	}

	if (args) {
		s_args = talloc_asprintf(NULL, "--append=%s", args);
		assert(s_args);
		*p++ = s_args;			 /* 3 */
	}

	/* First try by telling kexec to run shutdown */

	*(p + 0) = l_image;
	*(p + 1) = NULL;

	result = pb_run_cmd(argv);

	/* kexec will return zero on success */
	/* On error, force a kexec with the -f option */

	if (result) {
		*(p + 0) = "-f";		/* 4 */
		*(p + 1) = l_image;		/* 5 */
		*(p + 2) = NULL;		/* 6 */

		result = pb_run_cmd(argv);
	}

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	talloc_free(s_initrd);
	talloc_free(s_args);

	return result;
}

/**
 * pb_run_kexec - Run kexec with the supplied boot options.
 *
 * For the convenience of the user, tries to load both files before
 * returning error.
 */

int pb_run_kexec(const struct pb_kexec_data *kd)
{
	int result;
	char *l_image;
	char *l_initrd;

	pb_log("%s: image:  '%s'\n", __func__, kd->image);
	pb_log("%s: initrd: '%s'\n", __func__, kd->initrd);
	pb_log("%s: args:   '%s'\n", __func__, kd->args);

	if (kd->image)
		l_image = pb_load_file(NULL, kd->image);
	else {
		l_image = NULL;
		pb_log("%s: error null image\n", __func__);
	}

	l_initrd = kd->initrd ? pb_load_file(NULL, kd->initrd) : NULL;

	if (!l_image || (kd->initrd && !l_initrd))
		result = -1;
	else
		result = run_kexec_local(l_image, l_initrd, kd->args);

	talloc_free(l_image);
	talloc_free(l_initrd);

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
