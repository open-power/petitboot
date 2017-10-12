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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <i18n/i18n.h>
#include <log/log.h>

#include "nc-cui.h"
#include "nc-subset.h"

#define N_FIELDS        3

struct subset_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_scr 		*return_scr;
	struct nc_widgetset	*widgetset;
	WINDOW			*pad;
	struct nc_widget_subset *options;

	bool			exit;
	void			(*on_exit)(struct cui *);

	int			scroll_y;

	int			label_x;
	int			field_x;

	struct {
		struct nc_widget_select		*options_f;

		struct nc_widget_button		*ok_b;
		struct nc_widget_button         *cancel_b;
	} widgets;
};

struct nc_scr *subset_screen_return_scr(struct subset_screen *screen)
{
	return screen->return_scr;
}

void subset_screen_update(struct subset_screen *screen)
{
	pb_debug("Exiting subset due to update\n");
	return screen->on_exit(screen->cui);
}

static struct subset_screen *subset_screen_from_scr(struct nc_scr *scr)
{
	struct subset_screen *subset_screen;

	assert(scr->sig == pb_subset_screen_sig);
	subset_screen = (struct subset_screen *)
		((char *)scr - (size_t)&((struct subset_screen *)0)->scr);
	assert(subset_screen->scr.sig == pb_subset_screen_sig);
	return subset_screen;
}

static void pad_refresh(struct subset_screen *screen)
{
	int y, x, rows, cols;

	getmaxyx(screen->scr.sub_ncw, rows, cols);
	getbegyx(screen->scr.sub_ncw, y, x);

	prefresh(screen->pad, screen->scroll_y, 0, y, x, rows, cols);
}

static void subset_screen_process_key(struct nc_scr *scr, int key)
{
	struct subset_screen *screen = subset_screen_from_scr(scr);
	bool handled;

	handled = widgetset_process_key(screen->widgetset, key);

	if (!handled) {
		switch (key) {
		case 'x':
		case 27: /* esc */
			screen->exit = true;
			break;
		}
	}

	if (screen->exit)
		screen->on_exit(screen->cui);
	else if (handled)
		pad_refresh(screen);
}

static int subset_screen_post(struct nc_scr *scr)
{
	struct subset_screen *screen = subset_screen_from_scr(scr);
	widgetset_post(screen->widgetset);
	nc_scr_frame_draw(scr);
	redrawwin(scr->main_ncw);
	wrefresh(scr->main_ncw);
	pad_refresh(screen);
	return 0;
}

static int subset_screen_unpost(struct nc_scr *scr)
{
	struct subset_screen *screen = subset_screen_from_scr(scr);
	widgetset_unpost(screen->widgetset);
	return 0;
}

struct nc_scr *subset_screen_scr(struct subset_screen *screen)
{
	return &screen->scr;
}

static void ok_click(void *arg)
{
	struct subset_screen *screen = arg;
	int idx = widget_select_get_value(screen->widgets.options_f);
	widget_subset_callback(screen->return_scr, screen->options, idx);
	screen->exit = true;
}

static void cancel_click(void *arg)
{
	struct subset_screen *screen = arg;
	screen->exit = true;
}

static void subset_screen_layout_widgets(struct subset_screen *screen)
{
	int y = 1;

	/* select */
	widget_move(widget_select_base(screen->widgets.options_f),
		y, screen->label_x);
	y+= widget_height(widget_select_base(screen->widgets.options_f));

	/* ok, cancel */
	y += 1;

	widget_move(widget_button_base(screen->widgets.ok_b),
		y, screen->field_x);
	widget_move(widget_button_base(screen->widgets.cancel_b),
		y, screen->field_x + 14);
}

static void subset_screen_option_select(void *arg, int value)
{
	struct subset_screen *screen = arg;
	widgetset_unpost(screen->widgetset);
	subset_screen_layout_widgets(screen);
	widgetset_post(screen->widgetset);
	(void)value;
}

static void subset_screen_setup_widgets(struct subset_screen *screen)
{
	struct nc_widgetset *set = screen->widgetset;
	struct nc_widget_subset *subset = screen->options;

	build_assert(sizeof(screen->widgets) / sizeof(struct widget *)
			== N_FIELDS);

	screen->widgets.options_f = widget_new_select(set, 0, 0,
			COLS - (2 * screen->label_x));

	widget_select_on_change(screen->widgets.options_f,
			subset_screen_option_select, screen);

	widget_subset_show_inactive(subset, screen->widgets.options_f);

	screen->widgets.ok_b = widget_new_button(set, 0, 0, 10, _("OK"),
			ok_click, screen);
	screen->widgets.cancel_b = widget_new_button(set, 0, 0, 10, _("Cancel"),
			cancel_click, screen);
}

static void subset_screen_widget_focus(struct nc_widget *widget, void *arg)
{
	struct subset_screen *screen = arg;
	int w_y, s_max;

	w_y = widget_y(widget) + widget_focus_y(widget);
	s_max = getmaxy(screen->scr.sub_ncw) - 1;

	if (w_y < screen->scroll_y)
		screen->scroll_y = w_y;

	else if (w_y + screen->scroll_y + 1 > s_max)
		screen->scroll_y = 1 + w_y - s_max;

	else
		return;

	pad_refresh(screen);
}

static void subset_screen_draw(struct subset_screen *screen)
{
	bool repost = false;
	int height;

	/*
	 * Size of pad = top space + 2 * number of available options in case
	 * device names wrap
	 */
	height = 1 + N_FIELDS + widget_subset_n_inactive(screen->options) * 2;

	if (!screen->pad || getmaxy(screen->pad) < height) {
		if (screen->pad)
			delwin(screen->pad);
		screen->pad = newpad(height, COLS);
	}

	if (screen->widgetset) {
		widgetset_unpost(screen->widgetset);
		talloc_free(screen->widgetset);
		repost = true;
	}

	screen->widgetset = widgetset_create(screen, screen->scr.main_ncw,
			screen->pad);
	widgetset_set_widget_focus(screen->widgetset,
			subset_screen_widget_focus, screen);

	subset_screen_setup_widgets(screen);
	subset_screen_layout_widgets(screen);

	if (repost)
		widgetset_post(screen->widgetset);
}

static int subset_screen_destroy(void *arg)
{
	struct subset_screen *screen = arg;
	if (screen->pad)
		delwin(screen->pad);
	return 0;
}

struct subset_screen *subset_screen_init(struct cui *cui,
		struct nc_scr *current_scr,
		const char *title_suffix,
		void *subset,
		void (*on_exit)(struct cui *))
{
	struct subset_screen *screen;

	screen = talloc_zero(cui, struct subset_screen);
	talloc_set_destructor(screen, subset_screen_destroy);

	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->options = (struct nc_widget_subset *) subset;
	screen->label_x = 8;
	screen->field_x = 8;

	screen->return_scr = current_scr;

	nc_scr_init(&screen->scr, pb_subset_screen_sig, 0,
		cui, subset_screen_process_key,
		subset_screen_post, subset_screen_unpost,
		NULL);

	screen->scr.frame.ltitle = talloc_strdup(screen,
			title_suffix);
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen,
			_("tab=next, shift+tab=previous, x=exit"));

	scrollok(screen->scr.sub_ncw, true);

	subset_screen_draw(screen);

	return screen;
}
