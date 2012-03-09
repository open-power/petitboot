/*
 * Petitboot twin bootloader for the PS3 game console
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
#include <linux/input.h>
#include <sys/time.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "ui/common/timer.h"
#include "ui/common/ps3.h"

#include "pbt-client.h"
#include "pbt-main.h"

/* control to keyboard mappings for the sixaxis controller */

static const uint8_t ps3_sixaxis_map[] = {
	0,		/*   0  Select		*/
	0,		/*   1  L3              */
	0,		/*   2  R3              */
	0,		/*   3  Start           */
	KEY_UP,		/*   4  Dpad Up         */
	KEY_RIGHT,	/*   5  Dpad Right      */
	KEY_DOWN,	/*   6  Dpad Down       */
	KEY_LEFT,	/*   7  Dpad Left       */
	0,		/*   8  L2              */
	0,		/*   9  R2              */
	0,		/*  10  L1              */
	0,		/*  11  R1              */
	0,		/*  12  Triangle        */
	KEY_ENTER,	/*  13  Circle          */
	0,		/*  14  Cross           */
	KEY_DELETE,	/*  15  Square          */
	0,		/*  16  PS Button       */
	0,		/*  17  nothing      */
	0,		/*  18  nothing      */
};

static struct pbt_item *ps3_setup_system_item(struct pbt_menu *menu,
	const struct pbt_menu_layout *layout)
{
#if 0
	struct _twin_rect r;
	struct pbt_item *main_item;
	struct pbt_item *sub_item;
	twin_pixmap_t *icon;

	icon = pbt_icon_load(NULL);

	/* Main item */

	pbt_view_get_item_rect(menu->main, layout, icon, 0, &r);
	main_item = pbt_item_create(menu->main, menu->main, &r);

	if (!main_item)
		goto fail;

	main_item->image = icon;

	list_add_tail(menu->main->items_list, &main_item->list);

	/* Sub items */

	main_item->sub_items_list = talloc(main_item, struct list);
	list_init(main_item->sub_items_list);

	pbt_view_get_item_rect(menu->sub, layout, icon, 0, &r);
	sub_item = pbt_item_create(main_item, menu->sub, &r);

	if (!sub_item)
		goto fail;

	sub_item->image = pbt_item_create_text_image(layout, pbt_rect_width(&r),
		pbt_rect_height(&r), icon, "Boot GameOS",
		"Reboot the PS3 into the GameOS");

	list_add_tail(main_item->sub_items_list, &sub_item->list);

	pbt_view_get_item_rect(menu->sub, layout, icon, 1, &r);
	sub_item = pbt_item_create(main_item, menu->sub, &r);

	if (!sub_item)
		goto fail;

	sub_item->image = pbt_item_create_text_image(layout, pbt_rect_width(&r),
		pbt_rect_height(&r), icon, "Set Video Mode",
		"Set the current video mode");

	list_add_tail(main_item->sub_items_list, &sub_item->list);

	pbt_view_get_item_rect(menu->sub, layout, icon, 2, &r);
	sub_item = pbt_item_create(main_item, menu->sub, &r);

	if (!sub_item)
		goto fail;

	sub_item->image = pbt_item_create_text_image(layout, pbt_rect_width(&r),
		pbt_rect_height(&r), icon, "Exit to Shell",
		"Exit to a system shell prompt");

	list_add_tail(main_item->sub_items_list, &sub_item->list);

	menu->sub->selected = sub_item;
	menu->sub->items_list = main_item->sub_items_list;

	return main_item;
fail:
#endif
	// FIXME: need cleanup
	assert(0);
	return NULL;
}

