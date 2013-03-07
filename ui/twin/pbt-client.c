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

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>

#include <pb-protocol/pb-protocol.h>

#include "pbt-client.h"

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "ui/common/discover-client.h"

static struct pb_opt_data *pbt_opt_data_from_item(struct pbt_item *item)
{
	return item->data;
}

void pbt_client_resize(struct pbt_client *client)
{
	(void)client; // TODO
}

void pbt_frame_status_printf(struct pbt_frame *frame, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	// TODO
	(void)frame;
	va_end(ap);
}

static int pbt_client_boot(struct pbt_item *item)
{
	int result;
	struct pb_opt_data *opt_data = pbt_opt_data_from_item(item);

	pb_log("%s: %s\n", __func__, pbt_item_name(item));

	pbt_frame_status_printf(&item->pbt_client->frame, "Booting %s...",
		pbt_item_name(item));

	result = discover_client_boot(item->pbt_client->discover_client,
			opt_data->dev, opt_data->opt, opt_data->bd);

	if (result) {
		pb_log("%s: failed: %s\n", __func__, opt_data->bd->image);
		pbt_frame_status_printf(&item->pbt_client->frame,
				"Failed: kexec %s", opt_data->bd->image);
	}

	return 0;
}

static int pbt_client_on_edit(struct pbt_item *item)
{
	DBGS("*** %s ***\n", pbt_item_name(item));
	return 0;
}

static int pbt_device_add(struct device *dev, struct pbt_client *client)
{
	struct pbt_frame *const frame = &client->frame;
	struct pbt_item *device_item;
	struct boot_option *opt;
	struct pbt_quad q;
	const char *icon_file;
	struct pbt_item *selected_item = NULL;

	pb_log("%s: %p %s: n_options %d\n", __func__, dev, dev->id,
		dev->n_options);

	pb_protocol_dump_device(dev, "", pb_log_get_stream());

	/* device_item */

	// FIXME: check for existing dev->id

	icon_file = dev->icon_file ? dev->icon_file : pbt_icon_chooser(dev->id);

	device_item = pbt_item_create_reduced(frame->top_menu, dev->id,
		frame->top_menu->n_items, icon_file);

	if (!device_item)
		goto fail_device_item_create;

	device_item->pb_device = dev;
	frame->top_menu->n_items++;

	/* sub_menu */

	q.x = frame->top_menu->window->pixmap->width;
	q.y = 0;
	q.width = frame->top_menu->scr->tscreen->width - q.x;
	q.height = frame->top_menu->scr->tscreen->height;

	device_item->sub_menu = pbt_menu_create(device_item, dev->id,
		frame->top_menu->scr, frame->top_menu, &q,
		&frame->top_menu->layout);
	if (!device_item->sub_menu)
		goto fail_sub_menu_create;

	list_for_each_entry(&dev->boot_options, opt, list) {
		struct pbt_item *i;
		struct pb_opt_data *opt_data;

		i = pbt_item_create(device_item->sub_menu,
			opt->id, device_item->sub_menu->n_items++,
			opt->icon_file, opt->name, opt->description);

		if (!i) {
			assert(0);
			break;
		}

		i->pb_opt = opt;
		i->pbt_client = client;
		i->on_execute = pbt_client_boot;
		i->on_edit = pbt_client_on_edit;

		i->data = opt_data = talloc(i, struct pb_opt_data);
		opt_data->name = opt->name;
		opt_data->bd = talloc(i, struct pb_boot_data);
		opt_data->bd->image = talloc_strdup(opt_data->bd,
			opt->boot_image_file);
		opt_data->bd->initrd = talloc_strdup(opt_data->bd,
			opt->initrd_file);
		opt_data->bd->args = talloc_strdup(opt_data->bd,
			opt->boot_args);
		opt_data->dev = dev;
		opt_data->opt = opt;
		opt_data->opt_hash = pb_opt_hash(dev, opt);

		/* Select the first item as default. */

		if (!selected_item)
			selected_item = i;

		/* If this is the default_item select it and start timer. */

		if (opt_data->opt_hash
			== device_item->sub_menu->default_item_hash) {
			selected_item = i;
			ui_timer_kick(&client->signal_data.timer);
		}
	}

	pbt_menu_set_selected(device_item->sub_menu, selected_item);

	pbt_menu_show(frame->top_menu, 1);
	twin_screen_update(client->frame.scr->tscreen);

	return 0;

fail_sub_menu_create:
fail_device_item_create:
	assert(0);
	return -1;
}

