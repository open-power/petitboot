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

#include "config.h"

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/ioctl.h>

#include "log/log.h"
#include "pb-protocol/pb-protocol.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "process/process.h"
#include "ui/common/discover-client.h"
#include "nc-cui.h"

static struct cui_opt_data *cod_from_item(struct pmenu_item *item)
{
	return item->data;
}

/**
 * cui_abort - Signal the main cui program loop to exit.
 *
 * Sets cui.abort, which causes the cui_run() routine to return.
 */

void cui_abort(struct cui *cui)
{
	pb_log("%s: exiting\n", __func__);
	cui->abort = 1;
}

/**
 * cui_resize - Signal the main cui program loop to resize
 *
 * Called at SIGWINCH.
 */

void cui_resize(struct cui *cui)
{
	pb_log("%s: resizing\n", __func__);
	cui->resize = 1;
}

/**
 * cui_on_exit - A generic main menu ESC callback.
 */

void cui_on_exit(struct pmenu *menu)
{
	cui_abort(cui_from_pmenu(menu));
}

/**
 * cui_run_cmd - A generic cb to run the supplied command.
 */

int cui_run_cmd(struct pmenu_item *item)
{
	int result;
	struct cui *cui = cui_from_item(item);
	const char **cmd_argv = item->data;

	nc_scr_status_printf(cui->current, "Running %s...", cmd_argv[0]);

	def_prog_mode();

	result = process_run_simple_argv(item, cmd_argv);

	reset_prog_mode();
	redrawwin(cui->current->main_ncw);

	if (result) {
		pb_log("%s: failed: '%s'\n", __func__, cmd_argv[0]);
		nc_scr_status_printf(cui->current, "Failed: %s", cmd_argv[0]);
	}

	return result;
}

/**
 * cui_boot - A generic cb to run kexec.
 */

static int cui_boot(struct pmenu_item *item)
{
	int result;
	struct cui *cui = cui_from_item(item);
	struct cui_opt_data *cod = cod_from_item(item);

	assert(cui->current == &cui->main->scr);

	pb_log("%s: %s\n", __func__, cod->name);

	nc_scr_status_printf(cui->current, "Booting %s...", cod->name);

	def_prog_mode();

	result = discover_client_boot(cui->client, NULL, cod->opt, cod->bd);

	reset_prog_mode();
	redrawwin(cui->current->main_ncw);

	if (result) {
		nc_scr_status_printf(cui->current,
				"Failed: boot %s", cod->bd->image);
	}

	return 0;
}

/**
 * cui_boot_editor_on_exit - The boot_editor on_exit callback.
 */

static void cui_boot_editor_on_exit(struct boot_editor *boot_editor,
		enum boot_editor_result boot_editor_result,
		struct pb_boot_data *bd)
{
	struct cui *cui = cui_from_pmenu(boot_editor->original_pmenu);
	struct pmenu_item *item = boot_editor->data;
	struct cui_opt_data *cod;

	if (boot_editor_result != boot_editor_update) {
		cui_set_current(cui, &cui->main->scr);
		talloc_free(boot_editor);
		return;
	}

	assert(bd);

	/* Is this was a new item, we'll need to update the menu */
	if (!item) {
		struct pmenu *menu = boot_editor->original_pmenu;
		int insert_pt;

		/* Detach the items array. */
		set_menu_items(menu->ncm, NULL);

		/* Insert new item at insert_pt. */
		insert_pt = pmenu_grow(menu, 1);
		item = pmenu_item_alloc(menu);
		item->on_edit = cui_item_edit;
		item->on_execute = cui_boot;
		item->data = cod = talloc_zero(item, struct cui_opt_data);

		cod->name = talloc_asprintf(cod, "User item %u:", insert_pt);
		pmenu_item_setup(menu, item, insert_pt,
				talloc_strdup(item, cod->name));

		/* Re-attach the items array. */
		set_menu_items(menu->ncm, menu->items);
		menu->scr.post(&menu->scr);
	} else {
		cod = item->data;
	}

