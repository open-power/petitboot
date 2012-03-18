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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "paths.h"

/**
 * conf_strip_str - Remove quotes and/or whitespace around a string.
 *
 * Returns the next byte to process, or NULL if the string is empty.
 */

char *conf_strip_str(char *s)
{
	char *e;

	if (!s)
		return NULL;

	while (*s == '"' || *s == '\'' || isspace(*s))
		s++;

	e = s + strlen(s) - 1;

	while (*e == '"' || *e == '\'' || isspace(*e))
		*(e--) = 0;

	return strlen(s) ? s : NULL;
}

/**
 * conf_replace_char - replace one char with another.
 */

char *conf_replace_char(char *s, char from, char to)
{
	if (!s)
		return NULL;

	for ( ; *s; s++)
		if (*s == from)
			*s = to;

	return s;
}

/**
 * conf_get_param_pair - Get the next 'name=value' parameter pair.
 * @str: The string to process.
 * @name_out: Returns a pointer to the name.
 * @value_out: Returns a pointer to the value.
 * @terminator: The pair separator/terminator.
 *
 * Parses a name=value pair returning pointers in @name_out and @value_out.
 * The pair can be terminated by @terminator or a zero.
 * If no '=' character is found @value_out is set and @name_out is
 * set to NULL.
 * If the value is empty *value_out is set to NULL.
 * The string is modified in place.
 *
 * Returns the next byte to process, or NULL if we've hit the end of the
 * string.
 */

char *conf_get_param_pair(char *str, char **name_out, char **value_out,
		char terminator)
{
	char *sep, *end;

	/* terminate the value */
	end = strchr(str, terminator);

	if (end)
		*end = 0;

	sep = strchr(str, '=');

	if (!sep) {
		*name_out = NULL;
		*value_out = conf_strip_str(str);
	} else {
		*sep = 0;
		*name_out = conf_strip_str(str);
		*value_out = conf_strip_str(sep + 1);
	}

	pb_log("%s: @%s@%s@\n", __func__, *name_out, *value_out);

	return end ? end + 1 : NULL;
}

/**
 * conf_param_in_list - Search a list of strings for an entry.
 * @list: A NULL treminated array of pointers to strings.
 * @param: A string to search for.
 *
 * Retuns 1 if @param is found in @list, 0 if @param is not found.
 */

int conf_param_in_list(const char *const *list, const char *param)
{
	const char *const *str;

	for (str = list; *str; str++)
		if (streq(*str, param))
			return 1;
	return 0;
}

/**
 * conf_init_global_options - Zero the global option table.
 */

void conf_init_global_options(struct conf_context *conf)
{
	int i;

	for (i = 0; conf->global_options[i].name; i++)
		conf->global_options[i].value = NULL;
}

/**
 * conf_set_global_option - Set a value in the global option table.
 *
 * Check if an option (name=value) is a global option. If so, store it in
 * the global options table, and return 1. Otherwise, return 0.
 */

int conf_set_global_option(struct conf_context *conf, const char *name,
	const char *value)
{
	int i;

	for (i = 0; conf->global_options[i].name; i++) {
		if (streq(name, conf->global_options[i].name)) {
			conf->global_options[i].value
				= talloc_strdup(conf, value);
			pb_log("%s: @%s@%s@\n", __func__, name, value);
			return 1;
		}
	}
	return 0;
}

/**
 * conf_get_global_option - Get a value from the global option table.
 * @conf: The parser struct conf_context.
 * @name: The name of the (name:value) to retrieve.
 *
 * Returns the value if @name is found in the table, or NULL if @name
 * is not found.
 */

const char *conf_get_global_option(struct conf_context *conf,
	const char *name)
{
	int i;

	for (i = 0; conf->global_options[i].name ;i++)
		if (streq(name, conf->global_options[i].name)) {
			pb_log("%s: @%s@%s@\n", __func__, name,
				conf->global_options[i].value);
			return conf->global_options[i].value;
		}

	assert(0 && "unknown global name");
	return NULL;
}

/**
 * conf_parse_buf - The main parser loop.
 *
 * Called from conf_parse() with data read from a conf file.
 */

static void conf_parse_buf(struct conf_context *conf)
{
	char *pos, *name, *value;

	for (pos = conf->buf; pos;) {
		pos = conf_get_param_pair(pos, &name, &value, '\n');

		if (!value)
			continue;

		if (name && *name == '#')
			continue;

		if (*value == '#')
			continue;

		conf->process_pair(conf, name, value);
	}

	if (conf->finish)
		conf->finish(conf);
}

/**
 * conf_parse - The common parser entry.
 *
 * Called from the parser specific setup routines.  Searches for .conf
 * files, reads data into buffers, and calls conf_parse_buf().
 */

int conf_parse(struct conf_context *conf)
{
	int fd, rc;
	unsigned int i;
	struct stat stat;
	ssize_t len;

	rc = 0;
	fd = -1;
	len = 0;

	/* The parser is only run on the first file found. */
	/* FIXME: Could try others on error, etc. */

	for (i = 0; conf->conf_files[i]; i++) {
		char *filepath = resolve_path(conf->dc,
			conf->conf_files[i], conf->dc->device_path);

		pb_log("%s: try: %s\n", __func__, filepath);

		fd = open(filepath, O_RDONLY);

		talloc_free(filepath);

		if (fd < 0) {
			pb_log("%s: open failed: %s\n", __func__,
				strerror(errno));
			continue;
		}

		if (fstat(fd, &stat)) {
			pb_log("%s: fstat failed: %s\n", __func__,
				strerror(errno));
			continue;
		}

		conf->buf = talloc_array(conf, char, stat.st_size + 1);

		len = read(fd, conf->buf, stat.st_size);

		if (len < 0) {
			pb_log("%s: read failed: %s\n", __func__,
				strerror(errno));
			continue;
		}
		conf->buf[len] = 0;

		break;
	}

	if (fd >= 0)
		close(fd);

	if (len <= 0)
		goto out;

	if (!conf->dc->device->icon_file)
		conf->dc->device->icon_file = talloc_strdup(conf->dc,
			generic_icon_file(guess_device_type(conf->dc)));

	conf_parse_buf(conf);

	rc = 1;

out:
	pb_log("%s: %s\n", __func__, (rc ? "ok" : "failed"));
	return rc;
}

