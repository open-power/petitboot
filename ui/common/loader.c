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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>

#include "log/log.h"
#include <system/system.h>
#include "talloc/talloc.h"
#include "loader.h"
#include "url.h"


/**
 * pb_local_name - Helper to create a unique local path name.
 * @ctx: A talloc context.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *pb_local_name(void *ctx)
{
	char *tmp, *ret;

	tmp = tempnam(NULL, "pb-");

	if (!tmp)
		return NULL;

	ret = talloc_strdup(ctx, tmp);
	free(tmp);

	return ret;
}

/**
 * pb_load_nfs - Mounts the NFS export and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *pb_load_nfs(void *ctx, struct pb_url *url)
{
	int result;
	const char *argv[8];
	const char **p;
	char *local;
	char *opts;

	local = pb_local_name(ctx);

	if (!local)
		return NULL;

	result = pb_mkdir_recursive(local);

	if (result)
		goto fail;

	opts = talloc_strdup(NULL, "ro,nolock,nodiratime");

	if (url->port)
		opts = talloc_asprintf_append(opts, ",port=%s", url->port);

	p = argv;
	*p++ = pb_system_apps.mount;	/* 1 */
	*p++ = "-t";			/* 2 */
	*p++ = "nfs";			/* 3 */
	*p++ = opts;			/* 4 */
	*p++ = url->host;		/* 5 */
	*p++ = url->dir;		/* 6 */
	*p++ = local;			/* 7 */
	*p++ = NULL;			/* 8 */

	result = pb_run_cmd(argv);

	talloc_free(opts);

	if (result)
		goto fail;

	local = talloc_asprintf_append(local,  "/%s", url->path);
	pb_log("%s: local '%s'\n", __func__, local);

	return local;

fail:
	pb_rmdir_recursive("/", local);
	talloc_free(local);
	return NULL;
}

/**
 * pb_load_sftp - Loads a remote file via sftp and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *pb_load_sftp(void *ctx, struct pb_url __attribute__((unused)) *url)
{
	int result;
	const char *argv[5];
	const char **p;
	char *local;

	local = pb_local_name(ctx);

	if (!local)
		return NULL;

	p = argv;
	*p++ = pb_system_apps.sftp;	/* 1 */
	*p++ = url->host;		/* 2 */
	*p++ = url->path;		/* 3 */
	*p++ = local;			/* 4 */
	*p++ = NULL;			/* 5 */

	result = pb_run_cmd(argv);

	if (result)
		goto fail;

	return local;

fail:
	talloc_free(local);
	return NULL;
}

/**
 * pb_load_tftp - Loads a remote file via tftp and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *pb_load_tftp(void *ctx, struct pb_url *url)
{
	int result;
	const char *argv[10];
	const char **p;
	char *local;

	local = pb_local_name(ctx);

	if (!local)
		return NULL;

	/* first try busybox tftp args */

	p = argv;
	*p++ = pb_system_apps.tftp;	/* 1 */
	*p++ = "-g";			/* 2 */
	*p++ = "-l";			/* 3 */
	*p++ = local;			/* 4 */
	*p++ = "-r";			/* 5 */
	*p++ = url->path;		/* 6 */
	*p++ = url->host;		/* 7 */
	if (url->port)
		*p++ = url->port;	/* 8 */
	*p++ = NULL; 			/* 9 */

	result = pb_run_cmd(argv);

	if (!result)
		return local;

	/* next try tftp-hpa args */

	p = argv;
	*p++ = pb_system_apps.tftp;	/* 1 */
	*p++ = "-m";			/* 2 */
	*p++ = "binary";		/* 3 */
	*p++ = url->host;		/* 4 */
	if (url->port)
		*p++ = url->port;	/* 5 */
	*p++ = "-c";			/* 6 */
	*p++ = "get";			/* 7 */
	*p++ = url->path;		/* 8 */
	*p++ = local;			/* 9 */
	*p++ = NULL;			/* 10 */

	result = pb_run_cmd(argv);

	if (!result)
		return local;

	talloc_free(local);
	return NULL;
}

enum wget_flags {
	wget_empty = 0,
	wget_no_check_certificate = 1,
};

/**
 * pb_load_wget - Loads a remote file via wget and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *pb_load_wget(void *ctx, struct pb_url *url, enum wget_flags flags)
{
	int result;
	const char *argv[6];
	const char **p;
	char *local;

	local = pb_local_name(ctx);

	if (!local)
		return NULL;

	p = argv;
	*p++ = pb_system_apps.wget;			/* 1 */
	*p++ = "-O";					/* 2 */
	*p++ = local;					/* 3 */
	*p++ = url->full;				/* 4 */
	if (flags & wget_no_check_certificate)
		*p++ = "--no-check-certificate";	/* 5 */
	*p++ = NULL;					/* 6 */

	result = pb_run_cmd(argv);

	if (result)
		goto fail;

	return local;

fail:
	talloc_free(local);
	return NULL;
}

/**
 * pb_load_file - Loads a remote file and returns the local file path.
 * @ctx: The talloc context to associate with the returned string.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

char *pb_load_file(void *ctx, const char *remote)
{
	char *local;
	struct pb_url *url = pb_url_parse(NULL, remote);

	if (!url)
		return NULL;

	switch (url->scheme) {
	case pb_url_ftp:
	case pb_url_http:
		local = pb_load_wget(ctx, url, 0);
		break;
	case pb_url_https:
		local = pb_load_wget(ctx, url,
			wget_no_check_certificate);
		break;
	case pb_url_nfs:
		local = pb_load_nfs(ctx, url);
		break;
	case pb_url_sftp:
		local = pb_load_sftp(ctx, url);
		break;
	case pb_url_tftp:
		local = pb_load_tftp(ctx, url);
		break;
	default:
		local = talloc_strdup(ctx, url->full);
		break;
	}

	talloc_free(url);
	return local;
}
