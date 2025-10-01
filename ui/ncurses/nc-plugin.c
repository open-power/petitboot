/*
 *  Copyright (C) 2017 IBM Corporation
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
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <i18n/i18n.h>
#include <log/log.h>

#include "nc-cui.h"
#include "nc-plugin.h"
#include "nc-widgets.h"
#include "nc-menu.h"
#include "ui/common/discover-client.h"
#include "process/process.h"
#include "system/system.h"

#define N_FIELDS        15

extern const struct help_text plugin_help_text;

static void plugin_run_command(void *arg);

struct plugin_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_widgetset	*widgetset;
	WINDOW			*pad;

	bool			exit;
	bool			show_help;
	bool			show_auth_run;
	bool			need_redraw;
	void			(*on_exit)(struct cui *);

	int			label_x;
	int			field_x;
	int			scroll_y;

	const struct plugin_option	*opt;

	struct {
		struct nc_widget_label		*id_l;
		struct nc_widget_label		*id_f;
		struct nc_widget_label		*name_l;
		struct nc_widget_label		*name_f;
		struct nc_widget_label		*vendor_l;
		struct nc_widget_label		*vendor_f;
		struct nc_widget_label		*vendor_id_l;
		struct nc_widget_label		*vendor_id_f;
		struct nc_widget_label		*version_l;
		struct nc_widget_label		*version_f;
		struct nc_widget_label		*date_l;
		struct nc_widget_label		*date_f;

		struct nc_widget_label		*commands_l;
		struct nc_widget_select		*commands_f;

		struct nc_widget_button		*run_b;
	} widgets;
};

static void plugin_screen_draw(struct plugin_screen *screen,
		struct pmenu_item *item);

static struct plugin_screen *plugin_screen_from_scr(struct nc_scr *scr)
{
	struct plugin_screen *plugin_screen;

	assert(scr->sig == pb_plugin_screen_sig);
	plugin_screen = (struct plugin_screen *)
		((char *)scr - (size_t)&((struct plugin_screen *)0)->scr);
	assert(plugin_screen->scr.sig == pb_plugin_screen_sig);
	return plugin_screen;
}

struct nc_scr *plugin_screen_scr(struct plugin_screen *screen)
{
	return &screen->scr;
}

static void pad_refresh(struct plugin_screen *screen)
{
	int y, x, rows, cols;

	getmaxyx(screen->scr.sub_ncw, rows, cols);
	getbegyx(screen->scr.sub_ncw, y, x);

	prefresh(screen->pad, screen->scroll_y, 0, y, x, rows, cols);
}

static void plugin_screen_widget_focus(struct nc_widget *widget, void *arg)
{
	struct plugin_screen *screen = arg;
	int w_y, w_height, w_focus, s_max, adjust;

	w_height = widget_height(widget);
	w_focus = widget_focus_y(widget);
	w_y = widget_y(widget) + w_focus;
	s_max = getmaxy(screen->scr.sub_ncw) - 1;

	if (w_y < screen->scroll_y)
		screen->scroll_y = w_y;

	else if (w_y + screen->scroll_y + 1 > s_max) {
		/* Fit as much of the widget into the screen as possible */
		adjust = min(s_max - 1, w_height - w_focus);
		if (w_y + adjust >= screen->scroll_y + s_max)
			screen->scroll_y = max(0, 1 + w_y + adjust - s_max);
	} else
		return;

	pad_refresh(screen);
}

static void plugin_screen_process_key(struct nc_scr *scr, int key)
{
	struct plugin_screen *screen = plugin_screen_from_scr(scr);
	bool handled;

	handled = widgetset_process_key(screen->widgetset, key);

	if (!handled) {
		pb_log("Not handled by widgetset\n");
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
		screen->need_redraw = true;
		cui_show_help(screen->cui, _("Petitboot Plugin"),
				&plugin_help_text);

	} else if (handled && (screen->cui->current == scr)) {
		pad_refresh(screen);
	}
}

static int plugin_screen_post(struct nc_scr *scr)
{
	struct plugin_screen *screen = plugin_screen_from_scr(scr);

	widgetset_post(screen->widgetset);

	nc_scr_frame_draw(scr);
	if (screen->need_redraw) {
		redrawwin(scr->main_ncw);
		screen->need_redraw = false;
	}
	wrefresh(screen->scr.main_ncw);
	pad_refresh(screen);

	if (screen->show_auth_run) {
		screen->show_auth_run = false;
		plugin_run_command(screen);
	}

	return 0;
}

static int plugin_screen_unpost(struct nc_scr *scr)
{
	widgetset_unpost(plugin_screen_from_scr(scr)->widgetset);
	return 0;
}

static void plugin_screen_resize(struct nc_scr *scr)
{
	/* FIXME: forms can't be resized, need to recreate here */
	plugin_screen_unpost(scr);
	plugin_screen_post(scr);
}

static void plugin_run_command(void *arg)
{
	struct plugin_screen *screen = arg;
	char *cmd;
	int i, result;

	i = widget_select_get_value(screen->widgets.commands_f);
	/* pb-plugin copies all executables to the wrapper directory */
	cmd = talloc_asprintf(screen, "%s/%s", "/var/lib/pb-plugins/bin",
			basename(screen->opt->executables[i]));

	if (!cmd) {
		pb_log("nc-plugin: plugin option has missing command %d\n", i);
		return;
	}

	const char *argv[] = {
		pb_system_apps.pb_exec,
		cmd,
		NULL
	};

	/* Drop our pad before running plugin */
	delwin(screen->pad);
	screen->pad = NULL;

	result = cui_run_cmd(screen->cui, argv);

	if (result)
		pb_log("Failed to run plugin command %s\n", cmd);
	else
		nc_scr_status_printf(screen->cui->current, _("Finished: %s"), cmd);

	plugin_screen_draw(screen, NULL);

	talloc_free(cmd);
}

