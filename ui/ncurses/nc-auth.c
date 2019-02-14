/*
 *  Copyright (C) 2018 IBM Corporation
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
#include "nc-widgets.h"
#include "nc-auth.h"

#define N_FIELDS        5

struct auth_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_scr 		*return_scr;
	struct nc_widgetset 	*widgetset;
	void 			(*process_key)(struct nc_scr *, int);

	bool			set_password;
	const struct device	*dev;
	void			(*callback)(struct nc_scr *);
	int			offset_y;
	int			label_x;
	int			field_x;

	bool			exit;
	void			(*on_exit)(struct cui *);

	struct {
		struct nc_widget_label		*title_a_l;
		struct nc_widget_label		*title_b_l;
		struct nc_widget_textbox	*password_f;
		struct nc_widget_label		*new_l;
		struct nc_widget_textbox	*new_f;
		struct nc_widget_button		*ok_b;
		struct nc_widget_button		*cancel_b;
	} widgets;
};

struct nc_scr *auth_screen_return_scr(struct auth_screen *screen)
{
	return screen->return_scr;
}

struct nc_scr *auth_screen_scr(struct auth_screen *screen)
{
	return &screen->scr;
}

static struct auth_screen *auth_screen_from_scr(struct nc_scr *scr)
{
	struct auth_screen *auth_screen;

	assert(scr->sig == pb_auth_screen_sig);
	auth_screen = (struct auth_screen *)
		((char *)scr - (size_t)&((struct auth_screen *)0)->scr);
	assert(auth_screen->scr.sig == pb_auth_screen_sig);
	return auth_screen;
}

static void auth_screen_process_key(struct nc_scr *scr, int key)
{
	struct auth_screen *screen = auth_screen_from_scr(scr);
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
		wrefresh(screen->scr.sub_ncw);
}

static void auth_screen_frame_draw(struct nc_scr *scr)
{
	int y, x;

	getmaxyx(scr->sub_ncw, y, x);

	mvwhline(scr->sub_ncw, 0, 0, ACS_HLINE, x);
	mvwhline(scr->sub_ncw, y - 1, 0, ACS_HLINE, x);

	mvwvline(scr->sub_ncw, 0, 0, ACS_VLINE, y);
	mvwvline(scr->sub_ncw, 0, x - 1, ACS_VLINE, y);
}

static int auth_screen_post(struct nc_scr *scr)
{
	struct auth_screen *screen = auth_screen_from_scr(scr);
	widgetset_post(screen->widgetset);
	auth_screen_frame_draw(scr);
	wrefresh(scr->sub_ncw);
	return 0;
}

static int auth_screen_unpost(struct nc_scr *scr)
{
	struct auth_screen *screen = auth_screen_from_scr(scr);
	widgetset_unpost(screen->widgetset);
	return 0;
}

static void ok_click(void *arg)
{
	struct auth_screen *screen = arg;
	char *password, *new_password;
	int rc;


	password = widget_textbox_get_value(screen->widgets.password_f);
	if (screen->set_password) {
		new_password = widget_textbox_get_value(screen->widgets.new_f);
		rc = cui_send_set_password(screen->cui, password, new_password);
	} else if (screen->dev) {
		rc = cui_send_open_luks_device(screen->cui, password,
				screen->dev->id);
	} else
		rc = cui_send_authenticate(screen->cui, password);

	if (rc)
		pb_log("Failed to send authenticate action\n");
	else if (screen->callback)
		screen->callback(screen->return_scr);

	screen->exit = true;
}

static void cancel_click(void *arg)
{
	struct auth_screen *screen = arg;
	screen->exit = true;
}

static void auth_screen_layout_widgets(struct auth_screen *screen)
{
	int y = 1;

	widget_move(widget_label_base(screen->widgets.title_a_l),
			y++, screen->label_x);
	widget_move(widget_label_base(screen->widgets.title_b_l),
			y++, screen->label_x);

	y += 1;

	widget_move(widget_textbox_base(screen->widgets.password_f),
			y++, screen->field_x);

	y += 1;

	if (screen->set_password) {
		widget_move(widget_label_base(screen->widgets.new_l),
				y++, screen->label_x);
		widget_move(widget_textbox_base(screen->widgets.new_f),
				y++, screen->field_x);
		y += 1;
	}

	widget_move(widget_button_base(screen->widgets.ok_b),
			y, 10);
	widget_move(widget_button_base(screen->widgets.cancel_b),
			y, 30);
}

static void auth_screen_draw(struct auth_screen *screen)
{
	struct nc_widgetset *set;
	char *label;

	set = widgetset_create(screen, screen->scr.main_ncw,
			screen->scr.sub_ncw);
	if (!set) {
		pb_log("%s: failed to create widgetset\n", __func__);
		return;
	}
	screen->widgetset = set;

	if (screen->dev) {
		label = talloc_asprintf(screen,
				_("Opening encrypted device %s"),
				screen->dev->id);
		screen->widgets.title_a_l = widget_new_label(set, 0, 0, label);
		screen->widgets.title_b_l = widget_new_label(set, 0, 0,
				_("Please enter the disk password."));
		talloc_free(label);
	} else {
		screen->widgets.title_a_l = widget_new_label(set, 0, 0,
				_("This action requires authorisation."));
		screen->widgets.title_b_l = widget_new_label(set, 0, 0,
				_("Please enter the system password."));
	}

	screen->widgets.password_f = widget_new_textbox_hidden(set, 0, 0,
			COLS - 20 - 20, "", true);

	if (screen->set_password) {
		screen->widgets.new_l = widget_new_label(set, 0, 0,
				_("New password:"));
		screen->widgets.new_f = widget_new_textbox_hidden(set, 0, 0,
				COLS - 20 - 20, "", true);
	}

	screen->widgets.ok_b = widget_new_button(set, 0, 0, 10, _("OK"),
			ok_click, screen);
	screen->widgets.cancel_b = widget_new_button(set, 0, 0, 10, _("Cancel"),
			cancel_click, screen);

	auth_screen_layout_widgets(screen);
}

static int auth_screen_destroy(void *arg)
{
	struct auth_screen *screen = arg;
	if (screen->scr.sub_ncw)
		delwin(screen->scr.sub_ncw);
	return 0;
}

struct auth_screen *auth_screen_init(struct cui *cui,
		WINDOW *parent, bool set_password,
		const struct device *dev,
		void (*callback)(struct nc_scr *),
		void (*on_exit)(struct cui *))
{
	struct auth_screen *screen = NULL;
	struct nc_scr *scr;
	int y, x;

	if (!cui || !parent)
		return NULL;

	if (set_password && dev) {
		pb_log_fn("Incorrect parameters (set_password and device)\n");
		return NULL;
	}

	screen = talloc_zero(cui, struct auth_screen);
	if (!screen)
		return NULL;
	talloc_set_destructor(screen, auth_screen_destroy);

	screen->cui = cui;
	screen->return_scr = cui->current;
	screen->set_password = set_password;
	screen->dev = dev;
	screen->callback = callback;
	screen->on_exit = on_exit;
	screen->label_x = 5;
	screen->field_x = 10;

	/*
	 * Manually init our nc_scr: we only want to create the subwin and
	 * 'inherit' the parent window.
	 */
	scr = &screen->scr;
	scr->sig = pb_auth_screen_sig;
	scr->ui_ctx = cui;
	scr->process_key = auth_screen_process_key;
	scr->post = auth_screen_post;
	scr->unpost = auth_screen_unpost;
	scr->resize = NULL;


	getbegyx(parent, y, x);
	/* Hold on to the real offset from the top of the screen */
	screen->offset_y = y + 5;
	(void)x;

	scr->main_ncw = parent;
	scr->sub_ncw = derwin(parent, set_password ? 15 : 10, COLS - 20,
			5, 10);	/* relative to parent origin */
	if (!scr->sub_ncw) {
		pb_log("Could not create subwin\n");
		goto err;
	}

	auth_screen_draw(screen);

	return screen;
err:
	pb_log("failed to create auth screen\n");
	if (screen) {
		if (screen->scr.sub_ncw)
			delwin(screen->scr.sub_ncw);
		talloc_free(screen);
	}
	return NULL;
}
