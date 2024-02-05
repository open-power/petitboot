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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <wctype.h>
#include <util/util.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "i18n/i18n.h"
#include "ui/common/ui-system.h"
#include "nc-cui.h"
#include "nc-menu.h"

/**
 * pmenu_exit_cb - Callback helper that runs run menu.on_exit().
 */

int pmenu_exit_cb(struct pmenu_item *item)
{
	assert(item->pmenu->on_exit);
	item->pmenu->on_exit(item->pmenu);
	return 0;
}

/**
 * pmenu_find_selected - Find the selected pmenu_item.
 */

struct pmenu_item *pmenu_find_selected(struct pmenu *menu)
{
	return pmenu_item_from_arg(item_userptr(current_item(menu->ncm)));
}

static int pmenu_post(struct nc_scr *scr)
{
	int result;
	struct pmenu *menu = pmenu_from_scr(scr);

	result = post_menu(menu->ncm);

	nc_scr_frame_draw(scr);
	wrefresh(menu->scr.main_ncw);

	return result;
}

static int pmenu_unpost(struct nc_scr *scr)
{
	return unpost_menu(pmenu_from_scr(scr)->ncm);
}

static void pmenu_resize(struct nc_scr *scr)
{
	/* FIXME: menus can't be resized, need to recreate here */
	pmenu_unpost(scr);
	pmenu_post(scr);
}

static int pmenu_item_destructor(void *arg)
{
	struct pmenu_item *item = arg;
	free_item(item->nci);
	return 0;
}

static const char *pmenu_item_label(struct pmenu_item *item, const char *name)
{
	static int invalid_idx;
	unsigned int i;
	wchar_t *tmp;
	char *label;
	size_t len;

	len = mbstowcs(NULL, name, 0);

	/* if we have an invalid multibyte sequence, create an entirely
	 * new name, indicating that we had invalid input */
	if (len == SIZE_MAX) {
		name = talloc_asprintf(item, _("!Invalid option %d"),
				++invalid_idx);
		return name;
	}

	tmp = talloc_array(item, wchar_t, len + 1);
	mbstowcs(tmp, name, len + 1);

	/* replace anything unprintable with U+FFFD REPLACEMENT CHARACTER */
	for (i = 0; i < len; i++) {
		if (!iswprint(tmp[i]))
			tmp[i] = 0xfffd;
	}

	len = wcstombs(NULL, tmp, 0);
	label = talloc_array(item, char, len + 1);
	wcstombs(label, tmp, len + 1);

	pb_log_fn("%s\n", label);

	talloc_free(tmp);
	return label;
}

/**
 * pmenu_item_update - Update the label of an existing pmenu_item.
 *
 * The item array must be disconnected prior to calling.
 */
int pmenu_item_update(struct pmenu_item *item, const char *name)
{
	const char *label;
	ITEM *i;

	if (!item || !item->nci)
		return -1;

	label = pmenu_item_label(item, name);

	if (!label)
		return -1;

	i = new_item(label, NULL);
	if (!i) {
		talloc_free((char *)label);
		return -1;
	}
	free_item(item->nci);
	item->nci = i;

	return 0;
}

/**
 * pmenu_item_create - Allocate and initialize a new pmenu_item instance.
 *
 * Returns a pointer the the initialized struct pmenu_item instance or NULL
 * on error. The caller is responsible for calling talloc_free() for the
 * returned instance.
 */
struct pmenu_item *pmenu_item_create(struct pmenu *menu, const char *name)
{
	struct pmenu_item *item = talloc_zero(menu, struct pmenu_item);
	const char *label;

	label = pmenu_item_label(item, name);

	item->i_sig = pb_item_sig;
	item->pmenu = menu;
	item->nci = new_item(label, NULL);

	if (!item->nci) {
		talloc_free(item);
		return NULL;
	}

	talloc_set_destructor(item, pmenu_item_destructor);

	set_item_userptr(item->nci, item);

	return item;
}

void pmenu_item_insert(struct pmenu *menu, struct pmenu_item *item,
	unsigned int index)
{
	assert(item);
	assert(index < menu->item_count);
	assert(menu->items[index] == NULL);
	assert(menu_items(menu->ncm) == NULL);

	menu->items[index] = item->nci;
}