	cod->bd = talloc_steal(cod, bd);

	/* FIXME: need to make item visible somehow */
	set_current_item(item->pmenu->ncm, item->nci);
	cui_set_current(cui, &cui->main->scr);
	talloc_free(boot_editor);
}

void cui_item_edit(struct pmenu_item *item)
{
	struct cui *cui = cui_from_item(item);
	struct cui_opt_data *cod = cod_from_item(item);
	struct boot_editor *boot_editor;

	boot_editor = boot_editor_init(item->pmenu, cod->bd,
			cui_boot_editor_on_exit);
	boot_editor->data = item;
	cui_set_current(cui, &boot_editor->scr);
}

void cui_item_new(struct pmenu *menu)
{
	struct cui *cui = cui_from_pmenu(menu);
	struct boot_editor *boot_editor;

	boot_editor = boot_editor_init(menu, NULL,
			cui_boot_editor_on_exit);
	cui_set_current(cui, &boot_editor->scr);
}

/**
 * cui_set_current - Set the currently active screen and redraw it.
 */

struct nc_scr *cui_set_current(struct cui *cui, struct nc_scr *scr)
{
	struct nc_scr *old;

	DBGS("%p -> %p\n", cui->current, scr);

	assert(cui->current != scr);

	old = cui->current;
	old->unpost(old);

	cui->current = scr;
	cui->current->post(cui->current);

	return old;
}

static bool process_global_keys(struct cui *cui, int key)
{
	switch (key) {
	case 0xc:
		if (cui->current && cui->current->main_ncw) {
			redrawwin(cui->current->main_ncw);
			wrefresh(cui->current->main_ncw);
		}
		return true;
	}
	return false;
}

/**
 * cui_process_key - Process input on stdin.
 */

static int cui_process_key(void *arg)
{
	struct cui *cui = cui_from_arg(arg);

	assert(cui->current);

	if (!cui->has_input)
		discover_client_cancel_default(cui->client);
	cui->has_input = true;

	for (;;) {
		int c = getch();

		if (c == ERR)
			break;

		if (process_global_keys(cui, c))
			continue;

		cui->current->process_key(cui->current, c);
	}

	return 0;
}

/**
 * cui_process_js - Process joystick events.
 */

static int cui_process_js(void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	int c;

	c = pjs_process_event(cui->pjs);

	if (c) {
		ungetch(c);
		cui_process_key(arg);
	}

	return 0;
}

/**
 * cui_handle_resize - Handle the term resize.
 */

static void cui_handle_resize(struct cui *cui)
{
	struct winsize ws;

	if (ioctl(1, TIOCGWINSZ, &ws) == -1) {
		pb_log("%s: ioctl failed: %s\n", __func__, strerror(errno));
		return;
	}

	pb_log("%s: {%u,%u}\n", __func__, ws.ws_row, ws.ws_col);

	wclear(cui->current->main_ncw);
	resize_term(ws.ws_row, ws.ws_col);
	cui->current->resize(cui->current);

	/* For some reason this makes ncurses redraw the screen */
	getch();
	redrawwin(cui->current->main_ncw);
	wrefresh(cui->current->main_ncw);
}

/**
 * cui_device_add - Client device_add callback.
 *
 * Creates menu_items for all the device boot_options and inserts those
 * menu_items into the main menu.  Redraws the main menu if it is active.
 */