static void pbt_device_remove(struct device *dev, struct pbt_client *client)
{
	struct pbt_frame *const frame = &client->frame;
	struct list *i_list = frame->top_menu->item_list;
	twin_window_t *last_window = NULL;
	struct pbt_item *removed_item;
	struct pbt_item *prev_item;
	struct pbt_item *next_item;
	struct pbt_item *i;

	pb_log("%s: %p %s: n_options %d\n", __func__, dev, dev->id,
		dev->n_options);

	pb_protocol_dump_device(dev, "", pb_log_get_stream());

	removed_item = NULL;
	list_for_each_entry(i_list, i, list) {
		if (i->pb_device == dev) {
			removed_item = i;
			break;
		}
	}

	if (!removed_item) {
		pb_log("%s: %p %s: unknown device\n", __func__, dev, dev->id);
		assert(0 && "unknown device");
		return;
	}

	prev_item = list_prev_entry(i_list, removed_item, list);
	next_item = list_next_entry(i_list, removed_item, list);

	if (removed_item == frame->top_menu->selected) {
		if (prev_item)
			pbt_menu_set_selected(frame->top_menu, prev_item);
		else if (next_item)
			pbt_menu_set_selected(frame->top_menu, next_item);
		else
			assert(0 && "empty list");
	}

	if (next_item) {

		/* Shift items up. */

		i = next_item;
		list_for_each_entry_continue(i_list, i, list) {
			last_window = i->window;
			i->window = list_prev_entry(i_list, i, list)->window;
			twin_window_set_name(i->window, last_window->name);
			i->window->client_data = last_window->client_data;
		}
	}

	twin_window_hide(last_window);
	twin_window_destroy(last_window);

	list_remove(&removed_item->list);
	removed_item->window = NULL;
	talloc_free(removed_item);
	frame->top_menu->n_items--;

	pbt_menu_show(frame->top_menu, 1);
	twin_screen_update(client->frame.scr->tscreen);
}

static struct discover_client_ops pbt_client_ops = {
	.device_add = (void *)pbt_device_add,
	.device_remove = (void *)pbt_device_remove,
};

static void pbt_client_destructor(struct pbt_client *client)
{
	pb_log("%s\n", __func__);

	// move to screen twin_x11_destroy(twin_ctx);
	talloc_free(client->discover_client);
	memset(client, 0, sizeof(*client));
}

struct pbt_client *pbt_client_init(enum pbt_twin_backend backend,
	unsigned int width, unsigned int height, int start_deamon)
{
	struct pbt_client *pbt_client;
	unsigned int i;

	pbt_client = talloc_zero(NULL, struct pbt_client);

	if (!pbt_client) {
		pb_log("%s: alloc pbt_client failed.\n", __func__);
		fprintf(stderr, "%s: alloc pbt_client failed.\n", __func__);
		goto fail_alloc;
	}

	talloc_set_destructor(pbt_client, (void *)pbt_client_destructor);

	pbt_client->waitset = waitset_create(pbt_client);

	pbt_client->sig = "pbt_client";
	pbt_client->frame.scr = pbt_scr_init(pbt_client, pbt_client->waitset,
			backend, width, height, NULL, NULL);


	if (!pbt_client->frame.scr)
		goto fail_scr_init;

	/* Loop here for scripts that just started the server. */

retry_start:
	for (i = start_deamon ? 2 : 10; i; i--) {
		pbt_client->discover_client
			= discover_client_init(pbt_client->waitset,
					&pbt_client_ops, pbt_client);
		if (pbt_client->discover_client || !i)
			break;
		pb_log("%s: waiting for server %d\n", __func__, i);
		sleep(1);
	}

	if (!pbt_client->discover_client && start_deamon) {
		int result;

		start_deamon = 0;

		result = pb_start_daemon();

		if (!result)
			goto retry_start;

		pb_log("%s: discover_client_init failed.\n", __func__);
		fprintf(stderr, "%s: error: discover_client_init failed.\n",
			__func__);
		fprintf(stderr, "could not start pb-discover, the petitboot "
			"daemon.\n");
		goto fail_client_init;
	}

	if (!pbt_client->discover_client) {
		pb_log("%s: discover_client_init failed.\n", __func__);
		fprintf(stderr, "%s: error: discover_client_init failed.\n",
			__func__);
		fprintf(stderr, "check that pb-discover, "
			"the petitboot daemon is running.\n");
		goto fail_client_init;
	}

	return pbt_client;

fail_client_init:
	talloc_free(pbt_client);
fail_scr_init:
fail_alloc:
	return NULL;
}
