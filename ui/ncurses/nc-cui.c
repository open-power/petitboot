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
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>

#include "log/log.h"
#include "pb-protocol/pb-protocol.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "process/process.h"
#include "i18n/i18n.h"
#include "ui/common/discover-client.h"
#include "ui/common/ui-system.h"
#include "nc-cui.h"
#include "nc-boot-editor.h"
#include "nc-config.h"
#include "nc-add-url.h"
#include "nc-sysinfo.h"
#include "nc-lang.h"
#include "nc-helpscreen.h"
#include "nc-statuslog.h"
#include "nc-subset.h"
#include "nc-plugin.h"

extern const struct help_text main_menu_help_text;
extern const struct help_text plugin_menu_help_text;

static bool cui_detached = false;

static struct pmenu *main_menu_init(struct cui *cui);
static struct pmenu *plugin_menu_init(struct cui *cui);

static bool lockdown_active(void)
{
	bool lockdown = false;
	if (access(LOCKDOWN_FILE, F_OK) != -1)
		lockdown = true;
	return lockdown;
}

static void cui_start(void)
{
	initscr();			/* Initialize ncurses. */
	cbreak();			/* Disable line buffering. */
	noecho();			/* Disable getch() echo. */
	keypad(stdscr, TRUE);		/* Enable num keypad keys. */
	nonl();				/* Disable new-line translation. */
	intrflush(stdscr, FALSE);	/* Disable interrupt flush. */
	curs_set(0);			/* Make cursor invisible */
	nodelay(stdscr, TRUE);		/* Enable non-blocking getch() */

	/* We may be operating with an incorrect $TERM type; in this case
	 * the keymappings will be slightly broken. We want at least
	 * backspace to work though, so we'll define both DEL and ^H to
	 * map to backspace */
	define_key("\x7f", KEY_BACKSPACE);
	define_key("\x08", KEY_BACKSPACE);

	/* we need backtab too, for form navigation. vt220 doesn't include
	 * this (kcbt), but we don't want to require a full linux/xterm termcap
	 */
	define_key("\x1b[Z", KEY_BTAB);

	/* We'll define a few other keys too since they're commonly
	 * used for navigation but the escape character will cause
	 * Petitboot to exit if they're left undefined */
	define_key("\x1b\x5b\x35\x7e", KEY_PPAGE);
	define_key("\x1b\x5b\x36\x7e", KEY_NPAGE);
	define_key("\x1b\x5b\x31\x7e", KEY_HOME);
	define_key("\x1b\x5b\x34\x7e", KEY_END);
	define_key("\x1b\x4f\x48", KEY_HOME);
	define_key("\x1b\x4f\x46", KEY_END);
	define_key("OH", KEY_HOME);
	define_key("OF", KEY_END);
	define_key("\x1b\x5b\x41", KEY_UP);
	define_key("\x1b\x5b\x42", KEY_DOWN);
	define_key("\x1b\x5b\x33\x7e", KEY_DC);

	while (getch() != ERR)		/* flush stdin */
		(void)0;
}

static void cui_atexit(void)
{
	if (cui_detached)
		return;

	clear();
	refresh();
	endwin();

	bool lockdown = lockdown_active();

	while (lockdown) {
		sync();
		reboot(RB_AUTOBOOT);
	}
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
	pb_debug("%s: resizing\n", __func__);
	cui->resize = 1;
}

/**
 * cui_on_exit - A generic main menu exit callback.
 */

void cui_on_exit(struct pmenu *menu)
{
	struct cui *cui = cui_from_pmenu(menu);
	char *sh_cmd;

	sh_cmd = talloc_asprintf(cui,
		"echo \"Exiting petitboot. Type 'exit' to return.\";\
		 echo \"You may run 'pb-sos' to gather diagnostic data\";\
		 %s", pb_system_apps.sh);

	if (!sh_cmd) {
		pb_log("Failed to allocate shell arguments\n");
		return;
	}

	const char *argv[] = {
		pb_system_apps.sh,
		"-c",
		sh_cmd,
		NULL
	};

	cui_run_cmd(cui, argv);

	nc_scr_status_printf(cui->current, _("Returned from shell"));
	talloc_free(sh_cmd);
}

/**
 * cui_run_cmd - A generic cb to run the supplied command.
 */

int cui_run_cmd(struct cui *cui, const char **cmd_argv)
{
	struct process *process;
	int result;

	process = process_create(cui);
	if (!process)
		return -1;

	process->path = cmd_argv[0];
	process->argv = cmd_argv;
	process->raw_stdout = true;

	nc_scr_status_printf(cui->current, _("Running %s..."), cmd_argv[0]);

	nc_scr_unpost(cui->current);
	clear();
	refresh();

	def_prog_mode();
	endwin();

	result = process_run_sync(process);

	reset_prog_mode();
	refresh();

	redrawwin(cui->current->main_ncw);
	nc_scr_post(cui->current);

	if (result) {
		pb_log("%s: failed: '%s'\n", __func__, cmd_argv[0]);
		nc_scr_status_printf(cui->current, _("Failed: %s"),
				cmd_argv[0]);
	}

	process_release(process);

	return result;
}