static int cui_boot_option_add(struct device *dev, struct boot_option *opt,
		void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	struct cui_opt_data *cod;
	unsigned int insert_pt;
	struct pmenu_item *i;
	ITEM *selected;
	int result;

	pb_log("%s: %p %s\n", __func__, opt, opt->id);

	selected = current_item(cui->main->ncm);

	if (cui->current == &cui->main->scr)
		cui->current->unpost(cui->current);

	/* This disconnects items array from menu. */

	result = set_menu_items(cui->main->ncm, NULL);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	/* Insert new items at insert_pt. */
	insert_pt = pmenu_grow(cui->main, 1);

	/* Save the item in opt->ui_info for cui_device_remove() */

	opt->ui_info = i = pmenu_item_alloc(cui->main);

	i->on_edit = cui_item_edit;
	i->on_execute = cui_boot;
	i->data = cod = talloc(i, struct cui_opt_data);

	cod->opt = opt;
	cod->dev = dev;
	cod->opt_hash = pb_opt_hash(dev, opt);
	cod->name = opt->name;
	cod->bd = talloc(i, struct pb_boot_data);

	cod->bd->image = talloc_strdup(cod->bd, opt->boot_image_file);
	cod->bd->initrd = talloc_strdup(cod->bd, opt->initrd_file);
	cod->bd->dtb = talloc_strdup(cod->bd, opt->dtb_file);
	cod->bd->args = talloc_strdup(cod->bd, opt->boot_args);

	pmenu_item_setup(cui->main, i, insert_pt, cod->name);

	pb_log("%s: adding opt '%s'\n", __func__, cod->name);
	pb_log("   image  '%s'\n", cod->bd->image);
	pb_log("   initrd '%s'\n", cod->bd->initrd);
	pb_log("   args   '%s'\n", cod->bd->args);

	/* Re-attach the items array. */
	result = set_menu_items(cui->main->ncm, cui->main->items);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	if (0) {
		pb_log("%s\n", __func__);
		pmenu_dump_items(cui->main->items,
			item_count(cui->main->ncm) + 1);
	}

	/* FIXME: need to make item visible somehow */
	menu_driver(cui->main->ncm, REQ_SCR_UPAGE);
	menu_driver(cui->main->ncm, REQ_SCR_DPAGE);
	set_current_item(cui->main->ncm, selected);

	if (cui->current == &cui->main->scr)
		cui->current->post(cui->current);

	return 0;
}

/**
 * cui_device_remove - Client device remove callback.
 *
 * Removes all the menu_items for the device from the main menu and redraws the
 * main menu if it is active.
 */

static void cui_device_remove(struct device *dev, void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	int result;
	struct boot_option *opt;

	pb_log("%s: %p %s\n", __func__, dev, dev->id);

	if (cui->current == &cui->main->scr)
		cui->current->unpost(cui->current);

	/* This disconnects items array from menu. */

	result = set_menu_items(cui->main->ncm, NULL);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	list_for_each_entry(&dev->boot_options, opt, list) {
		struct pmenu_item *i = pmenu_item_from_arg(opt->ui_info);

		assert(pb_protocol_device_cmp(dev, cod_from_item(i)->dev));
		pmenu_remove(cui->main, i);
	}

	/* Re-attach the items array. */

	result = set_menu_items(cui->main->ncm, cui->main->items);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	if (0) {
		pb_log("%s\n", __func__);
		pmenu_dump_items(cui->main->items,
			item_count(cui->main->ncm) + 1);
	}

	if (cui->current == &cui->main->scr)
		cui->current->post(cui->current);
}

static void cui_update_status(struct boot_status *status, void *arg)
{
	struct cui *cui = cui_from_arg(arg);

	nc_scr_status_printf(cui->current,
			"%s: %s",
			status->type == BOOT_STATUS_ERROR ? "Error" : "Info",
			status->message);

}

static void cui_update_mm_title(struct cui *cui)
{
	struct nc_frame *frame = &cui->main->scr.frame;

	talloc_free(frame->rtitle);

	frame->rtitle = talloc_strdup(cui->main, cui->sysinfo->type);
	if (cui->sysinfo->identifier)
		frame->rtitle = talloc_asprintf_append(frame->rtitle,
				" %s", cui->sysinfo->identifier);

	if (cui->current == &cui->main->scr)
		cui->current->post(cui->current);
}