static int ps3_setup_test_item(struct pbt_menu *menu,
	const struct pbt_menu_layout *layout)
{
#if 0
	struct _twin_rect r;
	struct pbt_item *main_item;
	struct pbt_item *sub_item;
	twin_pixmap_t *icon;

	icon = pbt_icon_load(PB_ARTWORK_PATH "/drive-harddisk.png");

	/* Main item */

	pbt_view_get_item_rect(menu->main, layout, icon, 1, &r);
	main_item = pbt_item_create(menu->main, menu->main, &r);

	if (!main_item)
		goto fail;

	main_item->image = icon;

	list_add_tail(menu->main->items_list, &main_item->list);

	/* Sub items */

	main_item->sub_items_list = talloc(main_item, struct list);
	list_init(main_item->sub_items_list);

	pbt_view_get_item_rect(menu->sub, layout, icon, 0, &r);
	sub_item = pbt_item_create(main_item, menu->sub, &r);

	if (!sub_item)
		goto fail;

	sub_item->active = 0;
	sub_item->image = pbt_item_create_text_image(layout, pbt_rect_width(&r),
		pbt_rect_height(&r), icon, "test 1",
		"text for test 1");

	list_add_tail(main_item->sub_items_list, &sub_item->list);

	pbt_view_get_item_rect(menu->sub, layout, icon, 1, &r);
	sub_item = pbt_item_create(main_item, menu->sub, &r);

	if (!sub_item)
		goto fail;

	sub_item->active = 0;
	sub_item->image = pbt_item_create_text_image(layout, pbt_rect_width(&r),
		pbt_rect_height(&r), icon, "test 2",
		"text for test 2");

	list_add_tail(main_item->sub_items_list, &sub_item->list);

	menu->sub->selected = sub_item;
	menu->sub->items_list = main_item->sub_items_list;

	return 0;

fail:
#endif
	// FIXME: need cleanup
	assert(0);
	return -1;
}

static struct pbt_menu *ps3_menu_create(void *ctx, struct pbt_scr *scr)
{
#if 0
	struct pbt_menu *menu;
	struct _twin_rect r;
	twin_pixmap_t *icon;
	const struct pbt_border *border;
	static const struct pbt_menu_layout layout = {
		.item_space = 10,
		.icon_space = 6,
		.title_font_size = 30,
		.title_font_color = 0xff000000,
		.text_font_size = 18,
		.text_font_color = 0xff400000,
	};

	assert(scr && scr->sig == pbt_scr_sig);

	menu = talloc_zero(ctx, struct pbt_menu);

	if (!menu)
		return NULL;

	menu->sig = pbt_menu_sig;
	menu->scr = scr;

	icon = pbt_icon_load(NULL);

	if (!icon)
		return NULL;

	/* Setup main view */

	border = &pbt_right_border;
	r.left = 0;
	r.top = 0;
	r.right = icon->width + 2 * layout.item_space + 2 * layout.icon_space
		+ border->left + border->right;
	r.bottom = menu->scr->tscreen->height;

	menu->main = pbt_view_create(menu, &r);

	if (!menu->main)
		goto fail_main;

	menu->main->background_color = 0x80000000;
	menu->main->border = *border;
	menu->main->items_list = talloc(menu->main, struct list);
	list_init(menu->main->items_list);

	/* Setup sub view */

	r.left = r.right;
	r.top = 0;
	r.right = menu->scr->tscreen->width;
	r.bottom = menu->scr->tscreen->height;

	menu->sub = pbt_view_create(menu, &r);

	if (!menu->sub)
		goto fail_sub;

	menu->sub->background_color = 0x40000000;

	/* Setup system item */

	menu->main->selected = ps3_setup_system_item(menu, &layout);

	if (!menu->main->selected)
		goto fail_main_item;

	//ps3_setup_test_item(menu, &layout);

	return menu;

fail_main_item:
	// FIXME: need cleanup
fail_sub:
	// FIXME: need cleanup
fail_main:
	talloc_free(menu);
#endif
	assert(0);
	return NULL;
}

/**
 * struct ps3_gui - Main gui program instance.
 */


struct ps3_gui {
	struct ui_timer timer;
	struct ps3_flash_values values;
	int dirty_values;

	struct pbt_scr scr;
	struct pbt_frame frame;
	struct pbt_menu *menu;
};

