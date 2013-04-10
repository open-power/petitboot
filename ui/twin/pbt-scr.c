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

#include "config.h"
#define _GNU_SOURCE
#include <assert.h>

#include <string.h>
#include <linux/input.h>

#include "list/list.h"
#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "ui/common/ui-system.h"
#include "pbt-scr.h"

void _pbt_dump_event(const char *text, twin_window_t *twindow,
	const twin_event_t *tevent, const char *func,  int line)
{
	switch(tevent->kind) {
	case TwinEventButtonDown:
		DBG("%s:%d: %s@%p: TwinEventButtonDown %x\n", func, line, text,
			twindow, tevent->kind);
		return;
	case TwinEventButtonUp:
		DBG("%s:%d: %s@%p: TwinEventButtonUp %x\n", func, line, text,
			twindow, tevent->kind);
		return;
	case TwinEventMotion:
		//DBG("%s:%d:%s@%p: TwinEventMotion %x\n", func, line, text,
		//	twindow, tevent->kind);
		return;
	case TwinEventEnter:
		DBG("%s:%d: %s@%p: TwinEventEnter %x\n", func, line, text,
			twindow, tevent->kind);
		return;
	case TwinEventLeave:
		DBG("%s:%d: %s@%p: TwinEventLeave %x\n", func, line, text,
			twindow, tevent->kind);
		return;
	case TwinEventKeyDown:
	case TwinEventKeyUp:
	{
		const char *kind = (tevent->kind == TwinEventKeyDown)
			? "TwinEventKeyDown" : "TwinEventKeyUp  ";

		switch(tevent->u.key.key) {
		case (twin_keysym_t)XK_Up:
		case (twin_keysym_t)KEY_UP:
			DBG("%s:%d: %s@%p: %s = 'KEY_UP'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_Down:
		case (twin_keysym_t)KEY_DOWN:
			DBG("%s:%d: %s@%p: %s = 'KEY_DOWN'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_Right:
		case (twin_keysym_t)KEY_RIGHT:
			DBG("%s:%d: %s@%p: %s = 'KEY_RIGHT'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_Left:
		case (twin_keysym_t)KEY_LEFT:
			DBG("%s:%d: %s@%p: %s = 'KEY_LEFT'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_Escape:
		case (twin_keysym_t)KEY_ESC:
			DBG("%s:%d: %s@%p: %s = 'KEY_ESC'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_Return:
		case (twin_keysym_t)KEY_ENTER:
			DBG("%s:%d: %s@%p: %s = 'KEY_ENTER'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_Delete:
		case (twin_keysym_t)KEY_DELETE:
			DBG("%s:%d: %s@%p: %s = 'KEY_DELETE'\n", func, line, text,
				twindow, kind);
			return;
		case (twin_keysym_t)XK_BackSpace:
		case (twin_keysym_t)KEY_BACKSPACE:
			DBG("%s:%d: %s@%p: %s = 'KEY_BACKSPACE'\n", func, line, text,
				twindow, kind);
			return;
		default:
		DBG("%s:%d: %s@%p: %s = %d (%xh) = '%c'\n", func, line, text, twindow,
			kind,
			tevent->u.key.key, tevent->u.key.key,
			(char)tevent->u.key.key);
		}
		return;
	}
	default:
		DBG("%s:%d: %s@%p: %x\n", func, line, text, twindow, tevent->kind);
		break;
	}
}

/**
 * pbt_background_load - Load the background pixmap from storage.
 * @filename: File name of a jpg background.
 *
 * Returns the default background if @filename is NULL.  Returns a default
 * pattern if the load of @filename fails.
 */

twin_pixmap_t *pbt_background_load(twin_screen_t *tscreen,
	const char *filename)
{
	static const char *default_background_file =
		PB_ARTWORK_PATH "/background.jpg";
	twin_pixmap_t *raw_background;
	twin_pixmap_t *scaled_background;

	if (!filename)
		filename = default_background_file;

	raw_background = twin_jpeg_to_pixmap(filename, TWIN_ARGB32);

	if (!raw_background) {
		pb_log("%s: loading image '%s' failed\n", __func__, filename);

		/* Fallback to a default pattern */

		return twin_make_pattern();
	}

	if (tscreen->height == raw_background->height &&
		tscreen->width == raw_background->width)
		return raw_background;

	/* Scale as needed. */

	twin_fixed_t sx, sy;
	twin_operand_t srcop;

	scaled_background = twin_pixmap_create(TWIN_ARGB32,
				tscreen->width,
				tscreen->height);
	if (!scaled_background) {
		pb_log("%s: scale '%s' failed\n", __func__, filename);
		twin_pixmap_destroy(raw_background);
		return twin_make_pattern();
	}
	sx = twin_fixed_div(twin_int_to_fixed(raw_background->width),
			twin_int_to_fixed(tscreen->width));
	sy = twin_fixed_div(twin_int_to_fixed(raw_background->height),
			twin_int_to_fixed(tscreen->height));

	twin_matrix_scale(&raw_background->transform, sx, sy);
	srcop.source_kind = TWIN_PIXMAP;
	srcop.u.pixmap = raw_background;
	twin_composite(scaled_background, 0, 0, &srcop, 0, 0,
		NULL, 0, 0, TWIN_SOURCE,
		tscreen->width, tscreen->height);

	twin_pixmap_destroy(raw_background);

	return scaled_background;
}

const char *pbt_icon_chooser(const char *hint)
{
	if (strstr(hint, "net"))
		return PB_ARTWORK_PATH "/network-wired.png";

	return NULL;
}

/**
 * pbt_icon_load - Load an icon pixmap from storage.
 * @filename: File name of a png icon.
 *
 * Returns the default icon if @filename is NULL, or if the load
 * of @filename fails.
 * Caches pixmaps based on a hash of the @filename string.
 */

twin_pixmap_t *pbt_icon_load(const char *filename)
{
	static const char *default_icon_file = PB_ARTWORK_PATH "/tux.png";
	struct cache_entry {
		struct list_item list;
		int hash;
		twin_pixmap_t *icon;
	};
	STATIC_LIST(icon_cache);
	struct cache_entry new;
	struct cache_entry *i;

	if (!filename)
		filename = default_icon_file;

retry:
	new.hash = pb_elf_hash(filename);

	list_for_each_entry(&icon_cache, i, list) {
		if (i->hash == new.hash) {
			DBGS("found %p\n", i->icon);
			return i->icon;
		}
	}

	new.icon = twin_png_to_pixmap(filename, TWIN_ARGB32);

	if (!new.icon) {
		pb_log("%s: loading image '%s' failed\n", __func__, filename);

		if (filename == default_icon_file)
			return NULL;

		filename = default_icon_file;
		goto retry;
	}

	DBGS("new %p\n", new.icon);

	i = talloc(NULL, struct cache_entry);
	*i = new;
	list_add(&icon_cache, &i->list);

	pbt_dump_pixmap(new.icon);

	return new.icon;
}

/**
 * pbt_border_draw - Draw a border on a pixmap.
 * @pixmap: The image to operate on.
 * @border: The border to draw.
 */

void pbt_border_draw(twin_pixmap_t *pixmap, const struct pbt_border *border)
{
	twin_path_t *path = twin_path_create();
	twin_argb32_t fill = border->fill_color ? border->fill_color
		: 0xff000000;  /* default to black */

	assert(path);

	//pbt_dump_pixmap(pixmap);

	if (border->left) {
		twin_path_rectangle(path, 0, 0,
			twin_int_to_fixed(border->left),
			twin_int_to_fixed(pixmap->height));
	}

	if (border->right) {
		twin_path_rectangle(path,
			twin_int_to_fixed(pixmap->width - border->right),
			0,
			twin_int_to_fixed(pixmap->width),
			twin_int_to_fixed(pixmap->height));
	}

	if (border->top) {
		twin_path_rectangle(path, 0, 0,
			twin_int_to_fixed(pixmap->width),
			twin_int_to_fixed(border->top));
	}

	if (border->bottom) {
		twin_path_rectangle(path, 0,
			twin_int_to_fixed(pixmap->height - border->bottom),
			twin_int_to_fixed(pixmap->width),
			twin_int_to_fixed(border->bottom));
	}

	twin_paint_path(pixmap, fill, path);
	twin_path_empty(path);
}

int pbt_window_contains(const twin_window_t *window, const twin_event_t *event)
{
	pbt_dump_pixmap(window->pixmap);

	if (event->u.pointer.x < window->pixmap->x) {
		DBGS("%p: {%d,%d} left miss\n", window, event->u.pointer.x, event->u.pointer.y);
		return 0;
	}
	if (event->u.pointer.x >= window->pixmap->x + window->pixmap->width) {
		DBGS("%p: {%d,%d} right miss\n", window, event->u.pointer.x, event->u.pointer.y);
		return 0;
	}
	if (event->u.pointer.y < window->pixmap->y) {
		DBGS("%p: {%d,%d} high miss\n", window, event->u.pointer.x, event->u.pointer.y);
		return 0;
	}
	if (event->u.pointer.y >= window->pixmap->y + window->pixmap->height){
		DBGS("%p: {%d,%d} low miss\n", window, event->u.pointer.x, event->u.pointer.y);
		return 0;
	}

	DBGS("%p: {%d,%d} hit\n", window, event->u.pointer.x, event->u.pointer.y);
	return 1;
}


static __attribute__((unused)) void pbt_image_copy(twin_pixmap_t *dest, twin_pixmap_t *src)
{
	twin_operand_t op;

	assert(dest->height >= src->height);

	op.source_kind = TWIN_PIXMAP;
	op.u.pixmap = src;

	twin_composite(dest, 0, 0, &op, 0, 0, NULL,
		0, 0, TWIN_SOURCE, src->width, src->height);
}

void pbt_image_draw(twin_pixmap_t *dest, twin_pixmap_t *image)
{
	twin_operand_t src;
	int offset;

	assert(dest->height >= image->height);

	src.source_kind = TWIN_PIXMAP;
	src.u.pixmap = image;

	/* Center the image in the window. */

	offset = (dest->height - image->height) / 2;

	twin_composite(dest, offset, offset, &src, 0, 0, NULL,
		0, 0, TWIN_SOURCE, image->width, image->height);
}

static int pbt_twin_waiter_cb(struct pbt_twin_ctx *twin_ctx)
{
#if defined(HAVE_LIBTWIN_TWIN_X11_H)
	if (twin_ctx->backend == pbt_twin_x11)
		twin_x11_process_events(twin_ctx->x11);
#endif
#if defined(HAVE_LIBTWIN_TWIN_FBDEV_H)
	if (twin_ctx->backend == pbt_twin_fbdev)
		twin_fbdev_process_events(twin_ctx->fbdev);
#endif
	return 0;
};

static void pbt_scr_destructor(struct pbt_scr *scr)
{
	pb_log("%s\n", __func__);

	twin_x11_destroy(scr->twin_ctx.x11);
	// FIXME: need cursor cleanup???
	memset(scr, 0, sizeof(*scr));
}

struct pbt_scr *pbt_scr_init(void *talloc_ctx,
	struct waitset *waitset,
	enum pbt_twin_backend backend,
	unsigned int width, unsigned int height,
	const char *filename_background,
	twin_bool_t (*scr_event_cb)(twin_screen_t *tscreen,
		twin_event_t *event))
{
	struct pbt_scr *scr = talloc_zero(talloc_ctx, struct pbt_scr);
	int waiter_fd = -1;

	assert(backend && backend < 3);

	if (!scr) {
		pb_log("%s: alloc pbt_scr failed.\n", __func__);
		goto fail_alloc;
	}

	talloc_set_destructor(scr, (void *)pbt_scr_destructor);

	twin_feature_init(); // FIXME: need it???

	scr->twin_ctx.backend = backend;

	if (backend == pbt_twin_x11) {
		pb_log("%s: using twin x11 backend.\n", __func__);
		assert(width > 100);
		assert(height > 100);

#if !defined(HAVE_LIBTWIN_TWIN_X11_H)
		assert(0);
#else
		scr->twin_ctx.x11 = twin_x11_create_ext(XOpenDisplay(0), width,
			height, 0);

		if (!scr->twin_ctx.x11) {
			pb_log("%s: twin_x11_create_ext failed.\n", __func__);
			perror("failed to create twin x11 context\n");
			goto fail_ctx_create;
		}

		pb_log("%s: x11: %p\n", __func__, scr->twin_ctx.x11);

		assert(scr->twin_ctx.x11->screen);
		scr->tscreen = scr->twin_ctx.x11->screen;
		waiter_fd = ConnectionNumber(scr->twin_ctx.x11->dpy);
#endif
	} else if (backend == pbt_twin_fbdev) {
		pb_log("%s: using twin fbdev backend.\n", __func__);
#if !defined(HAVE_LIBTWIN_TWIN_FBDEV_H)
		assert(0);
#else
		scr->twin_ctx.fbdev = twin_fbdev_create_ext(-1, SIGUSR1, 0);

		if (!scr->twin_ctx.fbdev) {
			pb_log("%s: twin_fbdev_create_ext failed.\n", __func__);
			perror("failed to create twin fbdev context\n");
			goto fail_ctx_create;
		}

		assert(scr->twin_ctx.fbdev->screen);
		scr->tscreen = scr->twin_ctx.fbdev->screen;
		waiter_fd = scr->twin_ctx.fbdev->vt_fd;

		twin_fbdev_activate(scr->twin_ctx.fbdev);
#endif
	}

	scr->tscreen->event_filter = scr_event_cb;

	twin_screen_set_background(scr->tscreen,
		pbt_background_load(scr->tscreen, filename_background));

	assert(waiter_fd != -1);

	waiter_register(waitset, waiter_fd, WAIT_IN, (void *)pbt_twin_waiter_cb,
		&scr->twin_ctx);

	return scr;

fail_ctx_create:
fail_alloc:
	return NULL;
}

void pbt_window_redraw(twin_window_t *twindow)
{
	twin_window_damage(twindow, 0, 0, twindow->pixmap->width,
		twindow->pixmap->height);
	//twin_window_queue_paint(twindow);
	twin_window_draw(twindow);
}
