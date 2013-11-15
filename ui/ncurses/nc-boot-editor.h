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

#if !defined(_PB_NC_KED_H)
#define _PB_NC_KED_H

#include <linux/input.h> /* This must be included before ncurses.h */
#if defined HAVE_NCURSESW_FORM_H
#  include <ncursesw/form.h>
#elif defined HAVE_NCURSES_FORM_H
#  include <ncurses/form.h>
#elif defined HAVE_FORM_H
#  include <form.h>
#else
#  error "Curses form.h not found."
#endif

#include "types/types.h"
#include "ui/common/ui-system.h"
#include "nc-scr.h"

enum boot_editor_attr_cursor {
	boot_editor_attr_cursor_ins = A_NORMAL,
	boot_editor_attr_cursor_ovl = A_NORMAL | A_UNDERLINE,
};

/**
 * enum boot_editor_result - Result code for boot_editor:on_exit().
 * @boot_editor_cancel: The user canceled the operation.
 * @boot_editor_update: The args were updated.
 */

enum boot_editor_result {
	boot_editor_cancel,
	boot_editor_update,
};

/**
 * struct boot_editor - kexec args editor.
 */

struct boot_editor {
	struct nc_scr	scr;
	void		*data;
	struct pmenu	*original_pmenu;
	void		(*on_exit)(struct boot_editor *boot_editor,
					enum boot_editor_result result,
					struct pb_boot_data *bd);
	enum boot_editor_attr_cursor attr_cursor;

	struct nc_widgetset	*widgetset;
	struct {
		struct nc_widget_label		*image_l;
		struct nc_widget_textbox	*image_f;
		struct nc_widget_label		*initrd_l;
		struct nc_widget_textbox	*initrd_f;
		struct nc_widget_label		*dtb_l;
		struct nc_widget_textbox	*dtb_f;
		struct nc_widget_label		*args_l;
		struct nc_widget_textbox	*args_f;
		struct nc_widget_button		*ok_b;
		struct nc_widget_button		*cancel_b;
	} widgets;
};

struct boot_editor *boot_editor_init(struct pmenu *menu,
		const struct pb_boot_data *bd,
		void (*on_exit)(struct boot_editor *,
				enum boot_editor_result,
				struct pb_boot_data *));

#endif
