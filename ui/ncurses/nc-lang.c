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

#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <i18n/i18n.h>
#include <pb-config/pb-config.h>

#include "ui/common/discover-client.h"
#include "nc-cui.h"
#include "nc-lang.h"
#include "nc-widgets.h"

#define N_FIELDS	7

static struct lang {
	const char	*name;
	const wchar_t	*label;
} languages[] = {
	{ "de_DE.utf8",	L"Deutsch"},
	{ "en_US.utf8",	L"English"},
	{ "es_ES.utf8",	L"Espa\u00f1ol"},
	{ "fr_FR.utf8",	L"Fran\u00e7ais"},
	{ "it_IT.utf8",	L"Italiano"},
	{ "ja_JP.utf8",	L"\u65e5\u672c\u8a9e"},
	{ "ko_KR.utf8",	L"\ud55c\uad6d\uc5b4"},
	{ "pt_BR.utf8",	L"Portugu\u00eas/Brasil"},
	{ "ru_RU.utf8",	L"\u0420\u0443\u0441\u0441\u043a\u0438\u0439"},
	{ "zh_CN.utf8",	L"\u7b80\u4f53\u4e2d\u6587"},
	{ "zh_TW.utf8",	L"\u7e41\u9ad4\u4e2d\u6587"},
};

struct lang_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_widgetset	*widgetset;
	WINDOW			*pad;

	bool			exit;
	void			(*on_exit)(struct cui *);

	int			scroll_y;

	int			label_x;
	int			field_x;

	struct {
		struct nc_widget_select		*lang_f;
		struct nc_widget_label		*lang_l;

		struct nc_widget_label		*save_l;
		struct nc_widget_checkbox	*save_cb;

		struct nc_widget_label		*safe_mode;
		struct nc_widget_button		*ok_b;
		struct nc_widget_button		*cancel_b;
	} widgets;
};

static struct lang_screen *lang_screen_from_scr(struct nc_scr *scr)
{
	struct lang_screen *lang_screen;

	assert(scr->sig == pb_lang_screen_sig);
	lang_screen = (struct lang_screen *)
		((char *)scr - (size_t)&((struct lang_screen *)0)->scr);
	assert(lang_screen->scr.sig == pb_lang_screen_sig);
	return lang_screen;
}

static void pad_refresh(struct lang_screen *screen)
{
	int y, x, rows, cols;

	getmaxyx(screen->scr.sub_ncw, rows, cols);
	getbegyx(screen->scr.sub_ncw, y, x);

	prefresh(screen->pad, screen->scroll_y, 0, y, x, rows, cols);
}

static void lang_screen_process_key(struct nc_scr *scr, int key)
{
	struct lang_screen *screen = lang_screen_from_scr(scr);
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

	if (screen->exit) {
		screen->on_exit(screen->cui);
	} else if (handled && (screen->cui->current == scr)) {
		pad_refresh(screen);
	}
}

static const char *lang_get_lang_name(struct lang_screen *screen)
{
	struct lang *lang;
	int idx;

	idx = widget_select_get_value(screen->widgets.lang_f);

	/* Option -1 ("Unknown") can only be populated from the current
	 * language, so there's no change here */
	if (idx == -1)
		return NULL;

	lang = &languages[idx];

	return lang->name;
}

static int lang_process_form(struct lang_screen *screen)
{
	struct config *config;
	const char *lang;
	int rc;

	config = config_copy(screen, screen->cui->config);

	lang = lang_get_lang_name(screen);

	if (!lang || (config->lang && !strcmp(lang, config->lang)))
		return 0;

	config->lang = talloc_strdup(screen, lang);

	config->safe_mode = false;
	rc = cui_send_config(screen->cui, config);
	talloc_free(config);

	if (rc)
		pb_log("cui_send_config failed!\n");
	else
		pb_debug("config sent!\n");

	return 0;
}

static void lang_screen_resize(struct nc_scr *scr)
{
	struct lang_screen *screen = lang_screen_from_scr(scr);
	(void)screen;
}

static int lang_screen_post(struct nc_scr *scr)
{
	struct lang_screen *screen = lang_screen_from_scr(scr);

	if (screen->exit)
		screen->on_exit(screen->cui);

	widgetset_post(screen->widgetset);
	nc_scr_frame_draw(scr);
	wrefresh(screen->scr.main_ncw);
	pad_refresh(screen);
	return 0;
}

static int lang_screen_unpost(struct nc_scr *scr)
{
	struct lang_screen *screen = lang_screen_from_scr(scr);
	widgetset_unpost(screen->widgetset);
	return 0;
}

struct nc_scr *lang_screen_scr(struct lang_screen *screen)
{
	return &screen->scr;
}

static void lang_screen_update_cb(struct nc_scr *scr)
{
	struct lang_screen *screen = lang_screen_from_scr(scr);

	if (!lang_process_form(screen))
		screen->exit = true;
}

static void ok_click(void *arg)
{
	struct lang_screen *screen = arg;
	const char *lang;

	if (!widget_checkbox_get_value(screen->widgets.save_cb)) {
		/* Just update the client display */
		lang = lang_get_lang_name(screen);
		if (lang)
			cui_update_language(screen->cui, lang);
		screen->exit = true;
		return;
	}

	if (discover_client_authenticated(screen->cui->client)) {
		if (lang_process_form(screen))
			/* errors are written to the status line, so we'll need
			 * to refresh */
			wrefresh(screen->scr.main_ncw);
		else
			screen->exit = true;
	} else {
		cui_show_auth(screen->cui, screen->scr.main_ncw, false,
				lang_screen_update_cb);
	}
}

