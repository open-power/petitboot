/*
 *  Copyright Geoff Levand <geoff@infradead.org>
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

#include <assert.h>
#include <string.h>
#include <linux/input.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "ui/common/ui-system.h"

#include "pbt-menu.h"

static void pbt_item_draw_cb(twin_window_t *window)
{
	struct pbt_item *item = pbt_item_from_window(window);
	twin_pixmap_t *image;

	assert(window == item->window);

	pbt_dump_item(item);

	//pbt_dump_pixmap(window->pixmap);

	if (pbt_item_is_selected(item) && item->menu->has_focus)
		image = item->pixmap_active;
	else if (pbt_item_is_selected(item) && !item->menu->has_focus)
		image = item->pixmap_selected;
	else
		image = item->pixmap_idle;

	pbt_image_draw(window->pixmap, image);
}

static twin_bool_t pbt_item_event_cb(twin_window_t *window,
	twin_event_t *event)
{
	struct pbt_item *item = pbt_item_from_window(window);

	pbt_dump_event(pbt_item_name(item), window, event);

	switch(event->kind) {
	case TwinEventButtonDown:
		if (item->on_execute)
			item->on_execute(item);
		break;
	case TwinEventButtonUp:
		break;
	case TwinEventMotion:
		/* prevent window drag */
		return TWIN_TRUE;
	case TwinEventEnter:
		pbt_item_set_as_selected(item);
		break;
	case TwinEventLeave:
		break;
	case TwinEventKeyDown:
		switch(event->u.key.key) {
		case (twin_keysym_t)XK_Return:
		case (twin_keysym_t)KEY_ENTER:
			if (item->on_execute)
				item->on_execute(item);
			break;
		case (twin_keysym_t)'e':
			if (item->on_edit)
				item->on_edit(item);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return TWIN_FALSE;
}

int pbt_item_editor(struct pbt_item *item)
{
	(void)item;
	return -1;
}

struct pbt_item *pbt_item_create(struct pbt_menu *menu, const char *name,
	unsigned int position, const char *icon_filename, const char *title,
	const char *text)
{
	struct pbt_quad q;
	struct pbt_item *item;
	twin_pixmap_t *icon;
	twin_operand_t src;
	const struct pbt_menu_layout *layout = &menu->layout;
	twin_path_t *path;
	static const twin_argb32_t focus_color = 0x10404040;
	enum {
		corner_rounding = 8,
		stroke_width = 2,
	};

	assert(menu);

	DBGS("%d:%s (%s)\n", position, name, icon_filename);

	item = talloc_zero(menu, struct pbt_item);

	if (!item)
		return NULL;

	item->menu = menu;
	item->on_edit = pbt_item_editor;

	pbt_menu_get_item_quad(menu, position, &q);

	item->window = twin_window_create(menu->scr->tscreen,
		TWIN_ARGB32, TwinWindowPlain,
		q.x, q.y, q.width, q.height);

	if (!item->window)
		goto fail_window_create;

	twin_window_set_name(item->window, name);
	item->window->client_data = item;
	item->window->draw = pbt_item_draw_cb;
	item->window->event = pbt_item_event_cb;

	item->pixmap_idle = twin_pixmap_create(TWIN_ARGB32, q.width, q.height);
	assert(item->pixmap_idle);

	item->pixmap_selected = twin_pixmap_create(TWIN_ARGB32, q.width,
		q.height);
	assert(item->pixmap_selected);

	item->pixmap_active = twin_pixmap_create(TWIN_ARGB32, q.width,
		q.height);
	assert(item->pixmap_active);

	if (!item->pixmap_idle || !item->pixmap_selected || !item->pixmap_active)
		goto fail_pixmap_create;

	twin_fill(item->pixmap_idle, 0x01000000, TWIN_SOURCE, 0, 0, q.width,
		q.height);

	/* Add item icon */

	icon = pbt_icon_load(icon_filename);

	if (!icon)
		goto fail_icon;

	src.source_kind = TWIN_PIXMAP;
	src.u.pixmap = icon;

	twin_composite(item->pixmap_idle,
		//0, (item->pixmap_idle->height - icon->height) / 2,
		0, 0,
		&src, 0, 0,
		NULL, 0, 0,
		TWIN_SOURCE,
		icon->width, icon->height);

	/* Add item text */

	path = twin_path_create();
	assert(path);

	if (title) {
		twin_path_set_font_size(path,
			twin_int_to_fixed(layout->title.font_size));
		twin_path_set_font_style(path, TWIN_TEXT_UNHINTED);

		twin_path_move(path,
			twin_int_to_fixed(icon->width + layout->text_space),
			twin_int_to_fixed(layout->title.font_size
				+ layout->text_space));
		twin_path_utf8(path, title);
		twin_paint_path(item->pixmap_idle, layout->title.color, path);
		twin_path_empty(path);
	}

	if (text) {
		twin_path_set_font_size(path,
			twin_int_to_fixed(layout->text.font_size));
		twin_path_move(path,
			twin_int_to_fixed(icon->width + layout->text_space),
			twin_int_to_fixed(layout->title.font_size
				+ layout->text.font_size
				+ layout->text_space));
		twin_path_utf8(path, text);
		twin_paint_path(item->pixmap_idle, layout->text.color, path);
		twin_path_empty(path);
	}

	pbt_image_draw(item->pixmap_selected, item->pixmap_idle);
	pbt_image_draw(item->pixmap_active, item->pixmap_idle);

if (0) {
	static const struct pbt_border grey_border = {
		.right = 1,
		.left = 1,
		.top = 1,
		.bottom = 1,
		.fill_color = 0xffe0e0e0,
	};

	//pbt_border_draw(item->pixmap_idle, &pbt_blue_debug_border);
	pbt_border_draw(item->pixmap_selected, &grey_border);
	pbt_border_draw(item->pixmap_active, &pbt_green_debug_border);
} else {
	assert(!(stroke_width % 2));

	/* pixmap_selected */

	twin_path_rounded_rectangle(path,
		twin_int_to_fixed(stroke_width / 2),
		twin_int_to_fixed(stroke_width / 2),
		twin_int_to_fixed(item->pixmap_selected->width - stroke_width),
		twin_int_to_fixed(item->pixmap_selected->height - stroke_width),
		twin_int_to_fixed(corner_rounding),
		twin_int_to_fixed(corner_rounding));

	twin_paint_stroke(item->pixmap_selected, focus_color, path,
		twin_int_to_fixed(stroke_width));

	twin_path_empty(path);

	/* pixmap_active */

	twin_path_rounded_rectangle(path, 0, 0,
		twin_int_to_fixed(item->pixmap_active->width),
		twin_int_to_fixed(item->pixmap_active->height),
		twin_int_to_fixed(corner_rounding),
		twin_int_to_fixed(corner_rounding));

	twin_paint_path(item->pixmap_active, focus_color, path);

	twin_path_empty(path); // FIXME: need it???
}
	twin_path_destroy(path);

	list_add_tail(menu->item_list, &item->list);

	pbt_item_redraw(item);

	return item;

fail_window_create:
fail_pixmap_create:
fail_icon:
	return NULL;
}

