#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <syscall.h>
#include <assert.h>

#include <linux/input.h>

#undef _USE_X11

#include <libtwin/twin.h>
#include <libtwin/twin_linux_mouse.h>
#include <libtwin/twin_png.h>
#include <libtwin/twin_jpeg.h>

#include "petitboot.h"
#include "petitboot-paths.h"

#ifdef _USE_X11
#include <libtwin/twin_x11.h>
static twin_x11_t *pboot_x11;
#else
#include <libtwin/twin_fbdev.h>
static twin_fbdev_t *pboot_fbdev;
#endif

static twin_screen_t *pboot_screen;

#define PBOOT_LEFT_PANE_SIZE		200
#define PBOOT_LEFT_PANE_COLOR		0x80000000
#define PBOOT_LEFT_LINE_COLOR		0xff000000

#define PBOOT_LEFT_FOCUS_WIDTH		80
#define PBOOT_LEFT_FOCUS_HEIGHT		80
#define PBOOT_LEFT_FOCUS_XOFF		60
#define PBOOT_LEFT_FOCUS_YOFF		60
#define PBOOT_LEFT_FOCUS_XRAD		(6 * TWIN_FIXED_ONE)
#define PBOOT_LEFT_FOCUS_YRAD		(6 * TWIN_FIXED_ONE)

#define PBOOT_RIGHT_FOCUS_XOFF		20
#define PBOOT_RIGHT_FOCUS_YOFF		60
#define PBOOT_RIGHT_FOCUS_HEIGHT	80
#define PBOOT_RIGHT_FOCUS_XRAD		(6 * TWIN_FIXED_ONE)
#define PBOOT_RIGHT_FOCUS_YRAD		(6 * TWIN_FIXED_ONE)

#define PBOOT_LEFT_ICON_WIDTH		64
#define PBOOT_LEFT_ICON_HEIGHT		64
#define PBOOT_LEFT_ICON_XOFF		70
#define PBOOT_LEFT_ICON_YOFF		70
#define PBOOT_LEFT_ICON_STRIDE		100

#define PBOOT_RIGHT_OPTION_LMARGIN	30
#define PBOOT_RIGHT_OPTION_RMARGIN	30
#define PBOOT_RIGHT_OPTION_TMARGIN	70
#define PBOOT_RIGHT_OPTION_HEIGHT	64
#define PBOOT_RIGHT_OPTION_STRIDE	100
#define PBOOT_RIGHT_TITLE_TEXT_SIZE	(30 * TWIN_FIXED_ONE)
#define PBOOT_RIGHT_SUBTITLE_TEXT_SIZE	(18 * TWIN_FIXED_ONE)
#define PBOOT_RIGHT_TITLE_XOFFSET	80
#define PBOOT_RIGHT_TITLE_YOFFSET	30
#define PBOOT_RIGHT_SUBTITLE_XOFFSET	200
#define PBOOT_RIGHT_SUBTITLE_YOFFSET	50
#define PBOOT_RIGHT_BADGE_XOFFSET	2
#define PBOOT_RIGHT_BADGE_YOFFSET	0


#define PBOOT_RIGHT_TITLE_COLOR		0xff000000
#define PBOOT_RIGHT_SUBTITLE_COLOR	0xff400000

#define PBOOT_FOCUS_COLOR		0x10404040


typedef struct _pboot_option pboot_option_t;
typedef struct _pboot_device pboot_device_t;

struct _pboot_option
{
	char		*title;
	char		*subtitle;
	twin_pixmap_t	*badge;
	twin_pixmap_t	*cache;
	twin_rect_t	box;
	void		*data;
};

struct _pboot_device
{
	char			*id;
	twin_pixmap_t		*badge;
	twin_rect_t		box;
	int			option_count;
	pboot_option_t		options[PBOOT_MAX_OPTION];
};

static twin_pixmap_t	*pboot_cursor;
static int		pboot_cursor_hx;
static int		pboot_cursor_hy;

static pboot_device_t	*pboot_devices[PBOOT_MAX_DEV];
static int		pboot_dev_count;
static int		pboot_dev_sel = -1;
static int		pboot_focus_lpane = 1;

typedef struct _pboot_lpane {
	twin_window_t	*window;
	twin_rect_t	focus_box;
	int		focus_start;
	int		focus_target;
	int		focus_curindex;
	int		mouse_target;
} pboot_lpane_t;

