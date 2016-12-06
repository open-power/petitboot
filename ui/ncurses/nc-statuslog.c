/*
 *  Copyright (C) 2016 IBM Corporation
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
#include "nc-statuslog.h"

static const int max_status_entry = 10000;

struct statuslog_entry {
	struct status		*status;
	struct list_item	list;
};

struct statuslog {
	struct list		status;
	int			n_status;
	bool			truncated;
};

struct statuslog_screen {
	struct text_screen text_scr;
};

struct statuslog *statuslog_init(struct cui *cui)
{
	struct statuslog *sl;

	sl = talloc(cui, struct statuslog);
	sl->truncated = false;
	sl->n_status = 0;
	list_init(&sl->status);

	return sl;
}

void statuslog_append_steal(struct cui *cui, struct statuslog *statuslog,
		struct status *status)
{
	struct statuslog_entry *entry;

	entry = talloc(statuslog, struct statuslog_entry);
	entry->status = status;
	talloc_steal(statuslog, status);

	list_add_tail(&statuslog->status, &entry->list);

	if (statuslog->n_status >= max_status_entry) {
		list_remove(&statuslog->status.head);
		statuslog->truncated = true;
		statuslog->n_status--;
	}

	statuslog->n_status++;

	if (cui->statuslog_screen) {
		text_screen_append_line(&cui->statuslog_screen->text_scr,
				"%s", status->message);
		text_screen_draw(&cui->statuslog_screen->text_scr);
	}
}

struct statuslog_screen *statuslog_screen_init(struct cui *cui,
		void (*on_exit)(struct cui *))
{
	struct statuslog_screen *screen;
	struct statuslog_entry *entry;
	const char *title;

	screen = talloc_zero(cui, struct statuslog_screen);

	title = _("Petitboot status log");

	text_screen_init(&screen->text_scr, cui, title, on_exit);
	list_for_each_entry(&cui->statuslog->status, entry, list) {
		text_screen_append_line(&screen->text_scr, "%s",
				entry->status->message);
	}
	text_screen_draw(&screen->text_scr);

	return screen;
}

struct nc_scr *statuslog_screen_scr(struct statuslog_screen *screen)
{
	return text_screen_scr(&screen->text_scr);
}