void _pbt_dump_item(const struct pbt_item* item, const char *func, int line)
{
	DBG("%s:%d: %p: %sselected, %sfocus\n", func, line, item,
		(pbt_item_is_selected(item) ? "+" : "-"),
		(item->menu->has_focus ? "+" : "-"));
}

/**
 * pbt_menu_get_item_quad - Return item coords relative to screen origin.
 */

struct pbt_quad *pbt_menu_get_item_quad(const struct pbt_menu *menu,
	unsigned int pos, struct pbt_quad *q)
{
	const struct pbt_menu_layout *layout = &menu->layout;

	q->x = menu->window->pixmap->x + layout->item_space;

	q->width = menu->window->pixmap->width - 2 * layout->item_space;

	q->y = menu->window->pixmap->y + layout->item_space
		+ pos * (layout->item_height + layout->item_space);

	q->height = layout->item_height;

	return q;
}

static void pbt_menu_draw_cb(twin_window_t *window)
{
	struct pbt_menu *menu = pbt_menu_from_window(window);
	twin_path_t *path = twin_path_create();

	assert(path);

	pbt_dump_pixmap(window->pixmap);

	twin_fill(window->pixmap, menu->background_color, TWIN_SOURCE,
		0, 0, window->pixmap->width, window->pixmap->height);

	pbt_border_draw(window->pixmap, &menu->border);

	twin_path_destroy(path);
}

