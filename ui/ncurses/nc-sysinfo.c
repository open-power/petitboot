/*
 *  Copyright (C) 2013 IBM Corporation
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

#define _GNU_SOURCE


#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <util/util.h>

#include "config.h"
#include "nc-cui.h"
#include "nc-sysinfo.h"

struct sysinfo_screen {
	struct nc_scr	scr;
	struct cui	*cui;
	char		**lines;
	int		n_lines;
	int		n_alloc_lines;
	int		scroll_y;
	void		(*on_exit)(struct cui *);
};

static struct sysinfo_screen *sysinfo_screen_from_scr(struct nc_scr *scr)
{
	struct sysinfo_screen *sysinfo_screen;

	assert(scr->sig == pb_sysinfo_screen_sig);
	sysinfo_screen = (struct sysinfo_screen *)
		((char *)scr - (size_t)&((struct sysinfo_screen *)0)->scr);
	assert(sysinfo_screen->scr.sig == pb_sysinfo_screen_sig);
	return sysinfo_screen;
}

static void sysinfo_screen_draw(struct sysinfo_screen *screen)
{
	int max_y, i;

	max_y = getmaxy(screen->scr.sub_ncw);

	max_y = min(max_y, screen->scroll_y + screen->n_lines);

	for (i = screen->scroll_y; i < max_y; i++)
		mvwaddstr(screen->scr.sub_ncw, i, 1, screen->lines[i]);

	wrefresh(screen->scr.sub_ncw);
}

static void sysinfo_screen_scroll(struct sysinfo_screen *screen, int key)
{
	int win_lines = getmaxy(screen->scr.sub_ncw);
	int delta;

	if (key == KEY_UP)
		delta = -1;
	else if (key == KEY_DOWN)
		delta = 1;
	else
		return;

	if (screen->scroll_y + delta < 0)
		return;
	if (screen->scroll_y + delta + win_lines > screen->n_lines - 1)
		return;

	screen->scroll_y += delta;
	wscrl(screen->scr.sub_ncw, delta);

	if (delta > 0) {
		mvwaddstr(screen->scr.sub_ncw, win_lines - 1, 1,
				screen->lines[screen->scroll_y+win_lines-1]);
	} else if (delta < 0) {
		mvwaddstr(screen->scr.sub_ncw, 0, 1,
				screen->lines[screen->scroll_y]);
	}

	wrefresh(screen->scr.sub_ncw);
}

static void sysinfo_clear(struct sysinfo_screen *screen)
{
	talloc_free(screen->lines);
	screen->n_lines = 0;
	screen->n_alloc_lines = 16;
	screen->lines = talloc_array(screen, char *, screen->n_alloc_lines);
}

static __attribute__((format(printf, 2, 3))) void sysinfo_screen_append_line(
		struct sysinfo_screen *screen, const char *fmt, ...)
{
	char *line;
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		line = talloc_vasprintf(screen->lines, fmt, ap);
		va_end(ap);
	} else {
		line = "";
	}

	if (screen->n_lines == screen->n_alloc_lines) {
		screen->n_alloc_lines *= 2;
		screen->lines = talloc_realloc(screen, screen->lines,
						char *, screen->n_alloc_lines);
	}

	screen->lines[screen->n_lines] = line;
	screen->n_lines++;
}

static void if_info_mac_str(struct interface_info *info,
		char *buf, unsigned int buflen)
{
	return mac_str(info->hwaddr, info->hwaddr_size, buf, buflen);
}

static void sysinfo_screen_populate(struct sysinfo_screen *screen,
		const struct system_info *sysinfo)
{
	unsigned int i;

	sysinfo_clear(screen);

#define line(...) sysinfo_screen_append_line(screen, __VA_ARGS__)
	if (!sysinfo) {
		line("Waiting for system information...");
		return;
	}

	line("%-12s %s", "System type:", sysinfo->type ?: "");
	line("%-12s %s", "System id:",   sysinfo->identifier ?: "");

	if (sysinfo->n_blockdevs) {
		line(NULL);
		line("Storage devices");
	}

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *info = sysinfo->blockdevs[i];

		line("%s:", info->name);
		line(" UUID:       %s", info->uuid);
		line(" mounted at: %s", info->mountpoint);
		line(NULL);
	}

	if (sysinfo->n_interfaces) {
		line(NULL);
		line("Network interfaces");
	}

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *info = sysinfo->interfaces[i];
		char macbuf[32];

		if_info_mac_str(info, macbuf, sizeof(macbuf));

		line("%s:", info->name);
		line(" MAC: %s", macbuf);
		line(NULL);
	}

#undef line
}

static void sysinfo_screen_process_key(struct nc_scr *scr, int key)
{
	struct sysinfo_screen *screen = sysinfo_screen_from_scr(scr);

	switch (key) {
	case 'x':
		screen->on_exit(screen->cui);
		break;
	case KEY_DOWN:
	case KEY_UP:
		sysinfo_screen_scroll(screen, key);
		break;
	default:
		break;
	}
}

static void sysinfo_screen_resize(struct nc_scr *scr)
{
	struct sysinfo_screen *screen = sysinfo_screen_from_scr(scr);
	sysinfo_screen_draw(screen);
}

struct nc_scr *sysinfo_screen_scr(struct sysinfo_screen *screen)
{
	return &screen->scr;
}

void sysinfo_screen_update(struct sysinfo_screen *screen,
		const struct system_info *sysinfo)
{
	sysinfo_screen_populate(screen, sysinfo);
	sysinfo_screen_draw(screen);
}

struct sysinfo_screen *sysinfo_screen_init(struct cui *cui,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *))
{
	struct sysinfo_screen *screen;

	screen = talloc_zero(cui, struct sysinfo_screen);
	nc_scr_init(&screen->scr, pb_sysinfo_screen_sig, 0,
			cui, sysinfo_screen_process_key,
			NULL, NULL, sysinfo_screen_resize);

	screen->cui = cui;
	screen->on_exit = on_exit;

	screen->scr.frame.ltitle = talloc_strdup(screen,
			"Petitboot System Information");
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen, "x=exit");
	nc_scr_frame_draw(&screen->scr);

	sysinfo_screen_populate(screen, sysinfo);
	wrefresh(screen->scr.main_ncw);
	scrollok(screen->scr.sub_ncw, true);
	sysinfo_screen_draw(screen);

	return screen;
}