static void cui_update_sysinfo(struct system_info *sysinfo, void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	cui->sysinfo = talloc_steal(cui, sysinfo);
	cui_update_mm_title(cui);
}

static struct discover_client_ops cui_client_ops = {
	.device_add = NULL,
	.boot_option_add = cui_boot_option_add,
	.device_remove = cui_device_remove,
	.update_status = cui_update_status,
	.update_sysinfo = cui_update_sysinfo,
};

/**
 * cui_init - Setup the cui instance.
 * @platform_info: A value for the struct cui platform_info member.
 *
 * Returns a pointer to a struct cui on success, or NULL on error.
 *
 * Allocates the cui instance, sets up the client and stdin waiters, and
 * sets up the ncurses menu screen.
 */

struct cui *cui_init(void* platform_info,
	int (*js_map)(const struct js_event *e), int start_deamon)
{
	struct cui *cui;
	unsigned int i;

	cui = talloc_zero(NULL, struct cui);

	if (!cui) {
		pb_log("%s: alloc cui failed.\n", __func__);
		fprintf(stderr, "%s: alloc cui failed.\n", __func__);
		goto fail_alloc;
	}

	cui->c_sig = pb_cui_sig;
	cui->platform_info = platform_info;
	cui->waitset = waitset_create(cui);

	process_init(cui, cui->waitset, false);

	setlocale(LC_ALL, "");

	/* Loop here for scripts that just started the server. */

retry_start:
	for (i = start_deamon ? 2 : 10; i; i--) {
		cui->client = discover_client_init(cui->waitset,
				&cui_client_ops, cui);
		if (cui->client || !i)
			break;
		pb_log("%s: waiting for server %d\n", __func__, i);
		sleep(1);
	}

	if (!cui->client && start_deamon) {
		int result;

		start_deamon = 0;

		result = pb_start_daemon(cui);

		if (!result)
			goto retry_start;

		pb_log("%s: discover_client_init failed.\n", __func__);
		fprintf(stderr, "%s: error: discover_client_init failed.\n",
			__func__);
		fprintf(stderr, "could not start pb-discover, the petitboot "
			"daemon.\n");
		goto fail_client_init;
	}

	if (!cui->client) {
		pb_log("%s: discover_client_init failed.\n", __func__);
		fprintf(stderr, "%s: error: discover_client_init failed.\n",
			__func__);
		fprintf(stderr, "check that pb-discover, "
			"the petitboot daemon is running.\n");
		goto fail_client_init;
	}

	atexit(nc_atexit);
	nc_start();

	waiter_register_io(cui->waitset, STDIN_FILENO, WAIT_IN,
			cui_process_key, cui);

	if (js_map) {

		cui->pjs = pjs_init(cui, js_map);

		if (cui->pjs)
			waiter_register_io(cui->waitset, pjs_get_fd(cui->pjs),
					WAIT_IN, cui_process_js, cui);
	}

	return cui;

fail_client_init:
	talloc_free(cui);
fail_alloc:
	return NULL;
}

/**
 * cui_run - The main cui program loop.
 * @cui: The cui instance.
 * @main: The menu to use as the main menu.
 *
 * Runs the cui engine.  Does not return until indicated to do so by some
 * user action, or an error occurs.  Frees the cui object on return.
 * Returns 0 on success (return to shell), -1 on error (should restart).
 */

int cui_run(struct cui *cui, struct pmenu *main, unsigned int default_item)
{
	assert(main);

	cui->main = main;
	cui->current = &cui->main->scr;
	cui->default_item = default_item;

	cui->current->post(cui->current);

	while (1) {
		int result = waiter_poll(cui->waitset);

		if (result < 0) {
			pb_log("%s: poll: %s\n", __func__, strerror(errno));
			break;
		}

		if (cui->abort)
			break;

		while (cui->resize) {
			cui->resize = 0;
			cui_handle_resize(cui);
		}
	}

	nc_atexit();

	return cui->abort ? 0 : -1;
}
