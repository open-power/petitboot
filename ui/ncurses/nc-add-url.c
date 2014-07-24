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
#include "nc-add-url.h"
#include "nc-widgets.h"

#define N_FIELDS        5

extern const struct help_text add_url_help_text;

struct add_url_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_widgetset	*widgetset;

	bool			exit;
	bool			show_help;
	void			(*on_exit)(struct cui *);

	int			label_x;
	int			field_x;

	struct {
		struct nc_widget_textbox	*url_f;
		struct nc_widget_label		*url_l;

		struct nc_widget_button         *ok_b;
		struct nc_widget_button         *help_b;
		struct nc_widget_button         *cancel_b;
	} widgets;
};

static struct add_url_screen *add_url_screen_from_scr(struct nc_scr *scr)
{
	struct add_url_screen *add_url_screen;

	assert(scr->sig == pb_add_url_screen_sig);
	add_url_screen = (struct add_url_screen *)
		((char *)scr - (size_t)&((struct add_url_screen *)0)->scr);
	assert(add_url_screen->scr.sig == pb_add_url_screen_sig);
	return add_url_screen;
}

static void add_url_screen_process_key(struct nc_scr *scr, int key)
{
	struct add_url_screen *screen = add_url_screen_from_scr(scr);
	bool handled;

	handled = widgetset_process_key(screen->widgetset, key);

	if (!handled) {
		switch (key) {
		case 'x':
		case 27: /* esc */
			screen->exit = true;
			break;
		case 'h':
			screen->show_help = true;
			break;
		}
	}

	if (screen->exit) {
		screen->on_exit(screen->cui);

	} else if (screen->show_help) {
		screen->show_help = false;
		cui_show_help(screen->cui, _("Retrieve Config"),
			&add_url_help_text);

	} else if (handled) {
		wrefresh(screen->scr.main_ncw);
	}
}

static int add_url_screen_post(struct nc_scr *scr)
{
	struct add_url_screen *screen = add_url_screen_from_scr(scr);
	widgetset_post(screen->widgetset);
	nc_scr_frame_draw(scr);
	redrawwin(scr->main_ncw);
	wrefresh(screen->scr.main_ncw);
	return 0;
}

static int add_url_screen_unpost(struct nc_scr *scr)
{
	struct add_url_screen *screen = add_url_screen_from_scr(scr);
	widgetset_unpost(screen->widgetset);
	return 0;
}

struct nc_scr *add_url_screen_scr(struct add_url_screen *screen)
{
	return &screen->scr;
}

static int screen_process_form(struct add_url_screen *screen)
{
	char *url;
	int rc;

	url = widget_textbox_get_value(screen->widgets.url_f);
	if (!url || !strlen(url))
		return 0;

	/* Once we have all the info we need, tell the server */
	rc = cui_send_url(screen->cui, url);

	if (rc)
		pb_log("cui_send_retreive failed!\n");
	else
		pb_debug("add_url url sent!\n");
	return 0;
}

static void ok_click(void *arg)
{
	struct add_url_screen *screen = arg;
	if (screen_process_form(screen))
		/* errors are written to the status line, so we'll need
		 * to refresh */
		wrefresh(screen->scr.main_ncw);
	else
		screen->exit = true;
}

static void help_click(void *arg)
{
	struct add_url_screen *screen = arg;
	screen->show_help = true;
}

static void cancel_click(void *arg)
{
	struct add_url_screen *screen = arg;
	screen->exit = true;
}

static int layout_pair(struct add_url_screen *screen, int y,
	struct nc_widget_label *label,
	struct nc_widget *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	widget_move(label_w, y, screen->label_x);
	widget_move(field, y, screen->field_x);
	return max(widget_height(label_w), widget_height(field));
}

static void add_url_screen_layout_widgets(struct add_url_screen *screen)
{
	int y = 2;

	/* url field */
	y += layout_pair(screen, y, screen->widgets.url_l,
			 widget_textbox_base(screen->widgets.url_f));

	/* ok, help, cancel */
	y += 1;

	widget_move(widget_button_base(screen->widgets.ok_b),
		y, screen->field_x);
	widget_move(widget_button_base(screen->widgets.help_b),
		y, screen->field_x + 10);
	widget_move(widget_button_base(screen->widgets.cancel_b),
		y, screen->field_x + 20);
}

static void add_url_screen_setup_widgets(struct add_url_screen *screen)
{
	struct nc_widgetset *set = screen->widgetset;

	build_assert(sizeof(screen->widgets) / sizeof(struct widget *)
			== N_FIELDS);

	screen->widgets.url_l = widget_new_label(set, 0, 0,
			_("Configuration URL:"));
	screen->widgets.url_f = widget_new_textbox(set, 0, 0, 50, NULL);

	screen->widgets.ok_b = widget_new_button(set, 0, 0, 6, _("OK"),
			ok_click, screen);
	screen->widgets.help_b = widget_new_button(set, 0, 0, 6, _("Help"),
			help_click, screen);
	screen->widgets.cancel_b = widget_new_button(set, 0, 0, 6, _("Cancel"),
			cancel_click, screen);
}

struct add_url_screen *add_url_screen_init(struct cui *cui,
		void (*on_exit)(struct cui *))
{
	struct add_url_screen *screen;

	screen = talloc_zero(cui, struct add_url_screen);

	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->label_x = 2;
	screen->field_x = 22;

	nc_scr_init(&screen->scr, pb_add_url_screen_sig, 0,
		cui, add_url_screen_process_key,
		add_url_screen_post, add_url_screen_unpost,
		NULL);

	screen->scr.frame.ltitle = talloc_strdup(screen,
			_("Petitboot Config Retrieval"));
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen,
			_("tab=next, shift+tab=previous, x=exit, h=help"));
	nc_scr_frame_draw(&screen->scr);

	screen->widgetset = widgetset_create(screen, screen->scr.main_ncw,
			NULL);

	add_url_screen_setup_widgets(screen);
	add_url_screen_layout_widgets(screen);

	return screen;
}

