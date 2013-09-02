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
#include <stdarg.h>

#include "log/log.h"
#include "talloc/talloc.h"

#include "nc-scr.h"

void nc_start(void)
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

	while (getch() != ERR)		/* flush stdin */
		(void)0;
}

void nc_atexit(void)
{
	clear();
	refresh();
	endwin();
}

static void nc_scr_status_clear(struct nc_scr *scr)
{
	mvwhline(scr->main_ncw, LINES - nc_scr_pos_status, 0, ' ', COLS);
}

static void nc_scr_status_draw(struct nc_scr *scr)
{
	mvwaddstr(scr->main_ncw, LINES - nc_scr_pos_status, 1,
		scr->frame.status);
}

void nc_scr_frame_draw(struct nc_scr *scr)
{
	DBGS("title '%s'\n", scr->frame.title);
	DBGS("help '%s'\n", scr->frame.help);
	DBGS("status '%s'\n", scr->frame.status);

	mvwaddstr(scr->main_ncw, nc_scr_pos_title, 1, scr->frame.title);
	mvwhline(scr->main_ncw, nc_scr_pos_title_sep, 1, ACS_HLINE, COLS - 2);

	mvwhline(scr->main_ncw, LINES - nc_scr_pos_help_sep, 1, ACS_HLINE,
		COLS - 2);
	mvwaddstr(scr->main_ncw, LINES - nc_scr_pos_help, 1, scr->frame.help);
	nc_scr_status_draw(scr);
}

void nc_scr_status_free(struct nc_scr *scr)
{
	talloc_free(scr->frame.status);
	scr->frame.status = NULL;
	nc_scr_status_clear(scr);
}

/**
 * nc_scr_status_printf - Set the text of the scr status using sprintf.
 * @scr: The scr to opperate on.
 * @text: The status text.
 *
 * The caller is reponsible for calling scr_draw() to update the display.
 */

void nc_scr_status_printf(struct nc_scr *scr, const char *format, ...)
{
	va_list ap;

	nc_scr_status_free(scr);

	va_start(ap, format);
	scr->frame.status = talloc_vasprintf(scr, format, ap);
	va_end(ap);

	nc_scr_status_draw(scr);
	wrefresh(scr->main_ncw);
}

int nc_scr_init(struct nc_scr *scr, enum pb_nc_sig sig, int begin_x,
	void *ui_ctx,
	void (*process_key)(struct nc_scr *, int),
	int (*post)(struct nc_scr *),
	int (*unpost)(struct nc_scr *),
	void (*resize)(struct nc_scr *))
{
	scr->sig = sig;
	scr->ui_ctx = ui_ctx;
	scr->process_key = process_key;
	scr->post = post;
	scr->unpost = unpost;
	scr->resize = resize;

	scr->main_ncw = newwin(LINES, COLS, 0, 0);

	scr->sub_ncw = derwin(scr->main_ncw,
		LINES - nc_scr_frame_lines,
		COLS - 1 - begin_x,
		nc_scr_pos_sub,
		begin_x);

	assert(scr->main_ncw);
	assert(scr->sub_ncw);

	return scr->main_ncw && scr->sub_ncw;
}
