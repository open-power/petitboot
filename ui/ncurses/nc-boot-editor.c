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

/**
 * boot_editor_move_cursor - Move the cursor, setting correct attributes.
 * @req: An ncurses request or char to send to form_driver().
 */

static int boot_editor_move_cursor(struct boot_editor *boot_editor, int req)
{
	int result;

	wchgat(boot_editor->scr.sub_ncw, 1,
			boot_editor_attr_field_selected, 0, 0);
	result = form_driver(boot_editor->ncf, req);
	wchgat(boot_editor->scr.sub_ncw, 1, boot_editor->attr_cursor, 0, 0);
	wrefresh(boot_editor->scr.main_ncw);
	return result;
}

/**
 * boot_editor_insert_mode_set - Set the insert mode.
 */

static void boot_editor_insert_mode_set(struct boot_editor *boot_editor,
		int req)
{
	switch (req) {
	case REQ_INS_MODE:
		boot_editor->attr_cursor = boot_editor_attr_cursor_ins;
		break;
	case REQ_OVL_MODE:
		boot_editor->attr_cursor = boot_editor_attr_cursor_ovl;
		break;
	default:
		assert(0 && "bad req");
		break;
	}
	boot_editor_move_cursor(boot_editor, req);
}

/**
 * boot_editor_insert_mode_tog - Toggle the insert mode.
 */

static void boot_editor_insert_mode_tog(struct boot_editor *boot_editor)
{
	if (boot_editor->attr_cursor == boot_editor_attr_cursor_ins)
		boot_editor_insert_mode_set(boot_editor, REQ_OVL_MODE);
	else
		boot_editor_insert_mode_set(boot_editor, REQ_INS_MODE);
}

/**
 * boot_editor_move_field - Move selected field, setting correct attributes.
 * @req: An ncurses request to send to form_driver().
 */

static int boot_editor_move_field(struct boot_editor *boot_editor, int req)
{
	int result;

	set_field_back(current_field(boot_editor->ncf),
			boot_editor_attr_field_normal);

	result = form_driver(boot_editor->ncf, req);

	set_field_back(current_field(boot_editor->ncf),
			boot_editor_attr_field_selected);

	boot_editor_move_cursor(boot_editor, REQ_END_FIELD);
	return result;
}

static int boot_editor_post(struct nc_scr *scr)
{
	struct boot_editor *boot_editor = boot_editor_from_scr(scr);

	post_form(boot_editor->ncf);

	nc_scr_frame_draw(scr);
	boot_editor_move_field(boot_editor, REQ_FIRST_FIELD);
	boot_editor_move_field(boot_editor, REQ_END_FIELD);
	boot_editor_insert_mode_set(boot_editor, REQ_INS_MODE);

	redrawwin(boot_editor->scr.main_ncw);
	wrefresh(boot_editor->scr.main_ncw);

	return 0;
}

static int boot_editor_unpost(struct nc_scr *scr)
{
	return unpost_form(boot_editor_from_scr(scr)->ncf);
}

static void boot_editor_resize(struct nc_scr *scr)
{
	/* FIXME: forms can't be resized, need to recreate here */
	boot_editor_unpost(scr);
	boot_editor_post(scr);
}

/**
 * boot_editor_chomp - Eat leading and trailing WS.
 */

static char *boot_editor_chomp(char *s)
{
	char *start;
	char *end;
	char *const s_end = s + strlen(s);

	for (; s < s_end; s++)
		if (*s != ' ' && *s != '\t')
			break;

	start = end = s;

	for (; s < s_end; s++)
		if (*s != ' ' && *s != '\t')
			end = s;
	*(end + 1) = 0;
	return start;
}

static struct pb_boot_data *boot_editor_prepare_data(
		struct boot_editor *boot_editor)
{
	struct pb_boot_data *bd;
	char *s;

	bd = talloc(boot_editor, struct pb_boot_data);

	if (!bd)
		return NULL;

	s = boot_editor_chomp(field_buffer(boot_editor->fields[0], 0));
	bd->image = *s ? talloc_strdup(bd, s) : NULL;

	s = boot_editor_chomp(field_buffer(boot_editor->fields[1], 0));
	bd->initrd = *s ? talloc_strdup(bd, s) : NULL;

	s = boot_editor_chomp(field_buffer(boot_editor->fields[2], 0));
	bd->dtb = *s ? talloc_strdup(bd, s) : NULL;

	s = boot_editor_chomp(field_buffer(boot_editor->fields[3], 0));
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
	struct pb_boot_data *bd;

