/*
 *  Copyright (C) 2009 Sony Computer Entertainment Inc.
 *  Copyright 2009 Sony Corp.
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

#if !defined(_PB_NC_MENU_H)
#define _PB_NC_MENU_H

#include <assert.h>

#include <linux/input.h> /* This must be included before ncurses.h */
#if defined HAVE_NCURSESW_MENU_H
#  include <ncursesw/menu.h>
#elif defined HAVE_NCURSES_MENU_H
#  include <ncurses/menu.h>
#elif defined HAVE_MENU_H
#  include <menu.h>
#else
#  error "Curses menu.h not found."
#endif

#include "log/log.h"
#include "types/types.h"
#include "nc-scr.h"

struct pmenu;

/**
 * struct pmenu_item - Hold the state of a single menu item.
 * @i_sig: Signature for callback type checking, should be pmenu_item_sig.
 * @nci: The ncurses menu item instance for this item.
 */

struct pmenu_item {
	enum pb_nc_sig i_sig;
	ITEM *nci;
	struct pmenu *pmenu;
	void *data;
	void (*on_edit)(struct pmenu_item *item);
	int (*on_execute)(struct pmenu_item *item);
};

int pmenu_item_update(struct pmenu_item *item, const char *name);
struct pmenu_item *pmenu_item_create(struct pmenu *menu, const char *name);
struct pmenu_item *pmenu_find_device(struct pmenu *menu, struct device *dev,
	struct boot_option *opt);
void pmenu_item_insert(struct pmenu *menu, struct pmenu_item *item,
	unsigned int index);
void pmenu_item_add(struct pmenu *menu, struct pmenu_item *item,
	unsigned int insert_pt);

static inline struct pmenu_item *pmenu_item_from_arg(void *arg)
{
	struct pmenu_item *item = (struct pmenu_item *)arg;

	assert(item->i_sig == pb_item_sig);
	return item;
}

static inline struct cui_opt_data *cod_from_item(struct pmenu_item *item)
{
	return item->data;
}

typedef int (*hot_key_fn)(struct pmenu *menu, struct pmenu_item *item, int c);

int pmenu_main_hot_keys(struct pmenu *menu, struct pmenu_item *item, int c);

/**
 * struct pmenu - Data structure defining complete menu.
 * @insert_pt: Index in nc item array.
 * @ncm: The ncurses menu instance for this menu.
 */

struct pmenu {
	struct nc_scr scr;
	MENU *ncm;
	ITEM **items;
	unsigned int item_count;
	unsigned int insert_pt;
	const char *help_title;
	const struct help_text *help_text;
	hot_key_fn *hot_keys;
	unsigned int n_hot_keys;
	void (*on_exit)(struct pmenu *menu);
	void (*on_new)(struct pmenu *menu);
};

struct pmenu *pmenu_init(void *ui_ctx, unsigned int item_count,
	void (*on_exit)(struct pmenu *));
int pmenu_setup(struct pmenu *menu);
unsigned int pmenu_grow(struct pmenu *menu, unsigned int count);
int pmenu_remove(struct pmenu *menu, struct pmenu_item *item);
struct pmenu_item *pmenu_find_selected(struct pmenu *menu);

/* convenience routines */

int pmenu_exit_cb(struct pmenu_item *item);

static inline struct pmenu *pmenu_from_scr(struct nc_scr *scr)
{
	struct pmenu *pmenu;

	assert(scr->sig == pb_pmenu_sig);
	pmenu = (struct pmenu *)((char *)scr
		- (size_t)&((struct pmenu *)0)->scr);
	assert(pmenu->scr.sig == pb_pmenu_sig);

	return pmenu;
}

/* debug routines */

static inline void pmenu_dump_item(const ITEM *item)
{
	pb_debug("%p %s\n", item, (item ? item_name(item) : "(null)"));
}

static inline void pmenu_dump_items(ITEM *const *items, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		pb_debug("%u: %p %s\n", i, items[i],
			(items[i] ? item_name(items[i]) : "(null)"));
}

#endif
