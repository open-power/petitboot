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

#ifndef NC_WIDGETS_H

struct nc_widgetset;
struct nc_widget_label;
struct nc_widget_checkbox;
struct nc_widget_textbox;
struct nc_widget_button;

struct nc_widget_label *widget_new_label(struct nc_widgetset *set,
		int y, int x, char *str);
struct nc_widget_checkbox *widget_new_checkbox(struct nc_widgetset *set,
		int y, int x, bool checked);
struct nc_widget_textbox *widget_new_textbox(struct nc_widgetset *set,
		int y, int x, int len, char *str);
struct nc_widget_select *widget_new_select(struct nc_widgetset *set,
		int y, int x, int len);
struct nc_widget_button *widget_new_button(struct nc_widgetset *set,
		int y, int x, int size, const char *str,
		void (*click)(void *), void *arg);

void widget_textbox_set_fixed_size(struct nc_widget_textbox *textbox);
void widget_textbox_set_validator_integer(struct nc_widget_textbox *textbox,
		long min, long max);
void widget_textbox_set_validator_ipv4(struct nc_widget_textbox *textbox);
void widget_textbox_set_validator_ipv4_multi(struct nc_widget_textbox *textbox);

void widget_select_add_option(struct nc_widget_select *select, int value,
		const char *text, bool selected);

void widget_select_on_change(struct nc_widget_select *select,
		void (*on_change)(void *arg, int value), void *arg);

char *widget_textbox_get_value(struct nc_widget_textbox *textbox);
bool widget_checkbox_get_value(struct nc_widget_checkbox *checkbox);
int widget_select_get_value(struct nc_widget_select *select);
int widget_select_height(struct nc_widget_select *select);
void widget_select_drop_options(struct nc_widget_select *select);

/* generic widget API */
struct nc_widget *widget_textbox_base(struct nc_widget_textbox *textbox);
struct nc_widget *widget_checkbox_base(struct nc_widget_checkbox *checkbox);
struct nc_widget *widget_select_base(struct nc_widget_select *select);
struct nc_widget *widget_label_base(struct nc_widget_label *label);
struct nc_widget *widget_button_base(struct nc_widget_button *button);

void widget_move(struct nc_widget *widget, int y, int x);
void widget_set_visible(struct nc_widget *widget, bool visible);
int widget_height(struct nc_widget *widget);
int widget_width(struct nc_widget *widget);
int widget_y(struct nc_widget *widget);
int widget_x(struct nc_widget *widget);
int widget_focus_y(struct nc_widget *widget);

/* widgetset API */
typedef void (*widget_focus_cb)(struct nc_widget *widget, void *arg);
struct nc_widgetset *widgetset_create(void *ctx, WINDOW *main, WINDOW *sub);
void widgetset_set_widget_focus(struct nc_widgetset *set,
		widget_focus_cb cb, void *arg);
void widgetset_set_windows(struct nc_widgetset *widgetset,
		WINDOW *main, WINDOW *sub);
void widgetset_post(struct nc_widgetset *set);
void widgetset_unpost(struct nc_widgetset *set);
bool widgetset_process_key(struct nc_widgetset *set, int key);

#endif /* NC_WIDGETS_H */