	switch (key) {
	default:
		boot_editor_move_cursor(boot_editor, key);
		break;

	/* hot keys */
	case 27: /* ESC */
		boot_editor->on_exit(boot_editor,
				boot_editor_cancel, NULL);
		nc_flush_keys();
		return;
	case '\n':
	case '\r':
		form_driver(boot_editor->ncf, REQ_VALIDATION);
		bd = boot_editor_prepare_data(boot_editor);
		boot_editor->on_exit(boot_editor,
				boot_editor_update, bd);
		nc_flush_keys();
		return;

	/* insert mode */
	case KEY_IC:
		boot_editor_insert_mode_tog(boot_editor);
		break;

	/* form nav */
	case KEY_PPAGE:
		boot_editor_move_field(boot_editor, REQ_FIRST_FIELD);
		break;
	case KEY_NPAGE:
		boot_editor_move_field(boot_editor, REQ_LAST_FIELD);
		break;
	case KEY_DOWN:
		boot_editor_move_field(boot_editor, REQ_NEXT_FIELD);
		break;
	case KEY_UP:
		boot_editor_move_field(boot_editor, REQ_PREV_FIELD);
		break;

	/* field nav */
	case KEY_HOME:
		boot_editor_move_cursor(boot_editor, REQ_BEG_FIELD);
		break;
	case KEY_END:
		boot_editor_move_cursor(boot_editor, REQ_END_FIELD);
		break;
	case KEY_LEFT:
		boot_editor_move_cursor(boot_editor, REQ_LEFT_CHAR);
		break;
	case KEY_RIGHT:
		boot_editor_move_cursor(boot_editor, REQ_RIGHT_CHAR);
		break;
	case KEY_BACKSPACE:
		if (boot_editor_move_cursor(boot_editor, REQ_LEFT_CHAR)
				== E_OK)
			boot_editor_move_cursor(boot_editor,
					REQ_DEL_CHAR);
		break;
	case KEY_DC:
		boot_editor_move_cursor(boot_editor, REQ_DEL_CHAR);
		break;
	}
}

/**
 * boot_editor_destructor - The talloc destructor for a boot_editor.
 */

static int boot_editor_destructor(void *arg)
{
	struct boot_editor *boot_editor = boot_editor_from_arg(arg);
	FIELD **f;

	for (f = boot_editor->fields; *f; f++)
		free_field(*f);

	free_form(boot_editor->ncf);
	boot_editor->scr.sig = pb_removed_sig;

	return 0;
}

static FIELD *boot_editor_setup_field(unsigned int y, unsigned int x, char *str)
{
	FIELD *f;

	f = new_field(1, COLS - 1 - x, y, x, 0, 0);
	field_opts_off(f, O_STATIC | O_WRAP);
	set_max_field(f, 256);
	set_field_buffer(f, 0, str);
	set_field_status(f, 0);
	return f;
}

static FIELD *boot_editor_setup_label(unsigned int y, unsigned int x, char *str)
{
	FIELD *f;

	f = new_field(1, strlen(str), y, x, 0, 0);
	field_opts_off(f, O_ACTIVE);
	set_field_buffer(f, 0, str);
	return f;
}

struct boot_editor *boot_editor_init(void *ui_ctx,
		const struct pb_boot_data *bd,
		void (*on_exit)(struct boot_editor *,
				enum boot_editor_result,
				struct pb_boot_data *))
{
	struct boot_editor *boot_editor;

	pb_log("%s: image:  '%s'\n", __func__, bd->image);
	pb_log("%s: initrd: '%s'\n", __func__, bd->initrd);
	pb_log("%s: dtb:    '%s'\n", __func__, bd->dtb);
	pb_log("%s: args:   '%s'\n", __func__, bd->args);

	assert(on_exit);

	boot_editor = talloc_zero(ui_ctx, struct boot_editor);

	if (!boot_editor)
		return NULL;

	talloc_set_destructor(boot_editor, boot_editor_destructor);

	nc_scr_init(&boot_editor->scr, pb_boot_editor_sig, 0,
			ui_ctx, boot_editor_process_key,
		boot_editor_post, boot_editor_unpost, boot_editor_resize);

	boot_editor->scr.frame.title = talloc_strdup(boot_editor,
			"Petitboot Option Editor");
	boot_editor->scr.frame.help = talloc_strdup(boot_editor,
			"ESC=cancel, Enter=accept");

	boot_editor->on_exit = on_exit;

	boot_editor->fields = talloc_array(boot_editor, FIELD *, 9);

	boot_editor->fields[0] = boot_editor_setup_field(0, 9, bd->image);
	boot_editor->fields[1] = boot_editor_setup_field(1, 9, bd->initrd);
	boot_editor->fields[2] = boot_editor_setup_field(2, 9, bd->dtb);
	boot_editor->fields[3] = boot_editor_setup_field(3, 9, bd->args);
	boot_editor->fields[4] = boot_editor_setup_label(0, 1, "image:");
	boot_editor->fields[5] = boot_editor_setup_label(1, 1, "initrd:");
	boot_editor->fields[6] = boot_editor_setup_label(2, 1, "dtb:");
	boot_editor->fields[7] = boot_editor_setup_label(3, 1, "args:");
	boot_editor->fields[8] = NULL;

	boot_editor->ncf = new_form(boot_editor->fields);

	set_form_win(boot_editor->ncf, boot_editor->scr.main_ncw);
	set_form_sub(boot_editor->ncf, boot_editor->scr.sub_ncw);

	return boot_editor;
}