/**
 * pmenu_item_add - Insert item into appropriate position
 *
 * Inserts boot entry under matching, predefined device header entry,
 * moving items in the list if necessary
 */

void pmenu_item_add(struct pmenu *menu, struct pmenu_item *item,
	unsigned int insert_pt)
{
	struct cui_opt_data *cod = item->data;
	bool found = false;
	unsigned int dev;

	/* Items array should already be disconnected */

	for (dev = 0; dev < menu->item_count; dev++) {
		if (!menu->items[dev])
			continue;

		struct pmenu_item *i = item_userptr(menu->items[dev]);
		struct cui_opt_data *d = i->data;
		/* Device header will have opt == NULL */
		if (d && !d->opt) {
			if (cod->dev == d->dev) {
				found = true;
				break;
			}
		}
	}

	if (found) {
		assert(dev < insert_pt);
		/* Shift down entries between header and insert_pt */
		memmove(menu->items + dev + 2, menu->items + dev + 1,
			((menu->items + insert_pt) - (menu->items + dev + 1))
			* sizeof(menu->items[0]));
		memset(menu->items + dev + 1, 0, sizeof(menu->items[0]));
		insert_pt = dev + 1;
	}
	/* If for some reason we didn't find the matching device,
	 * at least add it to a valid position */
	pmenu_item_insert(menu, item, insert_pt);
}

/**
 * pmenu_find_device - Determine if a boot option is new, and if
 * so return a new pmenu_item to represent its parent device
 */

struct pmenu_item *pmenu_find_device(struct pmenu *menu, struct device *dev,
	struct boot_option *opt)
{
	struct pmenu_item *item, *dev_hdr = NULL;
	struct cui *cui = cui_from_pmenu(menu);
	bool newdev = true, matched = false;
	struct interface_info *intf;
	struct blockdev_info *bd;
	struct cui_opt_data *cod;
	struct system_info *sys;
	char hwaddr[32];
	unsigned int i;
	char buf[256];

	for (i = 0; i < menu->item_count; i++) {
		item = item_userptr(menu->items[i]);
		cod = cod_from_item(item);
		/* boot entries will have opt defined */
		if (!cod || cod->opt)
			continue;
		if (cod->dev == dev) {
			pb_debug("%s: opt %s fits under %s\n",__func__,
				 opt->name, opt->device_id);
			newdev = false;
			break;
		}
	}

	if (!newdev) {
		pb_debug("%s: No new device\n",__func__);
		return NULL;
	}

	/* Create a dummy pmenu_item to represent the dev */
	pb_debug("%s: Building new item\n",__func__);
	sys = cui->sysinfo;
	switch (dev->type) {
	case DEVICE_TYPE_OPTICAL:
	case DEVICE_TYPE_DISK:
	case DEVICE_TYPE_USB:
		/* Find block info */
		for (i = 0; sys && i < sys->n_blockdevs; i++) {
			bd = sys->blockdevs[i];
			if (!strcmp(opt->device_id, bd->name)) {
				matched = true;
				break;
			}
		}
		if (matched) {
			snprintf(buf,sizeof(buf),"[%s: %s / %s]",
				device_type_display_name(dev->type),
				bd->name, bd->uuid);
		}
		break;

	case DEVICE_TYPE_NETWORK:
		/* Find interface info */
		for (i = 0; sys && i < sys->n_interfaces; i++) {
			intf = sys->interfaces[i];
			if (!strcmp(opt->device_id, intf->name)) {
				matched = true;
				break;
			}
		}
		if (matched) {
			mac_str(intf->hwaddr, intf->hwaddr_size,
				hwaddr, sizeof(hwaddr));
			snprintf(buf,sizeof(buf),"[%s: %s / %s]",
				_("Network"), intf->name, hwaddr);
		}
		break;
	case DEVICE_TYPE_ANY:
		/* This is an option found from a file:// url, not associated
		 * with any device */
		snprintf(buf, sizeof(buf), "[Custom Local Options]");
		matched = true;
		break;

	default:
		/* Assume the device may be able to boot */
		break;
	}
	if (!matched) {
		pb_debug("%s: No matching device found for %s (%s)\n",
			__func__,opt->device_id, dev->id);
		snprintf(buf, sizeof(buf), "[%s: %s]",
			_("Unknown Device"), dev->id);
	}

