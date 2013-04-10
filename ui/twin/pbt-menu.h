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

#if !defined(_PBT_MENU_H)
#define _PBT_MENU_H

#include "list/list.h"
#include "types/types.h"

#include "pbt-scr.h"


/**
 * struct pbt_item - A menu item.
 */

struct pbt_item
{
	struct list_item list;

	struct pbt_menu *menu; // convinence pointer
	struct pbt_client *pbt_client; // convinence pointer

	twin_window_t *window;
	twin_pixmap_t *pixmap_idle;
	twin_pixmap_t *pixmap_selected;
	twin_pixmap_t *pixmap_active;

	struct pbt_menu *sub_menu;

	int (*on_execute)(struct pbt_item *item);
	int (*on_edit)(struct pbt_item *item);

	union {
		struct device *pb_device;
		struct boot_option *pb_opt;
	};
	void *data;
};

struct pbt_item *pbt_item_create(struct pbt_menu *menu, const char *name,
	unsigned int position, const char *icon_filename, const char *title,
	const char *text);

static inline struct pbt_item *pbt_item_create_reduced(struct pbt_menu *menu,
	const char *name, unsigned int position, const char *icon_filename)
{
	return pbt_item_create(menu, name, position, icon_filename, NULL,
		NULL);
}

static inline const char *pbt_item_name(const struct pbt_item *item)
{
	return item->window->name;
}

#define pbt_dump_item(_i) _pbt_dump_item(_i, __func__, __LINE__)
void _pbt_dump_item(const struct pbt_item* item, const char *func,
	int line);

struct pbt_text_layout {
	twin_argb32_t color;
	unsigned int font_size;
};

struct pbt_menu_layout {
	unsigned int item_height;
	unsigned int item_space;
	unsigned int text_space;
	//unsigned int icon_height;
	//unsigned int icon_width;
	struct pbt_text_layout title;
	struct pbt_text_layout text;
};

 /**
  * struct pbt_menu - A twin menu screen.
  */

struct pbt_menu {
	struct pbt_scr *scr; // convinence pointer
	struct pbt_menu *parent;
	twin_window_t *window;
	twin_pixmap_t *pixmap;

	struct pbt_border border;
	twin_argb32_t background_color;
	struct pbt_menu_layout layout;

	struct list* item_list;
	unsigned int n_items;
	uint32_t default_item_hash;

	int has_focus;
	struct pbt_item *selected;
	int (*on_open)(struct pbt_menu *menu);
};

struct pbt_menu *pbt_menu_create(void *talloc_ctx, const char *name,
	struct pbt_scr *scr, struct pbt_menu *parent, const struct pbt_quad *q,
	const struct pbt_menu_layout *layout);
void pbt_menu_set_focus(struct pbt_menu *menu, int focus);
void pbt_menu_set_selected(struct pbt_menu *menu, struct pbt_item *item);
struct pbt_quad *pbt_menu_get_item_quad(const struct pbt_menu *menu,
	unsigned int pos, struct pbt_quad *q);
void pbt_menu_hide(struct pbt_menu *menu);
void pbt_menu_show(struct pbt_menu *menu, int hide);

static inline const char *pbt_menu_name(const struct pbt_menu *menu)
{
	return menu->window->name;
}

static inline struct pbt_menu *pbt_menu_from_window(twin_window_t *window)
{
	struct pbt_menu *menu = window->client_data;

	assert(menu);
	return menu;
}

static inline struct pbt_item *pbt_item_from_window(twin_window_t *window)
{
	struct pbt_item *item = window->client_data;

	assert(item);
	return item;
}

static inline int pbt_item_is_selected(const struct pbt_item* item)
{
	return item == item->menu->selected;
}

static inline void pbt_item_set_as_selected(struct pbt_item* item)
{
	pbt_menu_set_selected(item->menu, item);
}

int pbt_item_editor(struct pbt_item *item);

static inline void pbt_item_redraw(struct pbt_item *item)
{
	pbt_window_redraw(item->window);
}

static inline void pbt_menu_redraw(struct pbt_menu *menu)
{
	pbt_window_redraw(menu->window);
}


#endif
