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

#ifndef _NC_HELPSCREEN_H
#define _NC_HELPSCREEN_H

struct help_screen;
struct cui;

/* Container struct for type-safety; we need to use gettext() before
 * displaying the untranslated string. */
struct help_text {
	const char *text;
};

#define define_help_text(s) { .text = s }

struct nc_scr *help_screen_scr(struct help_screen *screen);
struct nc_scr *help_screen_return_scr(struct help_screen *screen);

struct help_screen *help_screen_init(struct cui *cui,
		struct nc_scr *current_scr,
		const char *title_suffix,
		const struct help_text *text,
		void (*on_exit)(struct cui *));


#endif /* defined _NC_HELPSCREEN_H */