static void plugin_run_command_check(void *arg)
{
	struct plugin_screen *screen = arg;

	if (discover_client_authenticated(screen->cui->client)) {
		plugin_run_command(screen);
		return;
	}

	/*
	 * Don't supply a callback as we want to handle running the command
	 * from the plugin screen.
	 */
	screen->show_auth_run = true;
	cui_show_auth(screen->cui, screen->scr.main_ncw, false, NULL);
}

static void plugin_screen_setup_widgets(struct plugin_screen *screen)
{
	const struct plugin_option *opt = screen->opt;
	struct nc_widgetset *set = screen->widgetset;
	unsigned int i;

	build_assert(sizeof(screen->widgets) / sizeof(struct widget *)
			== N_FIELDS);

	screen->widgets.id_l = widget_new_label(set, 0, 0, _("ID:"));
	screen->widgets.id_f = widget_new_label(set, 0, 0, opt->id);
	screen->widgets.name_l = widget_new_label(set, 0, 0, _("Name:"));
	screen->widgets.name_f = widget_new_label(set, 0, 0, opt->name);
	screen->widgets.vendor_l = widget_new_label(set, 0, 0, _("Vendor:"));
	screen->widgets.vendor_f = widget_new_label(set, 0, 0, opt->vendor);
	screen->widgets.vendor_id_l = widget_new_label(set, 0, 0,
						       _("Vendor ID:"));
	screen->widgets.vendor_id_f = widget_new_label(set, 0, 0,
							opt->vendor_id);
	screen->widgets.version_l = widget_new_label(set, 0, 0, _("Version:"));
	screen->widgets.version_f = widget_new_label(set, 0, 0,
							opt->version);
	screen->widgets.date_l = widget_new_label(set, 0, 0, _("Date"));
	screen->widgets.date_f = widget_new_label(set, 0, 0, opt->date);

	screen->widgets.commands_l = widget_new_label(set, 0, 0,
							 _("Commands:"));
	screen->widgets.commands_f = widget_new_select(set, 0, 0,
			COLS - screen->field_x - 1);
	for (i = 0; i < opt->n_executables; i++) {
		widget_select_add_option(screen->widgets.commands_f, i,
				basename(opt->executables[i]), i == 0);
	}

	screen->widgets.run_b = widget_new_button(set, 0, 0, 30,
			_("Run selected command"), plugin_run_command_check, screen);
}

static int layout_pair(struct plugin_screen *screen, int y,
		struct nc_widget_label *label,
		struct nc_widget *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	widget_move(label_w, y, screen->label_x);
	widget_move(field, y, screen->field_x);
	return max(widget_height(label_w), widget_height(field));
}

static void plugin_screen_layout_widgets(struct plugin_screen *screen)
{
	unsigned int y;

	/* list of details (static) */

	y = 1;

	layout_pair(screen, y++, screen->widgets.id_l,
			widget_label_base(screen->widgets.id_f));
	layout_pair(screen, y++, screen->widgets.name_l,
			widget_label_base(screen->widgets.name_f));
	layout_pair(screen, y++, screen->widgets.vendor_l,
			widget_label_base(screen->widgets.vendor_f));
	layout_pair(screen, y++, screen->widgets.vendor_id_l,
			widget_label_base(screen->widgets.vendor_id_f));
	layout_pair(screen, y++, screen->widgets.version_l,
			widget_label_base(screen->widgets.version_f));
	layout_pair(screen, y++, screen->widgets.date_l,
			widget_label_base(screen->widgets.date_f));

	y += 1;

	/* available commands */
	widget_move(widget_label_base(screen->widgets.commands_l), y,
		    screen->label_x);
	widget_move(widget_select_base(screen->widgets.commands_f), y,
			screen->field_x);

	y += 2;

	widget_move(widget_button_base(screen->widgets.run_b), y++, screen->field_x);

}

static void plugin_screen_draw(struct plugin_screen *screen,
		struct pmenu_item *item)
{
	struct cui_opt_data *cod;
	int height = N_FIELDS * 2;
	bool repost = false;

	if (item) {
		/* First init or update */
		cod = cod_from_item(item);
		screen->opt = cod->pd->opt;
	}

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
			plugin_screen_widget_focus, screen);

	plugin_screen_setup_widgets(screen);
	plugin_screen_layout_widgets(screen);

	if (repost)
		widgetset_post(screen->widgetset);
}

static int plugin_screen_destroy(void *arg)
{
	struct plugin_screen *screen = arg;
	if (screen->pad)
		delwin(screen->pad);
	return 0;
}

struct plugin_screen *plugin_screen_init(struct cui *cui,
		struct pmenu_item *item,
		void (*on_exit)(struct cui *))
{
	struct plugin_screen *screen = talloc_zero(cui, struct plugin_screen);
	talloc_set_destructor(screen, plugin_screen_destroy);

	nc_scr_init(&screen->scr, pb_plugin_screen_sig, 0,
			cui, plugin_screen_process_key,
			plugin_screen_post, plugin_screen_unpost,
			plugin_screen_resize);


	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->label_x = 2;
	screen->field_x = 25;

	screen->scr.frame.ltitle = talloc_strdup(screen,
			_("Petitboot Plugin"));
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen,
			_("tab=next, shift+tab=previous, x=exit, h=help"));
	nc_scr_frame_draw(&screen->scr);

	scrollok(screen->scr.sub_ncw, true);

	plugin_screen_draw(screen, item);

	return screen;
}