typedef struct _pboot_rpane {
	twin_window_t	*window;
	twin_rect_t	focus_box;
	int		focus_start;
	int		focus_target;
	int		focus_curindex;
	int		mouse_target;
} pboot_rpane_t;

static pboot_lpane_t	*pboot_lpane;
static pboot_rpane_t	*pboot_rpane;

/* XXX move to twin */
static inline twin_bool_t twin_rect_intersect(twin_rect_t r1,
					      twin_rect_t r2)
{
	return !(r1.left > r2.right ||
		 r1.right < r2.left ||
		 r1.top > r2.bottom ||
		 r1.bottom < r2.top);
}

static void pboot_draw_option_cache(pboot_device_t *dev, pboot_option_t *opt,
				    int index)
{
	twin_pixmap_t	*px;
	twin_path_t	*path;
	twin_fixed_t	tx, ty;

	/* Create pixmap */
	px = twin_pixmap_create(TWIN_ARGB32, opt->box.right - opt->box.left,
				 opt->box.bottom - opt->box.top);
	assert(px);
	opt->cache = px;

	/* Fill background */
	twin_fill(px, 0x00000000, TWIN_SOURCE, 0, 0, px->width, px->height);

	/* Allocate a path for drawing */
	path = twin_path_create();
	assert(path);

#if 0
	/* TEST - Bounding rectangle */
	twin_path_rectangle(path, 0, 0,
			    twin_int_to_fixed(px->width),
			    twin_int_to_fixed(px->height));
	twin_paint_path(px, PBOOT_RIGHT_TITLE_COLOR, path);
	twin_path_empty(path);
	twin_fill(px, 0x00000000, TWIN_SOURCE, 2, 2,
		  px->width - 3, px->height - 3);
#endif

	/* Draw texts */
	twin_path_set_font_size(path, PBOOT_RIGHT_TITLE_TEXT_SIZE);
	twin_path_set_font_style(path, TWIN_TEXT_UNHINTED);
	tx = twin_int_to_fixed(PBOOT_RIGHT_TITLE_XOFFSET);
	ty = twin_int_to_fixed(PBOOT_RIGHT_TITLE_YOFFSET);
	twin_path_move (path, tx, ty);
	twin_path_utf8 (path, opt->title);
	twin_paint_path (px, PBOOT_RIGHT_TITLE_COLOR, path);
	twin_path_empty (path);

	if (opt->subtitle) {
		twin_path_set_font_size(path, PBOOT_RIGHT_SUBTITLE_TEXT_SIZE);
		twin_path_set_font_style(path, TWIN_TEXT_UNHINTED);
		tx = twin_int_to_fixed(PBOOT_RIGHT_SUBTITLE_XOFFSET);
		ty = twin_int_to_fixed(PBOOT_RIGHT_SUBTITLE_YOFFSET);
		twin_path_move (path, tx, ty);
		twin_path_utf8 (path, opt->subtitle);
		twin_paint_path (px, PBOOT_RIGHT_SUBTITLE_COLOR, path);
		twin_path_empty (path);
	}

	if (opt->badge) {
		twin_operand_t	src;

		src.source_kind = TWIN_PIXMAP;
		src.u.pixmap = opt->badge;

		twin_composite(px, PBOOT_RIGHT_BADGE_XOFFSET,
			       PBOOT_RIGHT_BADGE_YOFFSET,
			       &src, 0, 0, NULL, 0, 0, TWIN_OVER,
			       opt->badge->width, opt->badge->height);
	}


	/* Destroy path */
	twin_path_destroy(path);
}

