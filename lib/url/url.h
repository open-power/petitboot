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

#if !defined(_PB_URL_PARSER_H)
#define _PB_URL_PARSER_H

/**
 * enum pb_url_scheme - Enumeration of the URL schemes we can handle.
 */

enum pb_url_scheme {
	pb_url_unknown = 0,
	pb_url_file,
	pb_url_ftp,
	pb_url_http,
	pb_url_https,
	pb_url_nfs,
	pb_url_sftp,
	pb_url_tftp,
};

/**
 * struct pb_url - Parsed URL info.
 * @scheme: The enum pb_url_scheme for this URL.
 * @full: The full URL = scheme://host:port/path.
 * @host: The host part of URL.
 * @port: The port part of URL.
 * @path: The path part of URL.
 * @dir: The dir part of path.
 * @file: The file part of path.
 *
 * For info on URL syntax see:
 *  http://www.ietf.org/rfc/rfc3986.txt
 */

struct pb_url {
	enum pb_url_scheme scheme;
	char *full;
	char *host;
	char *port;
	char *path;
	char *dir;
	char *file;
};

struct pb_url *pb_url_parse(void *ctx, const char *url_str);
struct pb_url *pb_url_join(void *ctx, const struct pb_url *url, const char *s);

const char *pb_url_scheme_name(enum pb_url_scheme scheme);

#endif