static struct ps3_gui ps3;

static struct pbt_scr *ps3_scr_from_tscreen(twin_screen_t *tscreen)
{
	assert(ps3.scr.tscreen == tscreen);
	return &ps3.scr;
}

static twin_bool_t ps3_scr_event_cb(twin_screen_t *tscreen, twin_event_t *event)
{
	struct pbt_scr *scr = ps3_scr_from_tscreen(tscreen);

	pbt_dump_event("scr", NULL, event);

	switch(event->kind) {
	case TwinEventJoyButton:
		/* convert joystick events to key events */
		// FIXME: need to test
		if (event->u.js.control < sizeof(ps3_sixaxis_map)) {
			event->u.key.key = ps3_sixaxis_map[event->u.js.control];
			event->kind = event->u.js.value ? TwinEventKeyUp
				: TwinEventKeyDown;
		}
		break;
	default:
		break;
	}
	return TWIN_FALSE;
}

static void sig_handler(int signum)
{
	DBGS("%d\n", signum);

	switch (signum) {
	case SIGALRM:
		ui_timer_sigalrm(&ps3.timer);
		break;
	case SIGWINCH:
//		if (ps3.gui)
//			gui_resize(ps3.gui);
		break;
	default:
		assert(0 && "unknown sig");
		/* fall through */
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		exit(EXIT_FAILURE);
//		if (ps3.gui)
//			gui_abort(ps3.gui);
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
	int ui_result = -1;
	unsigned int mode;
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

	log = fopen(opts.log_file, "a");
	assert(log);
	pb_log_set_stream(log);

#if defined(DEBUG)
	pb_log_always_flush(1);
#endif

	pb_log("--- pb-twin ---\n");

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

	ps3.values = ps3_flash_defaults;

	if (opts.reset_defaults != pbt_opt_yes)
		ps3.dirty_values = ps3_flash_get_values(&ps3.values);

#if 0
	twin_feature_init(); // need it???

	/* Setup screen. */

#if defined(HAVE_LIBTWIN_TWIN_X11_H)
# if defined(USE_X11)
	if (1) {
# else
	if (0) {
# endif
		//ps3.scr.x11 = twin_x11_create(XOpenDisplay(0), 1024, 768);
		ps3.scr.x11 = twin_x11_create(XOpenDisplay(0), 512, 384);

		if (!ps3.scr.x11) {
			perror("failed to create x11 screen !\n");
			return EXIT_FAILURE;
		}

		ps3.scr.tscreen = ps3.scr.x11->screen;
	} else {
#else
	if (1) {
#endif
		result = ps3_get_video_mode(&mode);

		/* Current becomes default if ps3_flash_get_values() failed. */

		if (ps3.dirty_values && !result)
			ps3.values.video_mode = mode;

		/* Set mode if not at default. */

		if (!result && (ps3.values.video_mode != (uint16_t)mode))
			ps3_set_video_mode(ps3.values.video_mode);

		ps3.scr.fbdev = twin_fbdev_create(-1, SIGUSR1);

		if (!ps3.scr.fbdev) {
			perror("failed to create fbdev screen !\n");
			return EXIT_FAILURE;
		}

		ps3.scr.tscreen = ps3.scr.fbdev->screen;
	}

	ps3.scr.tscreen->event_filter = pbt_scr_event;

	twin_screen_set_background(ps3.scr.tscreen,
		pbt_background_load(ps3.scr.tscreen, NULL));

	/* setup menu */

	ps3.menu = ps3_menu_create(NULL, &ps3.scr);

	if (!ps3.menu)
		return EXIT_FAILURE;

	/* Console switch */

	if (ps3.scr.fbdev)
		twin_fbdev_activate(ps3.scr.fbdev);

	/* run twin */

	// need to hookup pb waiters to twin...
	twin_dispatch();

#endif

	pb_log("--- end ---\n");

	return ui_result ? EXIT_FAILURE : EXIT_SUCCESS;
}
