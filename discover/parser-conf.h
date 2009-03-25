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

#if !defined(_PB_DISCOVER_CONFIG_H)
#define _PB_DISCOVER_CONFIG_H

#include "device-handler.h"

struct conf_global_option {
	const char *name;
	char *value;
};

struct conf_context {
	void *parser_info;
	struct discover_context *dc;
	char *buf;
	struct conf_global_option *global_options;
	const char *const *conf_files;

	void (*process_pair)(struct conf_context *conf, const char *name,
		char *value);
	void (*finish)(struct conf_context *conf);
};

int conf_parse(struct conf_context *conf);
char *conf_get_param_pair(char *str, char **name_out, char **value_out,
		char terminator);
const char *conf_get_global_option(struct conf_context *conf,
	const char *name);
int conf_set_global_option(struct conf_context *conf, const char *name,
	const char *value);

/* utility routines */

int conf_param_in_list(const char *const *list, const char *param);
char *conf_strip_str(char *s);

#endif