static void pboot_rpane_draw(twin_window_t *window)
{
	twin_pixmap_t	*px = window->pixmap;
	pboot_rpane_t	*rpane = window->client_data;
	pboot_device_t	*dev;
	twin_path_t	*path;
	twin_fixed_t	x, y, w, h;
	int		i;

	/* Fill background */
	twin_fill(px, 0x00000000, TWIN_SOURCE, 0, 0, px->width, px->height);

	/* Nothing to draw, return */
	if (pboot_dev_sel < 0)
		return;

	/* Create a path for use later */
	path = twin_path_create();
	assert(path);

	/* Draw focus box */
	if (rpane->focus_curindex >= 0 &&
	    twin_rect_intersect(rpane->focus_box, px->clip)) {
		x = twin_int_to_fixed(rpane->focus_box.left + 2);
		y = twin_int_to_fixed(rpane->focus_box.top + 2);
		w = twin_int_to_fixed(rpane->focus_box.right -
				      rpane->focus_box.left - 4);
		h = twin_int_to_fixed(rpane->focus_box.bottom -
				      rpane->focus_box.top - 4);
		twin_path_rounded_rectangle(path, x, y, w, h,
					    PBOOT_RIGHT_FOCUS_XRAD,
					    PBOOT_RIGHT_FOCUS_YRAD);
		if (!pboot_focus_lpane)
			twin_paint_path(px, PBOOT_FOCUS_COLOR, path);
		else
			twin_paint_stroke(px, PBOOT_FOCUS_COLOR, path,
					  4 * TWIN_FIXED_ONE);
	}

	/* Get device and iterate through options */
	dev = pboot_devices[pboot_dev_sel];
	for (i = 0; i < dev->option_count; i++) {
		pboot_option_t	*opt = &dev->options[i];
		twin_operand_t	src;

		if (opt->title == NULL)
			continue;
		if (!twin_rect_intersect(opt->box, px->clip))
			continue;
		if (opt->cache == NULL)
			pboot_draw_option_cache(dev, opt, i);

		src.source_kind = TWIN_PIXMAP;
		src.u.pixmap = opt->cache;

		twin_composite(px, opt->box.left, opt->box.top,
			       &src, 0, 0, NULL, 0, 0, TWIN_OVER,
			       opt->box.right - opt->box.left,
			       opt->box.bottom - opt->box.top);
	}

	/* Destroy path */
	twin_path_destroy(path);
}

static twin_time_t pboot_rfocus_timeout (twin_time_t now, void *closure)
{
	int dir = 1, dist, pos;
	const int accel[11] = { 7, 4, 2, 1, 1, 1, 1, 2, 3, 4, 5 };

	dist = abs(pboot_rpane->focus_target - pboot_rpane->focus_start);
	dir = dist > 5 ? 5 : dist;
	pos = pboot_rpane->focus_target - (int)pboot_rpane->focus_box.top;
	if (pos == 0) {
		return -1;
	}
	if (pos < 0) {
		dir = -dir;
		pos = -pos;
	}
	twin_window_damage(pboot_rpane->window,
			   pboot_rpane->focus_box.left,
			   pboot_rpane->focus_box.top,
			   pboot_rpane->focus_box.right,
			   pboot_rpane->focus_box.bottom);

	pboot_rpane->focus_box.top += dir;
	pboot_rpane->focus_box.bottom += dir;

	twin_window_damage(pboot_rpane->window,
			   pboot_rpane->focus_box.left,
			   pboot_rpane->focus_box.top,
			   pboot_rpane->focus_box.right,
			   pboot_rpane->focus_box.bottom);

	twin_window_queue_paint(pboot_rpane->window);

	return accel[(pos * 10) / dist];
}

static void pboot_set_rfocus(int index)
{
	pboot_device_t	*dev;

	if (pboot_dev_sel < 0 || pboot_dev_sel >= pboot_dev_count)
		return;
	dev = pboot_devices[pboot_dev_sel];
	if (index < 0 || index >= dev->option_count)
		return;

	pboot_rpane->focus_start = pboot_rpane->focus_box.top;
	pboot_rpane->focus_target = PBOOT_RIGHT_FOCUS_YOFF +
		PBOOT_RIGHT_OPTION_STRIDE * index;
	pboot_rpane->focus_curindex = index;

	twin_set_timeout(pboot_rfocus_timeout, 0, NULL);
}

static void pboot_select_rpane(void)
{
	if (pboot_focus_lpane == 0)
		return;
	pboot_focus_lpane = 0;

	twin_screen_set_active(pboot_screen, pboot_rpane->window->pixmap);

	twin_window_damage(pboot_lpane->window,
			   pboot_lpane->focus_box.left,
			   pboot_lpane->focus_box.top,
			   pboot_lpane->focus_box.right,
			   pboot_lpane->focus_box.bottom);

	twin_window_damage(pboot_rpane->window,
			   pboot_rpane->focus_box.left,
			   pboot_rpane->focus_box.top,
			   pboot_rpane->focus_box.right,
			   pboot_rpane->focus_box.bottom);

	twin_window_queue_paint(pboot_lpane->window);
	twin_window_queue_paint(pboot_rpane->window);

	pboot_set_rfocus(0);
}