int cui_run_cmd_from_item(struct pmenu_item *item)
{
	struct cui *cui = cui_from_item(item);
	const char **cmd_argv = item->data;

	return cui_run_cmd(cui, cmd_argv);
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

	pb_debug("%s: %s\n", __func__, cod->name);

	nc_scr_status_printf(cui->current, _("Booting %s..."), cod->name);

	result = discover_client_boot(cui->client, NULL, cod->opt, cod->bd);

	if (result) {
		nc_scr_status_printf(cui->current,
				_("Failed: boot %s"), cod->bd->image);
	}

	return 0;
}

static void cui_boot_editor_on_exit(struct cui *cui,
		struct pmenu_item *item,
		struct pb_boot_data *bd)
{
	struct pmenu *menu = cui->main;
	struct cui_opt_data *cod;
	int idx, top, rows, cols;
	static int user_idx = 0;

	/* Was the edit cancelled? */
	if (!bd) {
		cui_set_current(cui, &cui->main->scr);
		talloc_free(cui->boot_editor);
		cui->boot_editor = NULL;
		return;
	}

	/* Is this was a new item, we'll need to update the menu */
	if (!item) {
		int insert_pt;

		cod = talloc_zero(NULL, struct cui_opt_data);
		cod->name = talloc_asprintf(cod, _("User item %u"), ++user_idx);

		item = pmenu_item_create(menu, cod->name);
		if (!item) {
			talloc_free(cod);
			goto out;
		}

		item->on_edit = cui_item_edit;
		item->on_execute = cui_boot;
		item->data = cod;

		talloc_steal(item, cod);

		/* Detach the items array. */
		set_menu_items(menu->ncm, NULL);

		/* Insert new item at insert_pt. */
		insert_pt = pmenu_grow(menu, 1);
		pmenu_item_insert(menu, item, insert_pt);

		/* Re-attach the items array. */
		set_menu_items(menu->ncm, menu->items);

		/* If our index is above the current top row, align
		 * us to the new top. Otherwise, align us to the new
		 * bottom */
		menu_format(cui->main->ncm, &rows, &cols);
		top = top_row(cui->main->ncm);
		idx = item_index(item->nci);

		if (top >= idx)
			top = idx;
		else
			top = idx < rows ? 0 : idx - rows + 1;

		set_top_row(cui->main->ncm, top);
		set_current_item(item->pmenu->ncm, item->nci);

		nc_scr_post(&menu->scr);
	} else {
		cod = item->data;
	}

	cod->bd = talloc_steal(cod, bd);

out:
	cui_set_current(cui, &cui->main->scr);
	talloc_free(cui->boot_editor);
	cui->boot_editor = NULL;
}

void cui_item_edit(struct pmenu_item *item)
{
	struct cui *cui = cui_from_item(item);
	cui->boot_editor = boot_editor_init(cui, item, cui->sysinfo,
					cui_boot_editor_on_exit);
	cui_set_current(cui, boot_editor_scr(cui->boot_editor));
}

void cui_item_new(struct pmenu *menu)
{
	struct cui *cui = cui_from_pmenu(menu);
	cui->boot_editor = boot_editor_init(cui, NULL, cui->sysinfo,
					cui_boot_editor_on_exit);
	cui_set_current(cui, boot_editor_scr(cui->boot_editor));
}

static void cui_sysinfo_exit(struct cui *cui)
{
	cui_set_current(cui, &cui->main->scr);
	talloc_free(cui->sysinfo_screen);
	cui->sysinfo_screen = NULL;
}

void cui_show_sysinfo(struct cui *cui)
{
	cui->sysinfo_screen = sysinfo_screen_init(cui, cui->sysinfo,
			cui_sysinfo_exit);
	cui_set_current(cui, sysinfo_screen_scr(cui->sysinfo_screen));
}

static void cui_config_exit(struct cui *cui)
{
	cui_set_current(cui, &cui->main->scr);
	talloc_free(cui->config_screen);
	cui->config_screen = NULL;
}

void cui_show_config(struct cui *cui)
{
	cui->config_screen = config_screen_init(cui, cui->config,
			cui->sysinfo, cui_config_exit);
	cui_set_current(cui, config_screen_scr(cui->config_screen));
}

static void cui_lang_exit(struct cui *cui)
{
	cui_set_current(cui, &cui->main->scr);
	talloc_free(cui->lang_screen);
	cui->lang_screen = NULL;
}

void cui_show_lang(struct cui *cui)
{
	cui->lang_screen = lang_screen_init(cui, cui->config, cui_lang_exit);
	cui_set_current(cui, lang_screen_scr(cui->lang_screen));
}

static void cui_statuslog_exit(struct cui *cui)
{
	cui_set_current(cui, &cui->main->scr);
	talloc_free(cui->statuslog_screen);
	cui->statuslog_screen = NULL;
}

void cui_show_statuslog(struct cui *cui)
{
	cui->statuslog_screen = statuslog_screen_init(cui, cui_statuslog_exit);
	cui_set_current(cui, statuslog_screen_scr(cui->statuslog_screen));
}