static void cancel_click(void *arg)
{
	struct lang_screen *screen = arg;
	screen->exit = true;
}

static int layout_pair(struct lang_screen *screen, int y,
		struct nc_widget_label *label,
		struct nc_widget *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	widget_move(label_w, y, screen->label_x);
	widget_move(field, y, screen->field_x);
	return max(widget_height(label_w), widget_height(field));
}

static void lang_screen_layout_widgets(struct lang_screen *screen)
{
	int y;

	y = 1;

	y += layout_pair(screen, y, screen->widgets.lang_l,
			widget_select_base(screen->widgets.lang_f));

	y += 1;

	y += layout_pair(screen, y, screen->widgets.save_l,
			widget_checkbox_base(screen->widgets.save_cb));
	y += 1;

	if (screen->cui->config->safe_mode) {
		widget_move(widget_label_base(screen->widgets.safe_mode),
			y, screen->field_x);
		y += 1;
	}

	widget_move(widget_button_base(screen->widgets.ok_b),
			y, screen->field_x);
	widget_move(widget_button_base(screen->widgets.cancel_b),
			y, screen->field_x + 14);
}

static void lang_screen_setup_empty(struct lang_screen *screen)
{
	widget_new_label(screen->widgetset, 2, screen->field_x,
			_("Waiting for configuration data..."));
	screen->widgets.cancel_b = widget_new_button(screen->widgetset,
			4, screen->field_x, 9, _("Cancel"),
			cancel_click, screen);
}


static void lang_screen_setup_widgets(struct lang_screen *screen,
		const struct config *config)
{
	struct nc_widgetset *set = screen->widgetset;
	unsigned int i;
	bool found;

	build_assert(sizeof(screen->widgets) / sizeof(struct widget *)
			== N_FIELDS);

	screen->widgets.lang_l = widget_new_label(set, 0, 0, _("Language"));
	screen->widgets.lang_f = widget_new_select(set, 0, 0, 50);

	found = false;

	for (i = 0; i < ARRAY_SIZE(languages); i++) {
		struct lang *lang = &languages[i];
		bool selected;
		char *label;
		int len;

		len = wcstombs(NULL, lang->label, 0);
		assert(len >= 0);
		if (len < 0) {
			label = talloc_asprintf(screen,
				"Unable to display text in this locale (%s)\n",
				setlocale(LC_ALL, NULL));
		} else {
			label = talloc_array(screen, char, len + 1);
			wcstombs(label, lang->label, len + 1);
		}

		selected = config->lang && !strcmp(lang->name, config->lang);
		found |= selected;

		widget_select_add_option(screen->widgets.lang_f, i,
					label, selected);
	}

	if (!found && config->lang) {
		char *label = talloc_asprintf(screen,
				_("Unknown language '%s'"), config->lang);
		widget_select_add_option(screen->widgets.lang_f, -1,
					label, true);
	}

	screen->widgets.save_l = widget_new_label(set, 0, 0,
			_("Save changes?"));
	screen->widgets.save_cb = widget_new_checkbox(set, 0, 0, false);

	if (config->safe_mode)
		screen->widgets.safe_mode = widget_new_label(set, 0, 0,
			 _("Selecting 'OK' will exit safe mode"));

	screen->widgets.ok_b = widget_new_button(set, 0, 0, 10, _("OK"),
			ok_click, screen);
	screen->widgets.cancel_b = widget_new_button(set, 0, 0, 10, _("Cancel"),
			cancel_click, screen);
}

static void lang_screen_widget_focus(struct nc_widget *widget, void *arg)
{
	struct lang_screen *screen = arg;
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

static void lang_screen_draw(struct lang_screen *screen,
		const struct config *config)
{
	bool repost = false;
	int height;

	height = ARRAY_SIZE(languages) + N_FIELDS + 4;
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
			lang_screen_widget_focus, screen);

	if (!config) {
		lang_screen_setup_empty(screen);
	} else {
		lang_screen_setup_widgets(screen, config);
		lang_screen_layout_widgets(screen);
	}

	if (repost)
		widgetset_post(screen->widgetset);
}

void lang_screen_update(struct lang_screen *screen,
		const struct config *config)
{
	lang_screen_draw(screen, config);
	pad_refresh(screen);
}

static int lang_screen_destroy(void *arg)
{
	struct lang_screen *screen = arg;
	if (screen->pad)
		delwin(screen->pad);
	return 0;
}

struct lang_screen *lang_screen_init(struct cui *cui,
		const struct config *config,
		void (*on_exit)(struct cui *))
{
	struct lang_screen *screen;

	screen = talloc_zero(cui, struct lang_screen);
	talloc_set_destructor(screen, lang_screen_destroy);
	nc_scr_init(&screen->scr, pb_lang_screen_sig, 0,
			cui, lang_screen_process_key,
			lang_screen_post, lang_screen_unpost,
			lang_screen_resize);

	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->label_x = 2;
	screen->field_x = 17;

	screen->scr.frame.ltitle = talloc_strdup(screen,
			_("Petitboot Language Selection"));
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen,
			_("tab=next, shift+tab=previous, x=exit"));
	nc_scr_frame_draw(&screen->scr);

	scrollok(screen->scr.sub_ncw, true);

	lang_screen_draw(screen, config);

	return screen;
}