static void pboot_select_lpane(void)
{
	if (pboot_focus_lpane == 1)
		return;
	pboot_focus_lpane = 1;

	twin_screen_set_active(pboot_screen, pboot_lpane->window->pixmap);

	twin_window_damage(pboot_lpane->window,
			   pboot_lpane->focus_box.left,
			   pboot_lpane->focus_box.top,
			   pboot_lpane->focus_box.right,
			   pboot_lpane->focus_box.bottom);

	twin_window_damage(pboot_rpane->window,
			   pboot_rpane->focus_box.left,
			   pboot_rpane->focus_box.top,
			   pboot_rpane->focus_box.right,
			   pboot_rpane->focus_box.bottom);

	twin_window_queue_paint(pboot_lpane->window);
	twin_window_queue_paint(pboot_rpane->window);
}

static void pboot_rpane_mousetrack(twin_coord_t x, twin_coord_t y)
{
	pboot_device_t	*dev;
	pboot_option_t	*opt;
	int		candidate = -1;

	if (pboot_dev_sel < 0 || pboot_dev_sel >= pboot_dev_count)
		return;
	dev = pboot_devices[pboot_dev_sel];

	if (y < PBOOT_RIGHT_OPTION_TMARGIN)
		goto miss;
	candidate = (y - PBOOT_RIGHT_OPTION_TMARGIN) /
		PBOOT_RIGHT_OPTION_STRIDE;
	if (candidate >= dev->option_count) {
		candidate = -1;
		goto miss;
	}
	if (candidate == pboot_rpane->mouse_target)
		return;
	opt = &dev->options[candidate];
	if (x < opt->box.left || x > opt->box.right ||
	    y < opt->box.top || y > opt->box.bottom) {
		candidate = -1;
		goto miss;
	}

	/* Ok, so now, we know the mouse hit an icon that wasn't the same
	 * as the previous one, we trigger a focus change
	 */
	pboot_set_rfocus(candidate);

 miss:
	pboot_rpane->mouse_target = candidate;
}

static void pboot_choose_option(void)
{
	pboot_device_t *dev = pboot_devices[pboot_dev_sel];
	pboot_option_t *opt = &dev->options[pboot_rpane->focus_curindex];

	LOG("Selected device %s\n", opt->title);

	/* Give user feedback, make sure errors and panics will be seen */
	pboot_exec_option(opt->data);
}

static twin_bool_t pboot_rpane_event (twin_window_t	    *window,
				      twin_event_t	    *event)
{
	/* filter out all mouse events */
	switch(event->kind) {
	case TwinEventEnter:
	case TwinEventMotion:
	case TwinEventLeave:
		pboot_select_rpane();
		pboot_rpane_mousetrack(event->u.pointer.x, event->u.pointer.y);
		return TWIN_TRUE;
	case TwinEventButtonDown:
		pboot_select_rpane();
		pboot_rpane_mousetrack(event->u.pointer.x, event->u.pointer.y);
		pboot_choose_option();
	case TwinEventButtonUp:
		return TWIN_TRUE;
	case TwinEventKeyDown:
		switch(event->u.key.key) {
		case KEY_UP:
			pboot_set_rfocus(pboot_rpane->focus_curindex - 1);
			return TWIN_TRUE;
		case KEY_DOWN:
			pboot_set_rfocus(pboot_rpane->focus_curindex + 1);
			return TWIN_TRUE;
		case KEY_LEFT:
			pboot_select_lpane();
			return TWIN_TRUE;
		case KEY_ENTER:
			pboot_choose_option();
		default:
			break;
		}
		break;
	default:
		break;
	}
	return TWIN_FALSE;
}


int pboot_add_option(int devindex, const char *title,
		     const char *subtitle, twin_pixmap_t *badge, void *data)
{
	pboot_device_t	*dev;
	pboot_option_t	*opt;
	twin_coord_t	width;
	int		index;

	if (devindex < 0 || devindex >= pboot_dev_count)
		return -1;
	dev = pboot_devices[devindex];

	if (dev->option_count >= PBOOT_MAX_OPTION)
		return -1;
	index = dev->option_count++;
	opt = &dev->options[index];

	opt->title = malloc(strlen(title) + 1);
	strcpy(opt->title, title);

	if (subtitle) {
		opt->subtitle = malloc(strlen(subtitle) + 1);
		strcpy(opt->subtitle, subtitle);
	} else
		opt->subtitle = NULL;

	opt->badge = badge;
	opt->cache = NULL;

	width = pboot_rpane->window->pixmap->width -
		(PBOOT_RIGHT_OPTION_LMARGIN + PBOOT_RIGHT_OPTION_RMARGIN);

	opt->box.left = PBOOT_RIGHT_OPTION_LMARGIN;
	opt->box.right = opt->box.left + width;
	opt->box.top = PBOOT_RIGHT_OPTION_TMARGIN +
		index * PBOOT_RIGHT_OPTION_STRIDE;
	opt->box.bottom = opt->box.top + PBOOT_RIGHT_OPTION_HEIGHT;

	opt->data = data;
	return index;
}


