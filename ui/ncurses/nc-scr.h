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

#if !defined(_PB_NC_SCR_H)
#define _PB_NC_SCR_H

#include <linux/input.h> /* This must be included before ncurses.h */
#if defined HAVE_NCURSESW_CURSES_H
#  include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
#  include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
#  include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#  include <ncurses.h>
#elif defined HAVE_CURSES_H
#  include <curses.h>
#else
#  error "Curses header file not found."
#endif

#define DBG(fmt, args...) pb_debug("DBG: " fmt, ## args)
#define DBGS(fmt, args...) \
	pb_debug("DBG:%s:%d: " fmt, __func__, __LINE__, ## args)


enum pb_nc_sig {
	pb_cui_sig		= 111,
	pb_pmenu_sig		= 222,
	pb_item_sig		= 333,
	pb_boot_editor_sig	= 444,
	pb_text_screen_sig	= 555,
	pb_config_screen_sig	= 666,
	pb_lang_screen_sig	= 777,
	pb_add_url_screen_sig	= 888,
	pb_subset_screen_sig	= 101,
	pb_removed_sig		= -999,
};

static inline void nc_flush_keys(void)
{
	while (getch() != ERR)
		(void)0;
}

enum nc_scr_pos {
	nc_scr_pos_title = 0,
	nc_scr_pos_title_sep = 1,
	nc_scr_pos_lrtitle_space = 2,
	nc_scr_pos_sub = 2,

	nc_scr_pos_help_sep = 3,
	nc_scr_pos_help = 2,
	nc_scr_pos_status = 1,

	nc_scr_frame_lines = 5,
	nc_scr_frame_cols = 1,
};

struct nc_frame {
	char *ltitle;
	char *rtitle;
	char *help;
	char *status;
};

struct nc_scr {
	enum pb_nc_sig sig;
	struct nc_frame frame;
	WINDOW *main_ncw;
	WINDOW *sub_ncw;
	void *ui_ctx;
	int (*post)(struct nc_scr *scr);
	int (*unpost)(struct nc_scr *scr);
	void (*process_key)(struct nc_scr *scr, int key);
	void (*resize)(struct nc_scr *scr);
};

int nc_scr_init(struct nc_scr *scr, enum pb_nc_sig sig, int begin_x,
	void *ui_ctx,
	void (*process_key)(struct nc_scr *, int),
	int (*post)(struct nc_scr *),
	int (*unpost)(struct nc_scr *),
	void (*resize)(struct nc_scr *));
void nc_scr_status_free(struct nc_scr *scr);
void nc_scr_status_printf(struct nc_scr *scr, const char *format, ...);
void nc_scr_frame_draw(struct nc_scr *scr);

int nc_scr_post(struct nc_scr *src);
int nc_scr_unpost(struct nc_scr *src);

#endif
