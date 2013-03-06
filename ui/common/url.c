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
#include <string.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "url.h"

/**
 * pb_scheme_info - Helper for parsing URLs.
 */

struct pb_scheme_info {
	enum pb_url_scheme scheme;
	const char *str;
	unsigned int str_len;
};

static const struct pb_scheme_info schemes[] = {
	{
		.scheme = pb_url_file,
		.str = "file",
		.str_len = sizeof("file") - 1,
	},
	{
		.scheme = pb_url_ftp,
		.str = "ftp",
		.str_len = sizeof("ftp") - 1,
	},
	{
		.scheme = pb_url_http,
		.str = "http",
		.str_len = sizeof("http") - 1,
	},
	{
		.scheme = pb_url_https,
		.str = "https",
		.str_len = sizeof("https") - 1,
	},
	{
		.scheme = pb_url_nfs,
		.str = "nfs",
		.str_len = sizeof("nfs") - 1,
	},
	{
		.scheme = pb_url_sftp,
		.str = "sftp",
		.str_len = sizeof("sftp") - 1,
	},
	{
		.scheme = pb_url_tftp,
		.str = "tftp",
		.str_len = sizeof("tftp") - 1,
	},
};

static const struct pb_scheme_info *file_scheme = &schemes[0];

/**
 * pb_url_find_scheme - Find the pb_scheme_info for a URL string.
 */

static const struct pb_scheme_info *pb_url_find_scheme(const char *url)
{
	static const int sep_len = sizeof("://") - 1;
	static const char *sep = "://";
	unsigned int i, url_len;

	url_len = strlen(url);

	for (i = 0; i < sizeof(schemes) / sizeof(schemes[0]); i++) {
		const struct pb_scheme_info *scheme = &schemes[i];

		if (url_len < scheme->str_len + sep_len)
			continue;

		if (strncmp(url + scheme->str_len, sep, sep_len))
			continue;

		if (strncasecmp(url, scheme->str, scheme->str_len))
			continue;

		return scheme;
	}

	/* Assume this is a non-url local file. */

	return file_scheme;
}

/**
 * pb_url_parse - Parse a remote file URL.
 * @ctx: The talloc context to associate with the returned string.
 *
 * Returns a talloc'ed struct pb_url instance on success, or NULL on error.
 */

struct pb_url *pb_url_parse(void *ctx, const char *url_str)
{
	const struct pb_scheme_info *si;
	struct pb_url *url;
	const char *p;

	pb_log("%s: '%s'\n", __func__, url_str);

	if (!url_str || !*url_str) {
		assert(0 && "bad url");
		return NULL;
	}

	url = talloc_zero(ctx, struct pb_url);

	if (!url)
		return NULL;

	si = pb_url_find_scheme(url_str);

	url->scheme = si->scheme;
	p = url_str + si->str_len + strlen("://");

	url->full = talloc_strdup(url, url_str);

	if (url->scheme == pb_url_file) {
		url->port = NULL;
		url->host = NULL;
		url->path = talloc_strdup(url, p);
	} else {
		int len;
		const char *col;
		const char *path;

		path = strchr(p, '/');

		if (!path) {
			pb_log("%s: parse path failed '%s'\n", __func__ , p);
			goto fail;
		}

		col = strchr(p, ':');

		if (col) {
			len = path - col - 1;
			url->port = len ? talloc_strndup(url, col + 1, len)
				: NULL;
			len = col - p;
			url->host = len ? talloc_strndup(url, p, len) : NULL;
		} else {
			url->port = NULL;
			url->host = talloc_strndup(url, p, path - p);
		}

		/* remove multiple leading slashes */
		for (; *path && *(path+1) == '/'; path++)
			;

		url->path = talloc_strdup(url, path);
	}

	p = strrchr(url->path, '/');

	if (p) {
		p++;
		url->dir = talloc_strndup(url, url->path, p - url->path);
		url->file = talloc_strdup(url, p);
	} else {
		url->dir = NULL;
		url->file = talloc_strdup(url, url->path);
	}

	pb_log(" scheme %d\n", url->scheme);
	pb_log(" host '%s'\n", url->host);
	pb_log(" port '%s'\n", url->port);
	pb_log(" path '%s'\n", url->path);
	pb_log(" dir '%s'\n", url->dir);
	pb_log(" file '%s'\n", url->file);

	return url;

fail:
	talloc_free(url);
	return NULL;
}

const char *pb_url_scheme_name(enum pb_url_scheme scheme)
{
	unsigned int i;

	for (i = 0; i < sizeof(schemes) / sizeof(schemes[0]); i++)
		if (schemes[i].scheme == scheme)
			return schemes[i].str;

	return NULL;
}
