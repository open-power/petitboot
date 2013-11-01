/*
 * Petitboot twin bootloader
 *
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

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "ui/common/timer.h"

#include "pbt-client.h"
#include "pbt-main.h"


static struct pbt_client *client_from_item(struct pbt_item *item)
{
	return item->data;
}

static int exit_to_shell_cb(struct pbt_item *item)
{
	struct pbt_client *client = client_from_item(item);

	client->signal_data.abort = 1;
	return 0;
}

static int edit_preferences_cb(struct pbt_item *item)
{
	struct pbt_client *client = client_from_item(item);

	(void)client;

	pb_debug("%s: TODO\n", __func__);

	return 0;
}

static struct pbt_item *setup_system_item(struct pbt_menu *menu,
	struct pbt_client *client)
{
	struct pbt_item *top_item;
	struct pbt_item *sub_item;
	struct pbt_quad q;

	top_item = pbt_item_create_reduced(menu, "system", 0,
		PB_ARTWORK_PATH "/applications-system.png");

	if (!top_item)
		goto fail_top_item_create;

	/* sub_menu */

	q.x = menu->window->pixmap->width;
	q.y = 0;
	q.width = menu->scr->tscreen->width - q.x;
	q.height = menu->scr->tscreen->height;

	top_item->sub_menu = pbt_menu_create(top_item, "system", menu->scr,
		menu, &q, &menu->layout);

	if (!top_item->sub_menu)
		goto fail_sub_menu_create;

	sub_item = pbt_item_create(top_item->sub_menu, "Preferences", 0,
		PB_ARTWORK_PATH "/configure.png", "Preferences",
		"Edit petitboot preferences");

	if (!sub_item)
		goto fail_sub_item_0;

	sub_item->on_execute = edit_preferences_cb;
	sub_item->data = client;
	pbt_menu_set_selected(top_item->sub_menu, sub_item);

	sub_item = pbt_item_create(top_item->sub_menu, "Exit to Shell", 1,
		PB_ARTWORK_PATH "/utilities-terminal.png", "Exit to Shell",
		"Exit to a system shell prompt");

	if (!sub_item)
		goto fail_sub_item_1;

	sub_item->on_execute = exit_to_shell_cb;
	sub_item->data = client;

	top_item->sub_menu->n_items = 2;

	/* Set shell item as default */

	pbt_menu_set_selected(top_item->sub_menu, sub_item);

	return top_item;

fail_sub_item_1:
fail_sub_item_0:
fail_sub_menu_create:
// FIXME: todo
fail_top_item_create:
// FIXME: need cleanup
	assert(0);
	return NULL;
}

static struct pbt_menu *menu_create(struct pbt_client *client)
{
	static struct pbt_menu_layout layout = {
		.item_height = 64,
		.item_space = 10,
		.text_space = 5,
		.title = {.font_size = 30, .color = 0xff000000,},
		.text = {.font_size = 18, .color = 0xff800000,},
	};

	struct pbt_menu *device_menu;
	struct pbt_item *system_item;
	struct pbt_quad q;
	twin_pixmap_t *icon;
	const struct pbt_border *border;

	assert(client->frame.scr);

	icon = pbt_icon_load(NULL);

	if (!icon)
		return NULL;

	assert((unsigned int)icon->height == layout.item_height);

	/* Create main (device) menu */

	border = &pbt_right_border;

	q.x = 0;
	q.y = 0;
	q.width = icon->width + 2 * layout.item_space + border->left
		+ border->right;
	q.height = client->frame.scr->tscreen->height;

	device_menu = pbt_menu_create(client, "device", client->frame.scr, NULL,
		&q, &layout);

	if (!device_menu)
		goto fail_menu;

	//FIXME: move to accessors
	device_menu->background_color = 0x80000000;
	device_menu->border = *border;

	/* Setup system item */

	system_item = setup_system_item(device_menu, client);

	if (!system_item)
		goto fail_system_item;

	device_menu->n_items++;

	/* Set system item as default */

	pbt_menu_set_selected(device_menu, system_item);
	pbt_menu_set_focus(device_menu, 1);
	pbt_menu_show(device_menu, 1);

	pbt_menu_redraw(device_menu);

	return device_menu;

fail_system_item:
	// FIXME: need cleanup
fail_menu:
	assert(0);
	return NULL;
}

static int run(struct pbt_client *client)
{
	while (1) {
		int result = waiter_poll(client->waitset);

		if (result < 0) {
			pb_log("%s: poll: %s\n", __func__, strerror(errno));
			break;
		}

		if (client->signal_data.abort)
			break;

		while (client->signal_data.resize) {
			client->signal_data.resize = 0;
			pbt_client_resize(client);
		}
	}

	return 0;
}

static struct pb_signal_data *_signal_data;

static void set_signal_data(struct pb_signal_data *sd)
{
	_signal_data = sd;
}

static struct pb_signal_data *get_signal_data(void)
{
	return _signal_data;
}

static void sig_handler(int signum)
{
	DBGS("%d\n", signum);

	struct pb_signal_data *sd = get_signal_data();

	if (!sd)
		return;

	switch (signum) {
	case SIGWINCH:
		sd->resize = 1;
		break;
	default:
		assert(0 && "unknown sig");
		/* fall through */
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		sd->abort = 1;
		break;
	}
}

/**
 * main - twin bootloader main routine.
 */

int main(int argc, char *argv[])
{
	static struct sigaction sa;
	static struct pbt_opts opts;
	int result;
	int ui_result;
	struct pbt_client *client;
	FILE *log;

	result = pbt_opts_parse(&opts, argc, argv);

	if (result) {
		pbt_print_usage();
		return EXIT_FAILURE;
	}

	if (opts.show_help == pbt_opt_yes) {
		pbt_print_usage();
		return EXIT_SUCCESS;
	}

	if (opts.show_version == pbt_opt_yes) {
		pbt_print_version();
		return EXIT_SUCCESS;
	}

	log = stderr;
	if (strcmp(opts.log_file, "-")) {
		FILE *log = fopen(opts.log_file, "a");
		if (!log)
			log = stderr;
	}
	pb_log_init(log);

	pb_log("--- petitboot-twin ---\n");

	sa.sa_handler = sig_handler;
	result = sigaction(SIGALRM, &sa, NULL);
	result += sigaction(SIGHUP, &sa, NULL);
	result += sigaction(SIGINT, &sa, NULL);
	result += sigaction(SIGTERM, &sa, NULL);
	result += sigaction(SIGWINCH, &sa, NULL);

	if (result) {
		pb_log("%s sigaction failed.\n", __func__);
		return EXIT_FAILURE;
	}

	client = pbt_client_init(opts.backend, 1024, 640, opts.start_daemon);

	if (!client) {
		ui_result = EXIT_FAILURE;
		goto done;
	}

	set_signal_data(&client->signal_data);

	client->frame.top_menu = menu_create(client);

	if (!client->frame.top_menu) {
		ui_result = EXIT_FAILURE;
		goto done;
	}

	twin_screen_update(client->frame.scr->tscreen);
	ui_result = run(client);

done:
	talloc_free(client);

	pb_log("--- end ---\n");

	return ui_result ? EXIT_FAILURE : EXIT_SUCCESS;
}