static void pboot_set_device_select(int sel)
{
	LOG("%s: %d -> %d\n", __FUNCTION__, pboot_dev_sel, sel);
	if (sel == pboot_dev_sel || sel >= pboot_dev_count)
		return;
	pboot_dev_sel = sel;
	pboot_rpane->focus_curindex = -1;
	pboot_rpane->mouse_target = -1;
	pboot_rpane->focus_box.top = -2*PBOOT_RIGHT_FOCUS_HEIGHT;
	pboot_rpane->focus_box.bottom = pboot_rpane->focus_box.top +
		PBOOT_RIGHT_FOCUS_HEIGHT;
	twin_window_damage(pboot_rpane->window, 0, 0,
			   pboot_rpane->window->pixmap->width,
			   pboot_rpane->window->pixmap->height);
	twin_window_queue_paint(pboot_rpane->window);
}

static twin_time_t pboot_lfocus_timeout (twin_time_t now, void *closure)
{
	int dir = 1, dist, pos;
	const int accel[11] = { 7, 4, 2, 1, 1, 1, 1, 2, 3, 4, 5 };

	dist = abs(pboot_lpane->focus_target - pboot_lpane->focus_start);
	pos = pboot_lpane->focus_target - (int)pboot_lpane->focus_box.top;
	if (pos == 0) {
		pboot_set_device_select(pboot_lpane->focus_curindex);
		return -1;
	}
	if (pos < 0) {
		dir = -1;
		pos = -pos;
	}
	twin_window_damage(pboot_lpane->window,
			   pboot_lpane->focus_box.left,
			   pboot_lpane->focus_box.top,
			   pboot_lpane->focus_box.right,
			   pboot_lpane->focus_box.bottom);

	pboot_lpane->focus_box.top += dir;
	pboot_lpane->focus_box.bottom += dir;

	twin_window_damage(pboot_lpane->window,
			   pboot_lpane->focus_box.left,
			   pboot_lpane->focus_box.top,
			   pboot_lpane->focus_box.right,
			   pboot_lpane->focus_box.bottom);

	twin_window_queue_paint(pboot_lpane->window);

	return accel[(pos * 10) / dist];
}

static void pboot_set_lfocus(int index)
{
	if (index >= pboot_dev_count)
		return;

	pboot_lpane->focus_start = pboot_lpane->focus_box.top;

	if (index < 0)
		pboot_lpane->focus_target = 0 - PBOOT_LEFT_FOCUS_HEIGHT;
	else
		pboot_lpane->focus_target = PBOOT_LEFT_FOCUS_YOFF +
			PBOOT_LEFT_ICON_STRIDE * index;

	pboot_lpane->focus_curindex = index;

	twin_set_timeout(pboot_lfocus_timeout, 0, NULL);
}

static void pboot_lpane_mousetrack(twin_coord_t x, twin_coord_t y)
{
	int candidate = -1;
	twin_coord_t icon_top;

	if (x < PBOOT_LEFT_ICON_XOFF ||
	    x > (PBOOT_LEFT_ICON_XOFF + PBOOT_LEFT_ICON_WIDTH))
		goto miss;
	if (y < PBOOT_LEFT_ICON_YOFF)
		goto miss;
	candidate = (y - PBOOT_LEFT_ICON_YOFF) / PBOOT_LEFT_ICON_STRIDE;
	if (candidate >= pboot_dev_count) {
		candidate = -1;
		goto miss;
	}
	if (candidate == pboot_lpane->mouse_target)
		return;
	icon_top = PBOOT_LEFT_ICON_YOFF +
		candidate * PBOOT_LEFT_ICON_STRIDE;
	if (y > (icon_top + PBOOT_LEFT_ICON_HEIGHT)) {
		candidate = -1;
		goto miss;
	}

	/* Ok, so now, we know the mouse hit an icon that wasn't the same
	 * as the previous one, we trigger a focus change
	 */
	pboot_set_lfocus(candidate);

 miss:
	pboot_lpane->mouse_target = candidate;
}