static void cui_add_url_exit(struct cui *cui)
{
	cui_set_current(cui, &cui->main->scr);
	talloc_free(cui->add_url_screen);
	cui->add_url_screen = NULL;
}

static void cui_plugin_exit(struct cui *cui)
{
	cui_set_current(cui, &cui->plugin_menu->scr);
	talloc_free(cui->plugin_screen);
	cui->plugin_screen = NULL;
}

static void cui_plugin_menu_exit(struct pmenu *menu)
{
	struct cui *cui = cui_from_pmenu(menu);
	cui_set_current(cui, &cui->main->scr);
}

void cui_show_add_url(struct cui *cui)
{
	cui->add_url_screen = add_url_screen_init(cui, cui_add_url_exit);
	cui_set_current(cui, add_url_screen_scr(cui->add_url_screen));
}

void cui_show_plugin_menu(struct cui *cui)
{
	cui_set_current(cui, &cui->plugin_menu->scr);
}

void cui_show_plugin(struct pmenu_item *item)
{
	struct cui *cui = cui_from_item(item);
	cui->plugin_screen = plugin_screen_init(cui, item, cui_plugin_exit);
	cui_set_current(cui, plugin_screen_scr(cui->plugin_screen));
}

static void cui_help_exit(struct cui *cui)
{
	cui_set_current(cui, help_screen_return_scr(cui->help_screen));
	talloc_free(cui->help_screen);
	cui->help_screen = NULL;
}

void cui_show_help(struct cui *cui, const char *title,
		const struct help_text *text)
{
	if (!cui->current)
		return;

	if (cui->help_screen)
		return;

	cui->help_screen = help_screen_init(cui, cui->current,
			title, text, cui_help_exit);

	if (cui->help_screen)
		cui_set_current(cui, help_screen_scr(cui->help_screen));
}

static void cui_subset_exit(struct cui *cui)
{
	cui_set_current(cui, subset_screen_return_scr(cui->subset_screen));
	talloc_free(cui->subset_screen);
	cui->subset_screen = NULL;
}

void cui_show_subset(struct cui *cui, const char *title,
		     void *arg)
{
	if (!cui->current)
		return;

	if (cui->subset_screen)
		return;

	cui->subset_screen = subset_screen_init(cui, cui->current,
			title, arg, cui_subset_exit);

	if (cui->subset_screen)
		cui_set_current(cui, subset_screen_scr(cui->subset_screen));
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
	nc_scr_unpost(old);

	cui->current = scr;

	nc_scr_post(cui->current);

	return old;
}

