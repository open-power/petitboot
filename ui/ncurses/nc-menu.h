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
#include <menu.h>

#include "log/log.h"
#include "pb-protocol/pb-protocol.h"
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
	int (*on_edit)(struct pmenu_item *item);
	int (*on_execute)(struct pmenu_item *item);
};

struct pmenu_item *pmenu_item_alloc(struct pmenu *menu);
struct pmenu_item *pmenu_item_setup(struct pmenu *menu, struct pmenu_item *i,
	unsigned int index, const char *name);
int pmenu_item_replace(struct pmenu_item *i, const char *name);
void pmenu_item_delete(struct pmenu_item *item);

static inline struct pmenu_item *pmenu_item_from_arg(void *arg)
{
	struct pmenu_item *item = (struct pmenu_item *)arg;

	assert(item->i_sig == pb_item_sig);
	return item;
}

static inline struct pmenu_item *pmenu_item_init(struct pmenu *menu,
	unsigned int index, const char *name)
{
	return pmenu_item_setup(menu, pmenu_item_alloc(menu), index, name);
}

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
	int (*hot_key)(struct pmenu *menu, struct pmenu_item *item, int c);
	void (*on_exit)(struct pmenu *menu);
	void (*on_open)(struct pmenu *menu);
};

struct pmenu *pmenu_init(void *ui_ctx, unsigned int item_count,
	void (*on_exit)(struct pmenu *));
int pmenu_setup(struct pmenu *menu);
void pmenu_delete(struct pmenu *menu);
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

#if defined(DEBUG)
enum {do_debug = 1};
#else
enum {do_debug = 0};
#endif

static inline void pmenu_dump_item(const ITEM *item)
{
	if (do_debug)
		pb_log("%p %s\n", item, (item ? item->name.str : "(null)"));
}

static inline void pmenu_dump_items(ITEM *const *items, unsigned int count)
{
	unsigned int i;

	if (do_debug)
		for (i = 0; i < count; i++)
			pb_log("%u: %p %s\n", i, items[i],
				(items[i] ? items[i]->name.str : "(null)"));
}

#endif