static twin_bool_t pbt_menu_event_cb(twin_window_t *window,
	twin_event_t *event)
{
	struct pbt_menu *menu = pbt_menu_from_window(window);
	struct pbt_item *i;

	pbt_dump_event(pbt_menu_name(menu), window, event);

	switch(event->kind) {
	case TwinEventButtonDown:
	case TwinEventButtonUp:
	case TwinEventMotion:
		/* prevent window drag */
		return TWIN_TRUE;
	case TwinEventEnter:
		pbt_menu_set_focus(menu, 1);
		break;
	case TwinEventLeave:
		if (!pbt_window_contains(window, event))
			pbt_menu_set_focus(menu, 0);
		break;
	case TwinEventKeyDown:
		switch(event->u.key.key) {
		case (twin_keysym_t)XK_Up:
		case (twin_keysym_t)KEY_UP:
			i = list_prev_entry(menu->item_list, menu->selected,
				list);
			if (i)
				pbt_item_set_as_selected(i);
			break;
		case (twin_keysym_t)XK_Down:
		case (twin_keysym_t)KEY_DOWN:
			i = list_next_entry(menu->item_list, menu->selected,
				list);
			if (i)
				pbt_item_set_as_selected(i);
			break;
		case (twin_keysym_t)XK_Left:
		case (twin_keysym_t)KEY_LEFT:
			if (menu->parent) {
				pbt_menu_set_focus(menu, 0);
				pbt_menu_set_focus(menu->parent, 1);
			} else
				DBGS("no parent\n");
			break;
		case (twin_keysym_t)XK_Right:
		case (twin_keysym_t)KEY_RIGHT:
			if (menu->selected->sub_menu) {
				pbt_menu_set_focus(menu, 0);
				pbt_menu_set_focus(menu->selected->sub_menu, 1);
			} else
				DBGS("no sub_menu\n");
			break;
		default:
			return pbt_item_event_cb(menu->selected->window, event);
		}
		break;
	default:
		break;
	}
	return TWIN_FALSE;
}

struct pbt_menu *pbt_menu_create(void *talloc_ctx, const char *name,
	struct pbt_scr *scr, struct pbt_menu *parent, const struct pbt_quad *q,
	const struct pbt_menu_layout *layout)
{
	struct pbt_menu *menu;

	assert(scr);

	DBGS("%s\n", name);

	menu = talloc_zero(talloc_ctx, struct pbt_menu);

	if (!menu)
		return NULL;

	menu->scr = scr;
	menu->parent = parent;
	menu->layout = *layout;

	menu->item_list = talloc(menu, struct list);
	list_init(menu->item_list);

	menu->window = twin_window_create(scr->tscreen, TWIN_ARGB32,
		TwinWindowPlain, q->x, q->y,
		q->width, q->height);

	if (!menu->window)
		goto fail_window;

	DBGS("window = %p\n", menu->window);

	twin_window_set_name(menu->window, name);

	menu->background_color = 0x01000000; //FIXME: what value???

	menu->window->draw = pbt_menu_draw_cb;
	menu->window->event = pbt_menu_event_cb;
	menu->window->client_data = menu;

	pbt_dump_pixmap(menu->window->pixmap);

	pbt_menu_redraw(menu);

	return menu;

fail_window:
	assert(0);
	talloc_free(menu);
	return NULL;
}

void pbt_menu_set_focus(struct pbt_menu *menu, int focus)
{
	DBGS("%s(%p): %d -> %d\n", pbt_menu_name(menu), menu, menu->has_focus,
		focus);

	assert(menu->selected);

	if (!menu->has_focus == !focus)
		return;

	menu->has_focus = !!focus;

	/* Route key events to menu with focus. */

	if (menu->has_focus)
		menu->scr->tscreen->active = menu->window->pixmap;

	pbt_item_redraw(menu->selected);
}

void pbt_menu_hide(struct pbt_menu *menu)
{
	struct pbt_item *item;

	if (!menu)
		return;

	list_for_each_entry(menu->item_list, item, list) {
		if (item->sub_menu)
			pbt_menu_hide(item->sub_menu);

		twin_window_hide(item->window);
		//twin_window_queue_paint(item->window);
	}

	twin_window_hide(menu->window);
	//twin_window_queue_paint(menu->window);
}

void pbt_menu_show(struct pbt_menu *menu, int hide)
{
	struct pbt_item *item;

	if (!menu)
		return;

	twin_window_show(menu->window);
	pbt_menu_redraw(menu);

	list_for_each_entry(menu->item_list, item, list) {
		twin_window_show(item->window);
		pbt_item_redraw(item);

		if (item->sub_menu) {
			if (pbt_item_is_selected(item))
				pbt_menu_show(item->sub_menu, hide);
			else if (hide)
				pbt_menu_hide(item->sub_menu);
		}
	}
}

void pbt_menu_set_selected(struct pbt_menu *menu, struct pbt_item *item)
{
	struct pbt_item *last_selected;

	assert(item);

	DBGS("%s(%p): %s(%p) -> %s(%p)\n", pbt_menu_name(menu), menu,
		(menu->selected ? pbt_menu_name(menu) : "none"),
		menu->selected, pbt_item_name(item), item);

	if (menu->selected == item)
		return;

	last_selected = menu->selected;
	menu->selected = item;

	if (last_selected) {
		pbt_menu_hide(last_selected->sub_menu);
		pbt_item_redraw(last_selected);
	}

	pbt_item_redraw(item);
	pbt_menu_show(item->sub_menu, 0);
}
