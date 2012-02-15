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

#if !defined(_PBT_SCR_H)
#define _PBT_SCR_H

#include <libtwin/twin.h>
#include <libtwin/twin_jpeg.h>
#include <libtwin/twin_linux_mouse.h>
#include <libtwin/twin_linux_js.h>
#include <libtwin/twin_png.h>

#if defined(HAVE_LIBTWIN_TWIN_X11_H)
# include <libtwin/twin_x11.h>
#endif
#if defined(HAVE_LIBTWIN_TWIN_FBDEV_H)
# include <libtwin/twin_fbdev.h>
#endif

#define DBG(fmt, args...) pb_log("DBG: " fmt, ## args)
#define DBGS(fmt, args...) \
	pb_log("DBG:%s:%d: " fmt, __func__, __LINE__, ## args)

struct pbt_quad {
	twin_coord_t x;
	twin_coord_t y;
	twin_coord_t width;
	twin_coord_t height;
};

/**
 * struct pbt_border - A window border.
 * @left: Pixel count for left side.
 * @fill_color: Border fill color.
 */

struct pbt_border {
    unsigned int left;
    unsigned int right;
    unsigned int top;
    unsigned int bottom;
    twin_argb32_t fill_color;
};

enum {
	pbt_debug_red = 0x00800000,
	pbt_debug_green = 0x00008000,
	pbt_debug_blue = 0x00000080,
};

static const struct pbt_border pbt_thin_border = {
	.right = 2,
	.left = 2,
	.top = 2,
	.bottom = 2,
};

static const struct pbt_border pbt_right_border = {
	.right = 2
};

static const struct pbt_border pbt_red_debug_border = {
	.right = 1,
	.left = 1,
	.top = 1,
	.bottom = 1,
	.fill_color = pbt_debug_red,
};

static const struct pbt_border pbt_green_debug_border = {
	.right = 1,
	.left = 1,
	.top = 1,
	.bottom = 1,
	.fill_color = pbt_debug_green,
};

static const struct pbt_border pbt_blue_debug_border = {
	.right = 1,
	.left = 1,
	.top = 1,
	.bottom = 1,
	.fill_color = pbt_debug_blue,
};

static const struct pbt_border pbt_yellow_debug_border = {
	.right = 1,
	.left = 1,
	.top = 1,
	.bottom = 1,
	.fill_color = pbt_debug_green + pbt_debug_red,
};

void pbt_border_draw(twin_pixmap_t *pixmap, const struct pbt_border *border);

struct pbt_cursor {
	twin_pixmap_t *pixmap;
	int hx;
	int hy;
};

enum pbt_twin_backend {
	pbt_twin_x11 = 1,
	pbt_twin_fbdev,
};

struct pbt_twin_ctx {
	union {
		void *ptr;
#if defined(HAVE_LIBTWIN_TWIN_X11_H)
		twin_x11_t *x11;
#endif
#if defined(HAVE_LIBTWIN_TWIN_FBDEV_H)
		twin_fbdev_t *fbdev;
#endif
	};
	enum pbt_twin_backend backend;
};

struct pbt_scr {
	struct pbt_twin_ctx twin_ctx;
	twin_screen_t *tscreen;
	twin_pixmap_t *cursor;
};

struct pbt_scr *pbt_scr_init(void *talloc_ctx, enum pbt_twin_backend backend,
	unsigned int width, unsigned int height,
	const char *filename_background,
	twin_bool_t (*scr_event_cb)(twin_screen_t *tscreen,
		twin_event_t *event));

static inline struct pbt_scr *pbt_scr_from_tscreen(twin_screen_t *tscreen)
{
	size_t offset = (size_t)&((struct pbt_scr *)0)->tscreen;
	return (struct pbt_scr *)((char *)tscreen - offset);
}

void pbt_image_draw(twin_pixmap_t *dest, twin_pixmap_t *image);

#define pbt_dump_event(_s, _w, _e) _pbt_dump_event(_s, _w, _e, __func__, __LINE__)
void _pbt_dump_event(const char *text, twin_window_t *twindow,
	const twin_event_t *tevent, const char *func,  int line);

twin_pixmap_t *pbt_background_load(twin_screen_t *tscreen,
	const char *filename);
twin_pixmap_t *pbt_icon_load(const char *filename);
const char *pbt_icon_chooser(const char *hint);
int pbt_window_contains(const twin_window_t *window, const twin_event_t *event);
void pbt_window_redraw(twin_window_t *twindow);

#define pbt_dump_pixmap(_p)			\
	DBGS("pixmap(%p): {x,y,w,h} = {%d,%d,%d,%d}\n",	\
		_p,					\
		_p->x,					\
		_p->y,					\
		_p->width,				\
		_p->height)


#endif