static bool process_global_keys(struct cui *cui, int key)
{
	switch (key) {
	case 0xc:
		if (cui->current && cui->current->main_ncw)
			wrefresh(curscr);
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

	for (;;) {
		int c = getch();

		pb_debug("%s: got key %d\n", __func__, c);

		if (c == ERR)
			break;

		if (!cui->has_input) {
			cui->has_input = true;
			if (cui->client) {
				pb_log("UI input received (key = %d), aborting "
					"default boot\n", c);
				discover_client_cancel_default(cui->client);
			} else {
				pb_log("UI input received (key = %d), aborting "
					"once server connects\n", c);
			}
		}

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

	pb_debug("%s: {%u,%u}\n", __func__, ws.ws_row, ws.ws_col);

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
 * If a 'plugin' type boot_option appears the plugin menu is updated instead.
 */

static int cui_boot_option_add(struct device *dev, struct boot_option *opt,
		void *arg)
{
	struct pmenu_item *i, *dev_hdr = NULL;
	struct cui *cui = cui_from_arg(arg);
	struct cui_opt_data *cod;
	const char *tab = "  ";
	unsigned int insert_pt;
	int result, rows, cols;
	struct pmenu *menu;
	bool plugin_option;
	ITEM *selected;
	char *name;

	plugin_option = opt->type == DISCOVER_PLUGIN_OPTION;
	menu = plugin_option ? cui->plugin_menu : cui->main;

	pb_debug("%s: %p %s\n", __func__, opt, opt->id);

	selected = current_item(menu->ncm);
	menu_format(menu->ncm, &rows, &cols);

	if (cui->current == &cui->main->scr)
		nc_scr_unpost(cui->current);
	if (plugin_option && cui->current == &cui->plugin_menu->scr)
		nc_scr_unpost(cui->current);

	/* Check if the boot device is new */
	dev_hdr = pmenu_find_device(menu, dev, opt);

	/* All actual boot entries are 'tabbed' across */
	name = talloc_asprintf(menu, "%s%s",
			tab, opt->name ? : "Unknown Name");

	/* Save the item in opt->ui_info for cui_device_remove() */
	opt->ui_info = i = pmenu_item_create(menu, name);
	talloc_free(name);
	if (!i)
		return -1;

	if (plugin_option) {
		i->on_edit = NULL;
		i->on_execute = plugin_install_plugin;
	} else {
		i->on_edit = cui_item_edit;
		i->on_execute = cui_boot;
	}

	i->data = cod = talloc(i, struct cui_opt_data);
	cod->opt = opt;
	cod->dev = dev;
	cod->opt_hash = pb_opt_hash(dev, opt);
	cod->name = opt->name;

	if (plugin_option) {
		cod->pd = talloc(i, struct pb_plugin_data);
		cod->pd->plugin_file = talloc_strdup(cod,
				opt->boot_image_file);
	} else {
		cod->bd = talloc(i, struct pb_boot_data);
		cod->bd->image = talloc_strdup(cod->bd, opt->boot_image_file);
		cod->bd->initrd = talloc_strdup(cod->bd, opt->initrd_file);
		cod->bd->dtb = talloc_strdup(cod->bd, opt->dtb_file);
		cod->bd->args = talloc_strdup(cod->bd, opt->boot_args);
		cod->bd->args_sig_file = talloc_strdup(cod->bd, opt->args_sig_file);
	}

	/* This disconnects items array from menu. */
	result = set_menu_items(menu->ncm, NULL);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	/* Insert new items at insert_pt. */
	if (dev_hdr) {
		insert_pt = pmenu_grow(menu, 2);
		pmenu_item_insert(menu, dev_hdr, insert_pt);
		pb_log("%s: adding new device hierarchy %s\n",
			__func__, opt->device_id);
		pmenu_item_insert(menu, i, insert_pt+1);
	} else {
		insert_pt = pmenu_grow(menu, 1);
		pmenu_item_add(menu, i, insert_pt);
	}

	if (plugin_option) {
		pb_log("%s: adding plugin '%s'\n", __func__, cod->name);
		pb_log("   file  '%s'\n", cod->pd->plugin_file);
	} else {
		pb_log("%s: adding opt '%s'\n", __func__, cod->name);
		pb_log("   image  '%s'\n", cod->bd->image);
		pb_log("   initrd '%s'\n", cod->bd->initrd);
		pb_log("   args   '%s'\n", cod->bd->args);
		pb_log("   argsig '%s'\n", cod->bd->args_sig_file);
	}

	/* Update the plugin menu label if needed */
	if (plugin_option) {
		struct pmenu_item *item;
		unsigned int j;
		result = set_menu_items(cui->main->ncm, NULL);
		for (j = 0 ; j < cui->main->item_count; j++) {
			item = item_userptr(cui->main->items[j]);
			if (strncmp(item->nci->name.str, "Plugins", strlen("Plugins")))
				continue;
			cui->n_plugins++;
			char *label = talloc_asprintf(item, "Plugins (%u)",
					cui->n_plugins);
			pmenu_item_update(item, label);
			talloc_free(label);
			break;
		}
		result = set_menu_items(cui->main->ncm, cui->main->items);
		if (result)
			pb_log("%s: set_menu_items failed: %d\n", __func__, result);
	}

	/* Re-attach the items array. */
	result = set_menu_items(menu->ncm, menu->items);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	if (0) {
		pb_log("%s\n", __func__);
		pmenu_dump_items(menu->items,
			item_count(menu->ncm) + 1);
	}

	if (!item_visible(selected)) {
		int idx, top;

		top = top_row(menu->ncm);
		idx = item_index(selected);

		/* If our index is above the current top row, align
		 * us to the new top. Otherwise, align us to the new
		 * bottom */
		top = top < idx ? idx - rows + 1 : idx;

		set_top_row(menu->ncm, top);
		set_current_item(menu->ncm, selected);
	}

	if (cui->current == &menu->scr)
		nc_scr_post(cui->current);
	if (plugin_option && cui->current == &cui->main->scr)
		nc_scr_post(cui->current);

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
	struct boot_option *opt;
	unsigned int i;
	int rows, cols, top, last;
	int result;

	pb_log("%s: %p %s\n", __func__, dev, dev->id);

	if (cui->current == &cui->main->scr)
		nc_scr_unpost(cui->current);
	if (cui->current == &cui->plugin_menu->scr)
		nc_scr_unpost(cui->current);

	/* This disconnects items array from menu. */

	result = set_menu_items(cui->main->ncm, NULL);
	result |= set_menu_items(cui->plugin_menu->ncm, NULL);

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	list_for_each_entry(&dev->boot_options, opt, list) {
		struct pmenu_item *item = pmenu_item_from_arg(opt->ui_info);

		assert(pb_protocol_device_cmp(dev, cod_from_item(item)->dev));
		if (opt->type == DISCOVER_PLUGIN_OPTION)
			pmenu_remove(cui->plugin_menu, item);
		else
			pmenu_remove(cui->main, item);
	}

	/* Manually remove remaining device hierarchy item */
	for (i=0; i < cui->main->item_count; i++) {
		struct pmenu_item *item = item_userptr(cui->main->items[i]);
		if (!item || !item->data )
			continue;

		struct cui_opt_data *data = item->data;
		if (data && data->dev && data->dev == dev)
			pmenu_remove(cui->main,item);
	}
	/* Look in plugins menu too */
	for (i=0; i < cui->plugin_menu->item_count; i++) {
		struct pmenu_item *item = item_userptr(cui->plugin_menu->items[i]);
		if (!item || !item->data )
			continue;

		struct cui_opt_data *data = item->data;
		if (data && data->dev && data->dev == dev)
			pmenu_remove(cui->plugin_menu,item);
	}

	/* Re-attach the items array. */

	result = set_menu_items(cui->main->ncm, cui->main->items);
	result |= set_menu_items(cui->plugin_menu->ncm, cui->plugin_menu->items);

	/* Move cursor to 'Exit' menu entry for the main menu.. */
	menu_format(cui->main->ncm, &rows, &cols);
	last = cui->main->item_count - 1;
	set_current_item(cui->main->ncm, cui->main->items[last]);
	if (!item_visible(cui->main->items[last])) {
		top = last < rows ? 0 : last - rows + 1;
		set_top_row(cui->main->ncm, top);
	}

	/* ..and the plugin menu */
	menu_format(cui->plugin_menu->ncm, &rows, &cols);
	last = cui->plugin_menu->item_count - 1;
	set_current_item(cui->plugin_menu->ncm, cui->plugin_menu->items[last]);
	if (!item_visible(cui->plugin_menu->items[last])) {
		top = last < rows ? 0 : last - rows + 1;
		set_top_row(cui->plugin_menu->ncm, top);
	}

	if (result)
		pb_log("%s: set_menu_items failed: %d\n", __func__, result);

	if (0) {
		pb_log("%s\n", __func__);
		pmenu_dump_items(cui->main->items,
			item_count(cui->main->ncm) + 1);
	}

	if (cui->current == &cui->main->scr)
		nc_scr_post(cui->current);
	if (cui->current == &cui->plugin_menu->scr)
		nc_scr_post(cui->current);
}

static void cui_update_status(struct status *status, void *arg)
{
	struct cui *cui = cui_from_arg(arg);

	statuslog_append_steal(cui, cui->statuslog, status);

	/* Ignore status messages from the backlog */
	if (!status->backlog)
		nc_scr_status_printf(cui->current, "%s", status->message);
}

/*
 * Handle a new installed plugin option and update its associated
 * (uninstalled) menu item if it exists.
 */
static int cui_plugin_option_add(struct plugin_option *opt, void *arg)
{
	struct cui_opt_data *cod;
	struct cui *cui = cui_from_arg(arg);
	struct pmenu_item *item = NULL;
	struct boot_option *dummy_opt;
	struct device *dev;
	unsigned int i;
	int result;

fallback:
	/* Find uninstalled plugin by matching on plugin_file */
	for (i = 0; i < cui->plugin_menu->item_count; i++) {
		item = item_userptr(cui->plugin_menu->items[i]);
		if (!item)
			continue;
		cod = cod_from_item(item);
		if (!cod || !cod->pd)
			continue;
		if (strncmp(cod->pd->plugin_file, opt->plugin_file,
					strlen(cod->pd->plugin_file)) == 0)
			break;
	}

	/*
	 * If pb-plugin was run manually there may not be an associated
	 * plugin-type boot_option. Pass a fake device and option to
	 * cui_boot_option_add() so we have an item to work with.
	 */
	if (!item || i >= cui->plugin_menu->item_count) {
		pb_log("New plugin option %s doesn't have a source item\n",
				opt->id);
		dev = talloc_zero(cui, struct device);
		dev->id = dev->name = talloc_asprintf(dev, "(unknown)");
		dev->type = DEVICE_TYPE_UNKNOWN;
		dummy_opt = talloc_zero(cui, struct boot_option);
		dummy_opt->device_id = talloc_strdup(dummy_opt, dev->id);
		dummy_opt->id = dummy_opt->name = talloc_asprintf(dummy_opt, "dummy");
		dummy_opt->boot_image_file = talloc_strdup(dummy_opt, opt->plugin_file);
		dummy_opt->type = DISCOVER_PLUGIN_OPTION;
		cui_boot_option_add(dev, dummy_opt, cui);
		goto fallback;
	}

	/*
	 * If this option was faked above move the context under
	 * the item so it is cleaned up later in cui_plugins_remove().
	 */
	if (strncmp(cod->opt->id, "dummy", strlen("dummy") == 0 &&
				cod->dev->type == DEVICE_TYPE_UNKNOWN)) {
		talloc_steal(item, cod->dev);
		talloc_steal(item, cod->opt);
	}

	talloc_free(cod->name);
	/* Name is still tabbed across */
	cod->name = talloc_asprintf(cod, _("  %s [installed]"), opt->name);

	cod->pd->opt = opt;
	item->on_execute = NULL;
	item->on_edit = cui_show_plugin;

	if (cui->current == &cui->plugin_menu->scr)
		nc_scr_unpost(cui->current);

	/* This disconnects items array from menu. */
	result = set_menu_items(cui->plugin_menu->ncm, NULL);

	if (result == E_OK)
		pmenu_item_update(item, cod->name);

	/* Re-attach the items array. */
	result = set_menu_items(cui->plugin_menu->ncm, cui->plugin_menu->items);

	if (cui->current == &cui->plugin_menu->scr)
		nc_scr_post(cui->current);

	return result;
}

/*
 * Most plugin menu items will be removed via cui_device_remove(). However if
 * pb-plugin has been run manually it is possible that there a plugin items
 * not associated with a device - remove them here.
 */
static int cui_plugins_remove(void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	struct pmenu_item *item = NULL;
	struct cui_opt_data *cod;
	unsigned int i = 0;

	pb_debug("%s\n", __func__);

	if (cui->current == &cui->plugin_menu->scr)
		nc_scr_unpost(cui->current);
	if (cui->current == &cui->main->scr)
		nc_scr_unpost(cui->current);

	/* This disconnects items array from menu. */
	set_menu_items(cui->plugin_menu->ncm, NULL);

	while (i < cui->plugin_menu->item_count) {
		item = item_userptr(cui->plugin_menu->items[i]);
		if (!item || !item->data) {
			i++;
			continue;
		}
		cod = cod_from_item(item);
		if (!cod->opt && !cod->dev) {
			i++;
			continue;
		}

		pmenu_remove(cui->plugin_menu, item);
		/* plugin_menu entries will shift, jump to bottom to make sure
		 * we remove all plugin option items */
		i = 0;
	}

	/* Re-attach the items array. */
	set_menu_items(cui->plugin_menu->ncm, cui->plugin_menu->items);

	set_menu_items(cui->main->ncm, NULL);
	for (i = 0; i < cui->main->item_count; i++) {
		item = item_userptr(cui->main->items[i]);
		if (strncmp(item->nci->name.str, "Plugins", strlen("Plugins")))
			continue;
		cui->n_plugins = 0;
		pmenu_item_update(item, "Plugins (0)");
		break;
	}

	set_menu_items(cui->main->ncm, cui->main->items);

	if (cui->current == &cui->main->scr)
		nc_scr_post(cui->current);

	/* If we're currently in a plugin screen jump back to the plugin menu */
	if (cui->plugin_screen &&
			cui->current == plugin_screen_scr(cui->plugin_screen))
		cui_plugin_exit(cui);

	return 0;
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
		nc_scr_post(cui->current);

	frame = &cui->plugin_menu->scr.frame;

	talloc_free(frame->rtitle);

	frame->rtitle = talloc_strdup(cui->main, cui->sysinfo->type);
	if (cui->sysinfo->identifier)
		frame->rtitle = talloc_asprintf_append(frame->rtitle,
				" %s", cui->sysinfo->identifier);

	if (cui->current == &cui->main->scr)
		nc_scr_post(cui->current);
}

static void cui_update_sysinfo(struct system_info *sysinfo, void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	cui->sysinfo = talloc_steal(cui, sysinfo);

	/* if we're currently displaying the system info screen, inform it
	 * of the updated information. */
	if (cui->sysinfo_screen)
		sysinfo_screen_update(cui->sysinfo_screen, sysinfo);

	if (cui->subset_screen)
		subset_screen_update(cui->subset_screen);

	/* ... and do the same with the config screen... */
	if (cui->config_screen)
		config_screen_update(cui->config_screen, cui->config, sysinfo);

	/* ... and the boot editor. */
	if (cui->boot_editor)
		boot_editor_update(cui->boot_editor, sysinfo);

	cui_update_mm_title(cui);
}

static void cui_update_language(struct cui *cui, char *lang)
{
	bool repost_menu;
	char *cur_lang;

	cur_lang = setlocale(LC_ALL, NULL);
	if (cur_lang && !strcmp(cur_lang, lang))
		return;

	setlocale(LC_ALL, lang);

	/* we'll need to update the menu: drop all items and repopulate */
	repost_menu = cui->current == &cui->main->scr ||
		cui->current == &cui->plugin_menu->scr;
	if (repost_menu)
		nc_scr_unpost(cui->current);

	talloc_free(cui->main);
	cui->main = main_menu_init(cui);
	cui->plugin_menu = plugin_menu_init(cui);

	if (repost_menu) {
		cui->current = &cui->main->scr;
		nc_scr_post(cui->current);
	}

	discover_client_enumerate(cui->client);
}

static void cui_update_config(struct config *config, void *arg)
{
	struct cui *cui = cui_from_arg(arg);
	cui->config = talloc_steal(cui, config);

	if (config->lang)
		cui_update_language(cui, config->lang);

	if (cui->subset_screen)
		subset_screen_update(cui->subset_screen);

	if (cui->config_screen)
		config_screen_update(cui->config_screen, config, cui->sysinfo);

	if (config->safe_mode)
		nc_scr_status_printf(cui->current,
				_("SAFE MODE: select '%s' to continue"),
				_("Rescan devices"));
}

int cui_send_config(struct cui *cui, struct config *config)
{
	return discover_client_send_config(cui->client, config);
}

int cui_send_url(struct cui *cui, char * url)
{
	return discover_client_send_url(cui->client, url);
}

int cui_send_plugin_install(struct cui *cui, char *file)
{
	return discover_client_send_plugin_install(cui->client, file);
}

void cui_send_reinit(struct cui *cui)
{
	discover_client_send_reinit(cui->client);
}

static int menu_sysinfo_execute(struct pmenu_item *item)
{
	cui_show_sysinfo(cui_from_item(item));
	return 0;
}

static int menu_config_execute(struct pmenu_item *item)
{
	cui_show_config(cui_from_item(item));
	return 0;
}

static int menu_lang_execute(struct pmenu_item *item)
{
	cui_show_lang(cui_from_item(item));
	return 0;
}

static int menu_statuslog_execute(struct pmenu_item *item)
{
	cui_show_statuslog(cui_from_item(item));
	return 0;
}

static int menu_reinit_execute(struct pmenu_item *item)
{
	if (cui_from_item(item)->client)
		cui_send_reinit(cui_from_item(item));
	return 0;
}

static int menu_add_url_execute(struct pmenu_item *item)
{
	if (cui_from_item(item)->client)
		cui_show_add_url(cui_from_item(item));
	return 0;
}

static int menu_plugin_execute(struct pmenu_item *item)
{
	if (cui_from_item(item)->client)
		cui_show_plugin_menu(cui_from_item(item));
	return 0;
}

/**
 * pb_mm_init - Setup the main menu instance.
 */
static struct pmenu *main_menu_init(struct cui *cui)
{
	struct pmenu_item *i;
	struct pmenu *m;
	int result;
	bool lockdown = lockdown_active();

	m = pmenu_init(cui, 9, cui_on_exit);
	if (!m) {
		pb_log("%s: failed\n", __func__);
		return NULL;
	}

	m->on_new = cui_item_new;

	m->scr.frame.ltitle = talloc_asprintf(m,
		"Petitboot (" PACKAGE_VERSION ")");
	m->scr.frame.rtitle = NULL;
	m->scr.frame.help = talloc_strdup(m,
		_("Enter=accept, e=edit, n=new, x=exit, l=language, g=log, h=help"));
	m->scr.frame.status = talloc_strdup(m, _("Welcome to Petitboot"));

	/* add a separator */
	i = pmenu_item_create(m, " ");
	item_opts_off(i->nci, O_SELECTABLE);
	pmenu_item_insert(m, i, 0);

	/* add system items */
	i = pmenu_item_create(m, _("System information"));
	i->on_execute = menu_sysinfo_execute;
	pmenu_item_insert(m, i, 1);

	i = pmenu_item_create(m, _("System configuration"));
	i->on_execute = menu_config_execute;
	pmenu_item_insert(m, i, 2);

	i = pmenu_item_create(m, _("System status log"));
	i->on_execute = menu_statuslog_execute;
	pmenu_item_insert(m, i, 3);

	/* this label isn't translated, so we don't want a gettext() here */
	i = pmenu_item_create(m, "Language");
	i->on_execute = menu_lang_execute;
	pmenu_item_insert(m, i, 4);

	i = pmenu_item_create(m, _("Rescan devices"));
	i->on_execute = menu_reinit_execute;
	pmenu_item_insert(m, i, 5);

	i = pmenu_item_create(m, _("Retrieve config from URL"));
	i->on_execute = menu_add_url_execute;
	pmenu_item_insert(m, i, 6);

	i = pmenu_item_create(m, _("Plugins (0)"));
	i->on_execute = menu_plugin_execute;
	pmenu_item_insert(m, i, 7);

	if (lockdown)
		i = pmenu_item_create(m, _("Reboot"));
	else
		i = pmenu_item_create(m, _("Exit to shell"));
	i->on_execute = pmenu_exit_cb;
	pmenu_item_insert(m, i, 8);

	result = pmenu_setup(m);

	if (result) {
		pb_log("%s:%d: pmenu_setup failed: %s\n", __func__, __LINE__,
			strerror(errno));
		goto fail_setup;
	}

	m->help_title = _("main menu");
	m->help_text = &main_menu_help_text;

	menu_opts_off(m->ncm, O_SHOWDESC);
	set_menu_mark(m->ncm, " *");
	set_current_item(m->ncm, i->nci);

	return m;

fail_setup:
	talloc_free(m);
	return NULL;
}

/*
 * plugin_menu_init: Set up the plugin menu instance
 */
static struct pmenu *plugin_menu_init(struct cui *cui)
{
	struct pmenu_item *i;
	struct pmenu *m;
	int result;

	m = pmenu_init(cui, 2, cui_plugin_menu_exit);
	m->on_new = cui_item_new;
	m->scr.frame.ltitle = talloc_asprintf(m, _("Petitboot Plugins"));
	m->scr.frame.rtitle = talloc_asprintf(m, NULL);
	m->scr.frame.help = talloc_strdup(m,
		_("Enter=install, e=details, x=exit, h=help"));
	m->scr.frame.status = talloc_asprintf(m,
			_("Available Petitboot Plugins"));

	/* add a separator */
	i = pmenu_item_create(m, " ");
	item_opts_off(i->nci, O_SELECTABLE);
	pmenu_item_insert(m, i, 0);

	i = pmenu_item_create(m, _("Return to Main Menu"));
	i->on_execute = pmenu_exit_cb;
	pmenu_item_insert(m, i, 1);

	result = pmenu_setup(m);

	if (result) {
		pb_log("%s:%d: pmenu_setup failed: %s\n", __func__, __LINE__,
			strerror(errno));
		goto fail_setup;
	}

	m->help_title = _("plugin menu");
	m->help_text = &plugin_menu_help_text;

	return m;

fail_setup:
	talloc_free(m);
	return NULL;
}

static struct discover_client_ops cui_client_ops = {
	.device_add = NULL,
	.boot_option_add = cui_boot_option_add,
	.device_remove = cui_device_remove,
	.plugin_option_add = cui_plugin_option_add,
	.plugins_remove = cui_plugins_remove,
	.update_status = cui_update_status,
	.update_sysinfo = cui_update_sysinfo,
	.update_config = cui_update_config,
};

/* cui_server_wait_on_exit - On exit spin until the server is available.
 *
 * If the program exits before connecting to the server autoboot won't be
 * cancelled even though there has been keyboard activity. This function is
 * called by a child process which will spin until the server is connected and
 * told to cancel autoboot.
 *
 * Processes exiting from this function will not carry out the cui_atexit()
 * steps.
 */
static void cui_server_wait_on_exit(struct cui *cui)
{
	cui_detached = true;

	while (!cui->client) {
		cui->client = discover_client_init(cui->waitset,
				&cui_client_ops, cui);
		if (!cui->client)
			sleep(1);
	}

	talloc_steal(cui, cui->client);
	discover_client_cancel_default(cui->client);
}

/* cui_server_wait - Connect to the discover server.
 * @arg: Pointer to the cui instance.
 *
 * A timeout callback that attempts to connect to the discover server; on
 * failure it registers itself with a one second timeout to try again.
 * On success the cui->client struct will be set.
 *
 * Since this updates the status line when called it must not be called
 * before the UI is ready.
 */
static int cui_server_wait(void *arg)
{
	struct cui *cui = cui_from_arg(arg);

	if (cui->client) {
		pb_debug("We already have a server!\n");
		return 0;
	}

	/* We haven't yet connected to the server */
	pb_log("Trying to connect...\n");
	cui->client = discover_client_init(cui->waitset,
			&cui_client_ops, cui);

	if (!cui->client) {
		waiter_register_timeout(cui->waitset, 1000, cui_server_wait,
					cui);
		nc_scr_status_printf(cui->current,
				     "Info: Waiting for device discovery");
	} else {
		nc_scr_status_free(cui->current);
		talloc_steal(cui, cui->client);

		if (cui->has_input) {
			pb_log("Aborting default boot on pb-discover connect\n");
			discover_client_cancel_default(cui->client);
		}
	}

	return 0;
}

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
	int (*js_map)(const struct js_event *e), int start_daemon, int timeout)
{
	struct cui *cui;
	unsigned int i;

	cui = talloc_zero(NULL, struct cui);
	if (!cui) {
		pb_log("%s: alloc cui failed.\n", __func__);
		fprintf(stderr, _("%s: alloc cui failed.\n"), __func__);
		goto fail_alloc;
	}

	cui->c_sig = pb_cui_sig;
	cui->platform_info = platform_info;
	cui->waitset = waitset_create(cui);
	cui->statuslog = statuslog_init(cui);

	process_init(cui, cui->waitset, false);

	/* Loop here for scripts that just started the server. */

retry_start:
	for (i = start_daemon ? 2 : 15; i && timeout; i--) {
		cui->client = discover_client_init(cui->waitset,
				&cui_client_ops, cui);
		if (cui->client || !i)
			break;
		pb_log("%s: waiting for server %d\n", __func__, i);
		sleep(1);
	}

	if (!cui->client && start_daemon) {
		int result;

		start_daemon = 0;

		result = pb_start_daemon(cui);

		if (!result)
			goto retry_start;

		pb_log("%s: discover_client_init failed.\n", __func__);
		fprintf(stderr, _("%s: error: discover_client_init failed.\n"),
			__func__);
		fprintf(stderr, _("could not start pb-discover, the petitboot "
			"daemon.\n"));
		goto fail_client_init;
	}

	if (!cui->client && !timeout) {
		/* Have the first timeout fire immediately so we can check
		 * for the server as soon as the UI is ready */
		waiter_register_timeout(cui->waitset, 0,
					cui_server_wait, cui);
	} else if (!cui->client) {
		pb_log("%s: discover_client_init failed.\n", __func__);
		fprintf(stderr, _("%s: error: discover_client_init failed.\n"),
			__func__);
		fprintf(stderr, _("check that pb-discover, "
			"the petitboot daemon is running.\n"));
		goto fail_client_init;
	}

	atexit(cui_atexit);
	talloc_steal(cui, cui->client);
	cui_start();

	cui->main = main_menu_init(cui);
	if (!cui->main)
		goto fail_client_init;

	cui->plugin_menu = plugin_menu_init(cui);
	if (!cui->plugin_menu)
		goto fail_client_init;

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

int cui_run(struct cui *cui)
{
	pid_t pid;

	assert(main);

	cui->current = &cui->main->scr;
	cui->default_item = 0;

	nc_scr_post(cui->current);

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

	cui_atexit();

	if (!cui->client) {
		/* Fork a child to tell the server to cancel autoboot */
		pid = fork();
		if (!pid) {
			cui_server_wait_on_exit(cui);
			exit(EXIT_SUCCESS);
		}
		if (pid < 0)
			pb_log("Failed to fork child on exit: %m\n");
	}

	return cui->abort ? 0 : -1;
}