static twin_bool_t pboot_lpane_event (twin_window_t	    *window,
				      twin_event_t	    *event)
{
	/* filter out all mouse events */
	switch(event->kind) {
	case TwinEventEnter:
	case TwinEventMotion:
	case TwinEventLeave:
		pboot_select_lpane();
		pboot_lpane_mousetrack(event->u.pointer.x, event->u.pointer.y);
		return TWIN_TRUE;
	case TwinEventButtonDown:
	case TwinEventButtonUp:
		return TWIN_TRUE;
	case TwinEventKeyDown:
		switch(event->u.key.key) {
		case KEY_UP:
			if (pboot_lpane->focus_curindex > 0)
				pboot_set_lfocus(
					pboot_lpane->focus_curindex - 1);
			return TWIN_TRUE;
		case KEY_DOWN:
			pboot_set_lfocus(pboot_lpane->focus_curindex + 1);
			return TWIN_TRUE;
		case KEY_RIGHT:
			pboot_select_rpane();
			return TWIN_TRUE;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return TWIN_FALSE;
}

twin_bool_t pboot_event_filter(twin_screen_t	    *screen,
			       twin_event_t	    *event)
{
	switch(event->kind) {
	case TwinEventEnter:
	case TwinEventMotion:
	case TwinEventLeave:
	case TwinEventButtonDown:
	case TwinEventButtonUp:
		if (pboot_cursor != NULL)
			twin_screen_set_cursor(pboot_screen, pboot_cursor,
					       pboot_cursor_hx,
					       pboot_cursor_hy);
		break;
	case TwinEventKeyDown:
	case TwinEventKeyUp:
		twin_screen_set_cursor(pboot_screen, NULL, 0, 0);
		break;
	default:
		break;
	}
	return TWIN_FALSE;
}

static void pboot_lpane_draw(twin_window_t *window)
{
	twin_pixmap_t	*px = window->pixmap;
	pboot_lpane_t	*lpane = window->client_data;
	twin_path_t	*path;
	twin_fixed_t	x, y, w, h;
	int		i;

	/* Fill background */
	twin_fill(px, PBOOT_LEFT_PANE_COLOR, TWIN_SOURCE,
		  0, 0, px->width, px->height);

	/* Create a path for use later */
	path = twin_path_create();
	assert(path);

	/* Draw right line if needed */
	if (px->clip.right > (PBOOT_LEFT_PANE_SIZE - 4)) {
		x = twin_int_to_fixed(PBOOT_LEFT_PANE_SIZE - 4);
		y = twin_int_to_fixed(px->height);
		twin_path_rectangle(path, x, 0, 0x40000, y);
		twin_paint_path(px, PBOOT_LEFT_LINE_COLOR, path);
		twin_path_empty(path);
	}

	/* Draw focus box */
	if (lpane->focus_curindex >= 0 &&
	    twin_rect_intersect(lpane->focus_box, px->clip)) {
		x = twin_int_to_fixed(lpane->focus_box.left + 2);
		y = twin_int_to_fixed(lpane->focus_box.top + 2);
		w = twin_int_to_fixed(lpane->focus_box.right -
				      lpane->focus_box.left - 4);
		h = twin_int_to_fixed(lpane->focus_box.bottom -
				      lpane->focus_box.top - 4);
		twin_path_rounded_rectangle(path, x, y, w, h,
					    PBOOT_LEFT_FOCUS_XRAD,
					    PBOOT_LEFT_FOCUS_YRAD);
		if (pboot_focus_lpane)
			twin_paint_path(px, PBOOT_FOCUS_COLOR, path);
		else
			twin_paint_stroke(px, PBOOT_FOCUS_COLOR, path,
					  4 * TWIN_FIXED_ONE);
	}

	/* Draw icons */
	for (i = 0; i < pboot_dev_count; i++) {
		pboot_device_t	*dev = pboot_devices[i];
		twin_operand_t	src;

		if (!twin_rect_intersect(dev->box, px->clip))
			continue;

		src.source_kind = TWIN_PIXMAP;
		src.u.pixmap = dev->badge;

		twin_composite(px, dev->box.left, dev->box.top,
			       &src, 0, 0, NULL, 0, 0, TWIN_OVER,
			       dev->box.right - dev->box.left,
			       dev->box.bottom - dev->box.top);

	}

	/* Destroy path */
	twin_path_destroy(path);
}

static void pboot_create_panels(void)
{
	/* left pane */
	pboot_lpane = calloc(1, sizeof(pboot_lpane_t));
	assert(pboot_lpane);

	pboot_lpane->window = twin_window_create(pboot_screen, TWIN_ARGB32,
						 TwinWindowPlain,
						 0, 0, PBOOT_LEFT_PANE_SIZE,
						 pboot_screen->height);
	assert(pboot_lpane->window);

	pboot_lpane->window->draw = pboot_lpane_draw;
	pboot_lpane->window->event = pboot_lpane_event;
	pboot_lpane->window->client_data = pboot_lpane;
	pboot_lpane->focus_curindex = -1;
	pboot_lpane->focus_box.left = PBOOT_LEFT_FOCUS_XOFF;
	pboot_lpane->focus_box.top = -2*PBOOT_LEFT_FOCUS_HEIGHT;
	pboot_lpane->focus_box.right = pboot_lpane->focus_box.left +
		PBOOT_LEFT_FOCUS_WIDTH;
	pboot_lpane->focus_box.bottom = pboot_lpane->focus_box.top +
		PBOOT_LEFT_FOCUS_HEIGHT;
	pboot_lpane->mouse_target = -1;
	twin_window_show(pboot_lpane->window);

	/* right pane */
	pboot_rpane = calloc(1, sizeof(pboot_rpane_t));
	assert(pboot_rpane);

	pboot_rpane->window = twin_window_create(pboot_screen, TWIN_ARGB32,
						 TwinWindowPlain,
						 PBOOT_LEFT_PANE_SIZE, 0,
						 pboot_screen->width -
						   PBOOT_LEFT_PANE_SIZE,
						 pboot_screen->height);
	assert(pboot_rpane->window);

	pboot_rpane->window->draw = pboot_rpane_draw;
	pboot_rpane->window->event = pboot_rpane_event;
	pboot_rpane->window->client_data = pboot_rpane;

	pboot_rpane->focus_curindex = -1;
	pboot_rpane->focus_box.left = PBOOT_RIGHT_FOCUS_XOFF;
	pboot_rpane->focus_box.top = -2*PBOOT_RIGHT_FOCUS_HEIGHT;
	pboot_rpane->focus_box.right = pboot_rpane->window->pixmap->width -
		2 * PBOOT_RIGHT_FOCUS_XOFF;
	pboot_rpane->focus_box.bottom = pboot_rpane->focus_box.top +
		PBOOT_RIGHT_FOCUS_HEIGHT;
	pboot_rpane->mouse_target = -1;
	twin_window_show(pboot_rpane->window);
}

int pboot_add_device(const char *dev_id, const char *name,
		twin_pixmap_t *pixmap)
{
	int		index;
	pboot_device_t	*dev;

	if (pboot_dev_count >= PBOOT_MAX_DEV)
		return -1;

	index = pboot_dev_count++;

	dev = malloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
	dev->id = malloc(strlen(dev_id) + 1);
	strcpy(dev->id, dev_id);
	dev->badge = pixmap;
	dev->box.left = PBOOT_LEFT_ICON_XOFF;
	dev->box.right = dev->box.left + PBOOT_LEFT_ICON_WIDTH;
	dev->box.top = PBOOT_LEFT_ICON_YOFF +
		PBOOT_LEFT_ICON_STRIDE * index;
	dev->box.bottom = dev->box.top + PBOOT_LEFT_ICON_HEIGHT;

	pboot_devices[index] = dev;

	twin_window_damage(pboot_lpane->window,
			   dev->box.left, dev->box.top,
			   dev->box.right, dev->box.bottom);
	twin_window_queue_paint(pboot_lpane->window);

	return index;
}

int pboot_remove_device(const char *dev_id)
{
	int		i, new_dev_index;
	pboot_device_t	*dev = NULL;

	/* find the matching device */
	for (i = 0; i < pboot_dev_count; i++) {
		if (!strcmp(pboot_devices[i]->id, dev_id)) {
			dev = pboot_devices[i];
			break;
		}
	}

	if (!dev)
		return TWIN_FALSE;

	/* select the newly-focussed device */
	if (i == pboot_dev_count - 1)
		new_dev_index = i - 1;
	else
		new_dev_index = i + 1;

	memmove(pboot_devices + i, pboot_devices + i + 1,
			sizeof(*pboot_devices) * (pboot_dev_count + i - 1));

	pboot_devices[--pboot_dev_count] = NULL;

	pboot_set_device_select(new_dev_index);
	twin_window_damage(pboot_lpane->window,
			   dev->box.left, dev->box.top,
			   dev->box.right, dev->box.bottom);
	twin_window_queue_paint(pboot_lpane->window);

	/* todo: free device & options */

	return TWIN_TRUE;
}

static void pboot_make_background(void)
{
	twin_pixmap_t	*filepic, *scaledpic;
	const char	*background_path;

	/* Set background pixmap */
	LOG("loading background...");
	background_path = artwork_pathname("background.jpg");
	filepic = twin_jpeg_to_pixmap(background_path, TWIN_ARGB32);
	LOG("%s\n", filepic ? "ok" : "failed");

	if (filepic == NULL)
		return;

	if (pboot_screen->height == filepic->height &&
	    pboot_screen->width == filepic->width)
		scaledpic = filepic;
	else {
		twin_fixed_t	sx, sy;
		twin_operand_t	srcop;

		scaledpic = twin_pixmap_create(TWIN_ARGB32,
					       pboot_screen->width,
					       pboot_screen->height);
		if (scaledpic == NULL) {
			twin_pixmap_destroy(filepic);
			return;
		}
		sx = twin_fixed_div(twin_int_to_fixed(filepic->width),
				    twin_int_to_fixed(pboot_screen->width));
		sy = twin_fixed_div(twin_int_to_fixed(filepic->height),
				    twin_int_to_fixed(pboot_screen->height));
		
		twin_matrix_scale(&filepic->transform, sx, sy);
		srcop.source_kind = TWIN_PIXMAP;
		srcop.u.pixmap = filepic;
		twin_composite(scaledpic, 0, 0, &srcop, 0, 0,
			       NULL, 0, 0, TWIN_SOURCE,
			       pboot_screen->width, pboot_screen->height);
		twin_pixmap_destroy(filepic);
			       
	}
	twin_screen_set_background(pboot_screen, scaledpic);
}

static void exitfunc(void)
{
#ifndef _USE_X11
	if (pboot_fbdev)
		twin_fbdev_destroy(pboot_fbdev);
	pboot_fbdev = NULL;
#endif
}

static void sigint(int sig)
{
	exitfunc();
	syscall(__NR_exit);
}

static void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [-u] [-h]\n", progname);
}

int main(int argc, char **argv)
{
	int c;
	int udev_trigger = 0;

	for (;;) {
		c = getopt(argc, argv, "u::h");
		if (c == -1)
			break;

		switch (c) {
		case 'u':
			udev_trigger = 1;
			break;
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	atexit(exitfunc);
	signal(SIGINT, sigint);

#ifdef _USE_X11
	pboot_x11 = twin_x11_create(XOpenDisplay(0), 1024, 768);
	if (pboot_x11 == NULL) {
		perror("failed to create x11 screen !\n");
		return 1;
	}
	pboot_screen = pboot_x11->screen;
#else
	/* Create screen and mouse drivers */
	pboot_fbdev = twin_fbdev_create(-1, SIGUSR1);
	if (pboot_fbdev == NULL) {
		perror("failed to create fbdev screen !\n");
		return 1;
	}
	pboot_screen = pboot_fbdev->screen;
	twin_linux_mouse_create(NULL, pboot_screen);

	if (pboot_fbdev != NULL) {
		char *cursor_path = artwork_pathname("cursor");
		pboot_cursor = twin_load_X_cursor(cursor_path, 2,
						  &pboot_cursor_hx,
						  &pboot_cursor_hy);
		if (pboot_cursor == NULL)
			pboot_cursor =
				twin_get_default_cursor(&pboot_cursor_hx,
							&pboot_cursor_hy);
	}
#endif

	/* Set background pixmap */
	pboot_make_background();

	/* Init more stuffs */
	pboot_create_panels();
	twin_window_queue_paint(pboot_lpane->window);
	twin_window_queue_paint(pboot_rpane->window);

	if (!pboot_start_device_discovery(udev_trigger)) {
		LOG("Couldn't start device discovery!\n");
		return 1;
	}

	pboot_set_lfocus(0);
	twin_screen_set_active(pboot_screen, pboot_lpane->window->pixmap);
	pboot_screen->event_filter = pboot_event_filter;

	/* Console switch */
#ifndef _USE_X11
	if (pboot_fbdev)
		twin_fbdev_activate(pboot_fbdev);
#endif

	/* Process events */
	twin_dispatch ();

	return 0;
}
