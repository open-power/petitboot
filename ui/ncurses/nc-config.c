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

#define _GNU_SOURCE


#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>

#include "config.h"
#include "nc-cui.h"
#include "nc-config.h"
#include "nc-widgets.h"

#define N_FIELDS	9

struct config_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_widgetset	*widgetset;
	bool			exit;
	void			(*on_exit)(struct cui *);

	int			label_x;
	int			field_x;

	struct {
		struct nc_widget_checkbox	*autoboot_f;
		struct nc_widget_label		*autoboot_l;
		struct nc_widget_textbox	*timeout_f;
		struct nc_widget_label		*timeout_l;

		struct nc_widget_label		*network_l;
		struct nc_widget_label		*iface_l;
		struct nc_widget_select		*iface_f;

		struct nc_widget_button		*ok_b;
		struct nc_widget_button		*cancel_b;
	} widgets;
};

static struct config_screen *config_screen_from_scr(struct nc_scr *scr)
{
	struct config_screen *config_screen;

	assert(scr->sig == pb_config_screen_sig);
	config_screen = (struct config_screen *)
		((char *)scr - (size_t)&((struct config_screen *)0)->scr);
	assert(config_screen->scr.sig == pb_config_screen_sig);
	return config_screen;
}

static void config_screen_process_key(struct nc_scr *scr, int key)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	bool handled;

	handled = widgetset_process_key(screen->widgetset, key);
	if (screen->exit)
		screen->on_exit(screen->cui);
	else if (handled)
		wrefresh(screen->scr.main_ncw);
}

static void config_screen_resize(struct nc_scr *scr)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	(void)screen;
}

static int config_screen_post(struct nc_scr *scr)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	widgetset_post(screen->widgetset);
	nc_scr_frame_draw(scr);
	wrefresh(scr->main_ncw);
	return 0;
}

static int config_screen_unpost(struct nc_scr *scr)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	widgetset_unpost(screen->widgetset);
	return 0;
}

struct nc_scr *config_screen_scr(struct config_screen *screen)
{
	return &screen->scr;
}

static void ok_click(void *arg)
{
	struct config_screen *screen = arg;
	/* todo: save config */
	screen->on_exit(screen->cui);
}

static void cancel_click(void *arg)
{
	struct config_screen *screen = arg;
	screen->exit = true;
}

static int layout_pair(struct config_screen *screen, int y,
		struct nc_widget_label *label,
		struct nc_widget *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	widget_move(label_w, y, screen->label_x);
	widget_move(field, y, screen->field_x);
	return max(widget_height(label_w), widget_height(field));
}

static void config_screen_layout_widgets(struct config_screen *screen)
{
	int y;

	y = 1;

	y += layout_pair(screen, y, screen->widgets.autoboot_l,
			widget_checkbox_base(screen->widgets.autoboot_f));

	y += layout_pair(screen, y, screen->widgets.timeout_l,
			widget_textbox_base(screen->widgets.timeout_f));

	y++;

	widget_move(widget_button_base(screen->widgets.ok_b),
			y, screen->field_x);
	widget_move(widget_button_base(screen->widgets.cancel_b),
			y, screen->field_x + 10);
}

static void config_screen_setup_widgets(struct config_screen *screen,
		const struct config *config,
		const struct system_info *sysinfo)
{
	struct nc_widgetset *set = screen->widgetset;
	char *str;

	(void)sysinfo;

	build_assert(sizeof(screen->widgets) / sizeof(struct widget *)
			== N_FIELDS);

	screen->widgets.autoboot_l = widget_new_label(set, 0, 0, "Autoboot:");
	screen->widgets.autoboot_f = widget_new_checkbox(set, 0, 0,
					config->autoboot_enabled);

	str = talloc_asprintf(screen, "%d", config->autoboot_timeout_sec);
	screen->widgets.timeout_l = widget_new_label(set, 0, 0, "Timeout:");
	screen->widgets.timeout_f = widget_new_textbox(set, 0, 0, 5, str);

	screen->widgets.ok_b = widget_new_button(set, 0, 0, 6, "OK",
			ok_click, screen);
	screen->widgets.cancel_b = widget_new_button(set, 0, 0, 6, "Cancel",
			cancel_click, screen);

}

struct config_screen *config_screen_init(struct cui *cui,
		const struct config *config,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *))
{
	struct config_screen *screen;

	screen = talloc_zero(cui, struct config_screen);
	nc_scr_init(&screen->scr, pb_config_screen_sig, 0,
			cui, config_screen_process_key,
			config_screen_post, config_screen_unpost,
			config_screen_resize);

	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->label_x = 2;
	screen->field_x = 16;

	screen->scr.frame.ltitle = talloc_strdup(screen,
			"Petitboot System Configuration");
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen,
			"tab=next, shift+tab=previous");
	nc_scr_frame_draw(&screen->scr);

	screen->widgetset = widgetset_create(screen, screen->scr.main_ncw,
			screen->scr.sub_ncw);
	config_screen_setup_widgets(screen, config, sysinfo);
	config_screen_layout_widgets(screen);

	wrefresh(screen->scr.main_ncw);
	scrollok(screen->scr.sub_ncw, true);

	return screen;
}