	dev_hdr = pmenu_item_create(menu, buf);
	if (!dev_hdr) {
		pb_log("%s: Failed to create item\n",__func__);
		return NULL;
	}

	dev_hdr->on_execute = NULL;
	item_opts_off(dev_hdr->nci, O_SELECTABLE);

	/* We identify dev_hdr items as having a valid c->name,
	 * but a NULL c->opt */
	cod = talloc_zero(dev_hdr, struct cui_opt_data);
	cod->name = talloc_strdup(dev_hdr, opt->device_id);
	cod->dev = dev;
	dev_hdr->data = cod;

	pb_debug("%s: returning %s\n",__func__,cod->name);
	return dev_hdr;
}

static int pmenu_item_get_index(const struct pmenu_item *item)
{
	unsigned int i;

	if (item)
		for (i = 0; i < item->pmenu->item_count; i++)
			if (item->pmenu->items[i] == item->nci)
				return i;

	pb_log_fn("not found: %p %s\n", item,
		(item ? item_name(item->nci) : "(null)"));
	return -1;
}

/**
 * pmenu_move_cursor - Move the cursor.
 * @req: An ncurses request or char to send to menu_driver().
 */

static void pmenu_move_cursor(struct pmenu *menu, int req)
{
	menu_driver(menu->ncm, req);
	wrefresh(menu->scr.main_ncw);
}

/**
 * pmenu_main_hot_keys - Hot keys for the main boot menu
 */
int pmenu_main_hot_keys(struct pmenu *menu, struct pmenu_item *item, int c)
{
	struct nc_scr *scr = &menu->scr;
	(void)item;

	switch (c) {
	case 'i':
		cui_show_sysinfo(cui_from_arg(scr->ui_ctx));
		break;
	case 'c':
		cui_show_config(cui_from_arg(scr->ui_ctx));
		break;
	case 'l':
		cui_show_lang(cui_from_arg(scr->ui_ctx));
		break;
	case 'g':
		cui_show_statuslog(cui_from_arg(scr->ui_ctx));
		break;
	default:
		return 0;
	}

	return c;
}

/**
 * pmenu_process_key - Process a user keystroke.
 */

static void pmenu_process_key(struct nc_scr *scr, int key)
{
	struct pmenu *menu = pmenu_from_scr(scr);
	struct pmenu_item *item = pmenu_find_selected(menu);
	unsigned int i;

	nc_scr_status_free(&menu->scr);

	if (menu->hot_keys)
		for (i = 0; i < menu->n_hot_keys; i++) {
			if (menu->hot_keys[i](menu, item, key))
				return;
		}

	switch (key) {
	case 27: /* ESC */
	case 'x':
		if (menu->on_exit)
			menu->on_exit(menu);
		nc_flush_keys();
		return;

	case KEY_PPAGE:
		pmenu_move_cursor(menu, REQ_SCR_UPAGE);
		break;
	case KEY_NPAGE:
		pmenu_move_cursor(menu, REQ_SCR_DPAGE);
		break;
	case KEY_HOME:
		pmenu_move_cursor(menu, REQ_FIRST_ITEM);
		break;
	case KEY_END:
		pmenu_move_cursor(menu, REQ_LAST_ITEM);
		break;
	case KEY_UP:
		pmenu_move_cursor(menu, REQ_UP_ITEM);
		break;
	case KEY_BTAB:
		pmenu_move_cursor(menu, REQ_PREV_ITEM);
		break;
	case KEY_DOWN:
		pmenu_move_cursor(menu, REQ_DOWN_ITEM);
		break;
	case '\t':
		pmenu_move_cursor(menu, REQ_NEXT_ITEM);
		break;
	case 'e':
		if (item->on_edit)
			item->on_edit(item);
		break;
	case 'n':
		if (menu->on_new)
			menu->on_new(menu);
		break;
	case ' ':
	case '\n':
	case '\r':
		if (item->on_execute)
			item->on_execute(item);
		break;
	case KEY_F(1):
	case 'h':
		if (menu->help_text)
			cui_show_help(cui_from_arg(scr->ui_ctx),
					menu->help_title, menu->help_text);
		break;
	default:
		menu_driver(menu->ncm, key);
		break;
	}
}

