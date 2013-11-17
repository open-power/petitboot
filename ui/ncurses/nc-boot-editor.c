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

struct boot_editor {
	struct nc_scr		scr;
	struct cui		*cui;
	void			*data;
	struct pmenu_item	*item;
	void			(*on_exit)(struct cui *cui,
					struct pmenu_item *item,
					struct pb_boot_data *bd);

	int			label_x;
	int			field_x;

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

struct nc_scr *boot_editor_scr(struct boot_editor *boot_editor)
{
	return &boot_editor->scr;
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
		boot_editor->on_exit(boot_editor->cui, NULL, NULL);
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
	boot_editor->on_exit(boot_editor->cui, boot_editor->item, bd);
}

static void cancel_click(void *arg)
{
	struct boot_editor *boot_editor = arg;
	boot_editor->on_exit(boot_editor->cui, NULL, NULL);
}

static int layout_pair(struct boot_editor *boot_editor, int y,
		struct nc_widget_label *label,
		struct nc_widget_textbox *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	struct nc_widget *field_w = widget_textbox_base(field);
	widget_move(label_w, y, boot_editor->label_x);
	widget_move(field_w, y, boot_editor->field_x);
	return max(widget_height(label_w), widget_height(field_w));
}

static void boot_editor_layout_widgets(struct boot_editor *boot_editor)
{
	int y = 1;

	y += layout_pair(boot_editor, y, boot_editor->widgets.image_l,
					 boot_editor->widgets.image_f);

	y += layout_pair(boot_editor, y, boot_editor->widgets.initrd_l,
					 boot_editor->widgets.initrd_f);

	y += layout_pair(boot_editor, y, boot_editor->widgets.dtb_l,
					 boot_editor->widgets.dtb_f);

	y += layout_pair(boot_editor, y, boot_editor->widgets.args_l,
					 boot_editor->widgets.args_f);


	y++;
	widget_move(widget_button_base(boot_editor->widgets.ok_b), y, 9);
	widget_move(widget_button_base(boot_editor->widgets.cancel_b), y, 19);
}

struct boot_editor *boot_editor_init(struct cui *cui,
		struct pmenu_item *item,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *cui,
				struct pmenu_item *item,
				struct pb_boot_data *bd))
{
	char *image, *initrd, *dtb, *args;
	struct boot_editor *boot_editor;
	struct nc_widgetset *set;
	int field_size;

	(void)sysinfo;

	boot_editor = talloc_zero(cui, struct boot_editor);

	if (!boot_editor)
		return NULL;

	talloc_set_destructor(boot_editor, boot_editor_destructor);
	boot_editor->cui = cui;
	boot_editor->item = item;
	boot_editor->on_exit = on_exit;

	boot_editor->label_x = 1;
	boot_editor->field_x = 9;

	nc_scr_init(&boot_editor->scr, pb_boot_editor_sig, 0,
			cui, boot_editor_process_key,
		boot_editor_post, boot_editor_unpost, boot_editor_resize);

	boot_editor->scr.frame.ltitle = talloc_strdup(boot_editor,
			"Petitboot Option Editor");
	boot_editor->scr.frame.rtitle = NULL;
	boot_editor->scr.frame.help = talloc_strdup(boot_editor,
			"Enter=accept");

	if (item) {
		struct pb_boot_data *bd = cod_from_item(item)->bd;
		image = bd->image;
		initrd = bd->initrd;
		dtb = bd->dtb;
		args = bd->args;
	} else {
		image = initrd = dtb = args = "";
	}

	field_size = COLS - 1 - boot_editor->field_x;

	boot_editor->widgetset = set = widgetset_create(boot_editor,
			boot_editor->scr.main_ncw,
			boot_editor->scr.sub_ncw);

	boot_editor->widgets.image_l = widget_new_label(set, 0, 0, "image:");
	boot_editor->widgets.image_f = widget_new_textbox(set, 0, 0,
						field_size, image);

	boot_editor->widgets.initrd_l = widget_new_label(set, 0, 0, "initrd:");
	boot_editor->widgets.initrd_f = widget_new_textbox(set, 0, 0,
						field_size, initrd);

	boot_editor->widgets.dtb_l = widget_new_label(set, 0, 0, "dtb:");
	boot_editor->widgets.dtb_f = widget_new_textbox(set, 0, 0,
						field_size, dtb);

	boot_editor->widgets.args_l = widget_new_label(set, 0, 0, "args:");
	boot_editor->widgets.args_f = widget_new_textbox(set, 0, 0,
					field_size, args);

	boot_editor->widgets.ok_b = widget_new_button(set, 0, 0, 6,
					"OK", ok_click, boot_editor);
	boot_editor->widgets.cancel_b = widget_new_button(set, 0, 0, 6,
					"Cancel", cancel_click, boot_editor);

	boot_editor_layout_widgets(boot_editor);

	return boot_editor;
}
