/*
 *  Copyright (C) 2013 IBM Corporation
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

#ifndef _NC_TEXTSCREEN_H
#define _NC_TEXTSCREEN_H

#include "types/types.h"
#include "nc-cui.h"

struct text_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	const char		**lines;
	int			n_lines;
	int			n_alloc_lines;
	int			scroll_y;
	const char		*help_title;
	const struct help_text	*help_text;
	void			(*on_exit)(struct cui *);
};

void text_screen_init(struct text_screen *screen, struct cui *cui,
		const char *title, void (*on_exit)(struct cui *));

struct text_screen *text_screen_from_scr(struct nc_scr *scr);
struct nc_scr *text_screen_scr(struct text_screen *screen);

/* content modification */
void text_screen_clear(struct text_screen *screen);
void text_screen_append_line(struct text_screen *screen,
		const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void text_screen_set_text(struct text_screen *screen, const char *text);
void text_screen_set_help(struct text_screen *screen, const char *title,
		const struct help_text *text);

/* interaction */
void text_screen_process_key(struct nc_scr *scr, int key);
void text_screen_draw(struct text_screen *screen);


#endif /* defined _NC_TEXTSCREEN_H */