/**
 * pmenu_grow - Grow the item array.
 * @count: The count of new items.
 *
 * The item array must be disconnected prior to calling pmenu_grow().
 * Returns the insert point index.
 */

unsigned int pmenu_grow(struct pmenu *menu, unsigned int count)
{
	unsigned int tmp;

	assert(item_count(menu->ncm) == 0 && "not disconnected");

	pb_log_fn("%u current + %u new = %u\n", menu->item_count,
		count, menu->item_count + count);

	/* Note that items array has a null terminator. */

	menu->items = talloc_realloc(menu, menu->items, ITEM *,
		menu->item_count + count + 1);

	memmove(menu->items + menu->insert_pt + count,
		menu->items + menu->insert_pt,
		(menu->item_count - menu->insert_pt + 1) * sizeof(ITEM *));

	memset(menu->items + menu->insert_pt, 0, count * sizeof(ITEM *));

	tmp = menu->insert_pt;
	menu->insert_pt += count;
	menu->item_count += count;

	return tmp;
}

/**
 * pmenu_remove - Remove an item from the item array.
 *
 * The item array must be disconnected prior to calling pmenu_remove()
 */

int pmenu_remove(struct pmenu *menu, struct pmenu_item *item)
{
	int index;

	assert(item_count(menu->ncm) == 0 && "not disconnected");

	assert(menu->item_count);

	pb_log_fn("%u\n", menu->item_count);

	index = pmenu_item_get_index(item);

	if (index < 0)
		return -1;

	talloc_free(item);

	/* Note that items array has a null terminator. */

	menu->insert_pt--;
	menu->item_count--;

	memmove(&menu->items[index], &menu->items[index + 1],
		(menu->item_count - index + 1) * sizeof(ITEM *));
	menu->items = talloc_realloc(menu, menu->items, ITEM *,
		menu->item_count + 1);

	return 0;
}

static int pmenu_destructor(void *ptr)
{
	struct pmenu *menu = ptr;
	assert(menu->scr.sig == pb_pmenu_sig);
	menu->scr.sig = pb_removed_sig;

	unpost_menu(menu->ncm);
	free_menu(menu->ncm);
	delwin(menu->scr.sub_ncw);
	delwin(menu->scr.main_ncw);
	return 0;
}

/**
 * pmenu_init - Allocate and initialize a new menu instance.
 *
 * Returns a pointer the the initialized struct pmenu instance or NULL on error.
 * The caller is responsible for calling talloc_free() for the returned
 * instance.
 */

struct pmenu *pmenu_init(void *ui_ctx, unsigned int item_count,
	void (*on_exit)(struct pmenu *))
{
	struct pmenu *menu = talloc_zero(ui_ctx, struct pmenu);
	if (!menu)
		return NULL;

	talloc_set_destructor(menu, pmenu_destructor);

	/* note items array has a null terminator */
	menu->items = talloc_zero_array(menu, ITEM *, item_count + 1);
	if (!menu->items) {
		talloc_free(menu);
		return NULL;
	}

	nc_scr_init(&menu->scr, pb_pmenu_sig, 0, ui_ctx, pmenu_process_key,
		pmenu_post, pmenu_unpost, pmenu_resize);

	menu->item_count = item_count;
	menu->insert_pt = 0; /* insert from top */
	menu->on_exit = on_exit;

	return menu;
}

/**
 * pmenu_setup - Create nc menu, setup nc windows.
 *
 */

int pmenu_setup(struct pmenu *menu)
{
	assert(!menu->ncm);

	menu->ncm = new_menu(menu->items);

	if (!menu->ncm) {
		pb_log("%s:%d: new_menu failed: %s\n", __func__, __LINE__,
			strerror(errno));
		return -1;
	}

	set_menu_win(menu->ncm, menu->scr.main_ncw);
	set_menu_sub(menu->ncm, menu->scr.sub_ncw);

	/* Makes menu scrollable. */
	set_menu_format(menu->ncm, LINES - nc_scr_frame_lines, 1);

	set_menu_grey(menu->ncm, A_NORMAL);

	return 0;
}

