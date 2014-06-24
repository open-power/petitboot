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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <util/util.h>
#include <i18n/i18n.h>

#include "nc-cui.h"
#include "nc-textscreen.h"
#include "nc-helpscreen.h"

struct help_screen {
	struct text_screen text_scr;
	struct nc_scr *return_scr;
};

struct nc_scr *help_screen_scr(struct help_screen *screen)
{
	return text_screen_scr(&screen->text_scr);
}

struct nc_scr *help_screen_return_scr(struct help_screen *screen)
{
	return screen->return_scr;
}

struct help_screen *help_screen_init(struct cui *cui,
		struct nc_scr *current_scr,
		const char *title_suffix,
		const struct help_text *text,
		void (*on_exit)(struct cui *))
{
	struct help_screen *screen;
	const char *title;

	screen = talloc_zero(cui, struct help_screen);
	screen->return_scr = current_scr;

	title = _("Petitboot help");
	if (title_suffix)
		title = talloc_asprintf(screen,
				_("Petitboot help: %s"), title_suffix);

	text_screen_init(&screen->text_scr, cui, title, on_exit);
	text_screen_set_text(&screen->text_scr, gettext(text->text));
	text_screen_draw(&screen->text_scr);

	return screen;
}
