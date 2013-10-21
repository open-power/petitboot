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
#include <string.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "nc-boot-editor.h"
#include "nc-widgets.h"

static struct boot_editor *boot_editor_from_scr(struct nc_scr *scr)
{
	struct boot_editor *boot_editor;

	assert(scr->sig == pb_boot_editor_sig);
	boot_editor = (struct boot_editor *)
		((char *)scr - (size_t)&((struct boot_editor *)0)->scr);
	assert(boot_editor->scr.sig == pb_boot_editor_sig);
	return boot_editor;
}

static struct boot_editor *boot_editor_from_arg(void *arg)
{
	struct boot_editor *boot_editor = arg;

	assert(boot_editor->scr.sig == pb_boot_editor_sig);
	return boot_editor;
}

static int boot_editor_post(struct nc_scr *scr)
{
	struct boot_editor *boot_editor = boot_editor_from_scr(scr);

	widgetset_post(boot_editor->widgetset);
	nc_scr_frame_draw(scr);
	redrawwin(boot_editor->scr.main_ncw);
	wrefresh(boot_editor->scr.main_ncw);

	return 0;
}

static int boot_editor_unpost(struct nc_scr *scr)
{
	widgetset_unpost(boot_editor_from_scr(scr)->widgetset);
	return 0;
}

static void boot_editor_resize(struct nc_scr *scr)
{
	/* FIXME: forms can't be resized, need to recreate here */
	boot_editor_unpost(scr);
	boot_editor_post(scr);
}

static struct pb_boot_data *boot_editor_prepare_data(
		struct boot_editor *boot_editor)
{
	struct pb_boot_data *bd;
	char *s;

	bd = talloc(boot_editor, struct pb_boot_data);

	if (!bd)
		return NULL;

	s = widget_textbox_get_value(boot_editor->widgets.image_f);
	bd->image = *s ? talloc_strdup(bd, s) : NULL;

	s = widget_textbox_get_value(boot_editor->widgets.initrd_f);
	bd->initrd = *s ? talloc_strdup(bd, s) : NULL;

	s = widget_textbox_get_value(boot_editor->widgets.dtb_f);
	bd->dtb = *s ? talloc_strdup(bd, s) : NULL;

	s = widget_textbox_get_value(boot_editor->widgets.args_f);
	bd->args = *s ? talloc_strdup(bd, s) : NULL;

	return bd;
}

/**
 * boot_editor_process_key - Process a user keystroke.
 *
 * Called from the cui via the scr:process_key method.
 */

static void boot_editor_process_key(struct nc_scr *scr, int key)
{
	struct boot_editor *boot_editor = boot_editor_from_scr(scr);
	bool handled;

	handled = widgetset_process_key(boot_editor->widgetset, key);
	if (handled) {
		wrefresh(boot_editor->scr.main_ncw);
		return;
	}

	switch (key) {
	case 'x':
	case 27: /* ESC */
		boot_editor->on_exit(boot_editor, boot_editor_cancel, NULL);
		nc_flush_keys();
	}
}

/**
 * boot_editor_destructor - The talloc destructor for a boot_editor.
 */

static int boot_editor_destructor(void *arg)
{
	struct boot_editor *boot_editor = boot_editor_from_arg(arg);
	boot_editor->scr.sig = pb_removed_sig;
	return 0;
}

static void ok_click(void *arg)
{
	struct boot_editor *boot_editor = arg;
	struct pb_boot_data *bd;

	bd = boot_editor_prepare_data(boot_editor);
	boot_editor->on_exit(boot_editor, boot_editor_update, bd);
}

static void cancel_click(void *arg)
{
	struct boot_editor *boot_editor = arg;
	boot_editor->on_exit(boot_editor, boot_editor_cancel, NULL);
}

struct boot_editor *boot_editor_init(struct pmenu *menu,
		const struct pb_boot_data *bd,
		void (*on_exit)(struct boot_editor *,
				enum boot_editor_result,
				struct pb_boot_data *))
{
	int y, field_size, label_x = 1, field_x = 9;
	char *image, *initrd, *dtb, *args;
	struct boot_editor *boot_editor;
	struct nc_widgetset *set;

	assert(on_exit);

	boot_editor = talloc_zero(menu, struct boot_editor);

	if (!boot_editor)
		return NULL;

	talloc_set_destructor(boot_editor, boot_editor_destructor);
	boot_editor->original_pmenu = menu;

	nc_scr_init(&boot_editor->scr, pb_boot_editor_sig, 0,
			menu, boot_editor_process_key,
		boot_editor_post, boot_editor_unpost, boot_editor_resize);

	boot_editor->scr.frame.ltitle = talloc_strdup(boot_editor,
			"Petitboot Option Editor");
	boot_editor->scr.frame.rtitle = NULL;
	boot_editor->scr.frame.help = talloc_strdup(boot_editor,
			"Enter=accept");

	boot_editor->on_exit = on_exit;

	if (bd) {
		image = bd->image;
		initrd = bd->initrd;
		dtb = bd->dtb;
		args = bd->args;
	} else {
		image = initrd = dtb = args = "";
	}

	y = 0;
	field_size = COLS - 1 - field_x;

	boot_editor->widgetset = set = widgetset_create(boot_editor,
			boot_editor->scr.main_ncw,
			boot_editor->scr.sub_ncw);

	boot_editor->widgets.image_l = widget_new_label(set, y,
					label_x, "image:");
	boot_editor->widgets.image_f = widget_new_textbox(set, y,
					field_x, field_size, image);

	y++;
	boot_editor->widgets.initrd_l = widget_new_label(set, y,
					label_x, "initrd:");
	boot_editor->widgets.initrd_f = widget_new_textbox(set, y,
					field_x, field_size, initrd);

	y++;
	boot_editor->widgets.dtb_l = widget_new_label(set, y,
					label_x, "dtb:");
	boot_editor->widgets.dtb_f = widget_new_textbox(set, y,
					field_x, field_size, dtb);

	y++;
	boot_editor->widgets.args_l = widget_new_label(set, y,
					label_x, "args:");
	boot_editor->widgets.args_f = widget_new_textbox(set, y,
					field_x, field_size, args);

	y++;
	y++;
	boot_editor->widgets.ok_b = widget_new_button(set, y,
					9, 6, "OK", ok_click, boot_editor);
	boot_editor->widgets.cancel_b = widget_new_button(set, y,
					19, 6, "Cancel", cancel_click,
					boot_editor);

	return boot_editor;
}
