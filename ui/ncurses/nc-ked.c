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

#define _GNU_SOURCE

#include <assert.h>
#include <string.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "nc-ked.h"

static struct ked *ked_from_scr(struct nc_scr *scr)
{
	struct ked *ked;

	assert(scr->sig == pb_ked_sig);
	ked = (struct ked *)((char *)scr - (size_t)&((struct ked *)0)->scr);
	assert(ked->scr.sig == pb_ked_sig);
	return ked;
}

static struct ked *ked_from_arg(void *arg)
{
	struct ked *ked = arg;

	assert(ked->scr.sig == pb_ked_sig);
	return ked;
}

/**
 * ked_move_cursor - Move the cursor, setting correct attributes.
 * @req: An ncurses request or char to send to form_driver().
 */

static int ked_move_cursor(struct ked *ked, int req)
{
	int result;

	wchgat(ked->scr.sub_ncw, 1, ked_attr_field_selected, 0, 0);
	result = form_driver(ked->ncf, req);
	wchgat(ked->scr.sub_ncw, 1, ked->attr_cursor, 0, 0);
	wrefresh(ked->scr.main_ncw);
	return result;
}

/**
 * ked_insert_mode_set - Set the insert mode.
 */

static void ked_insert_mode_set(struct ked *ked, int req)
{
	switch (req) {
	case REQ_INS_MODE:
		ked->attr_cursor = ked_attr_cursor_ins;
		break;
	case REQ_OVL_MODE:
		ked->attr_cursor = ked_attr_cursor_ovl;
		break;
	default:
		assert(0 && "bad req");
		break;
	}
	ked_move_cursor(ked, req);
}

/**
 * ked_insert_mode_tog - Toggle the insert mode.
 */

static void ked_insert_mode_tog(struct ked *ked)
{
	if (ked->attr_cursor == ked_attr_cursor_ins)
		ked_insert_mode_set(ked, REQ_OVL_MODE);
	else
		ked_insert_mode_set(ked, REQ_INS_MODE);
}

/**
 * ked_move_field - Move selected field, setting correct attributes.
 * @req: An ncurses request to send to form_driver().
 */

static int ked_move_field(struct ked *ked, int req)
{
	int result;

	set_field_back(current_field(ked->ncf), ked_attr_field_normal);
	result = form_driver(ked->ncf, req);
	set_field_back(current_field(ked->ncf), ked_attr_field_selected);
	ked_move_cursor(ked, REQ_END_FIELD);
	return result;
}

static int ked_post(struct nc_scr *scr)
{
	struct ked *ked = ked_from_scr(scr);

	post_form(ked->ncf);

	nc_scr_frame_draw(scr);
	ked_move_field(ked, REQ_FIRST_FIELD);
	ked_move_field(ked, REQ_END_FIELD);
	ked_insert_mode_set(ked, REQ_INS_MODE);

	redrawwin(ked->scr.main_ncw);
	wrefresh(ked->scr.main_ncw);

	return 0;
}

static int ked_unpost(struct nc_scr *scr)
{
	return unpost_form(ked_from_scr(scr)->ncf);
}

static void ked_resize(struct nc_scr *scr)
{
	/* FIXME: forms can't be resized, need to recreate here */
	ked_unpost(scr);
	ked_post(scr);
}

/**
 * ked_chomp - Eat leading and trailing WS.
 */

static char *ked_chomp(char *s)
{
	char *start;
	char *end;
	char *const s_end = s + strlen(s);

	for (; s < s_end; s++)
		if (*s != ' ' && *s != '\t')
			break;
	start = s;

	for (++s; s < s_end; s++)
		if (*s != ' ' && *s != '\t')
			end = s;
	*(end + 1) = 0;
	return start;
}

static struct pb_kexec_data *ked_prepare_data(struct ked *ked)
{
	struct pb_kexec_data *kd;
	char *s;

	kd = talloc(ked, struct pb_kexec_data);

	if (!kd)
		return NULL;

	s = ked_chomp(field_buffer(ked->fields[0], 0));
	kd->image = *s ? talloc_strdup(kd, s) : NULL;

	s = ked_chomp(field_buffer(ked->fields[1], 0));
	kd->initrd = *s ? talloc_strdup(kd, s) : NULL;

	s = ked_chomp(field_buffer(ked->fields[2], 0));
	kd->args = *s ? talloc_strdup(kd, s) : NULL;

	return kd;
}

/**
 * ked_process_key - Process a user keystroke.
 *
 * Called from the cui via the scr:process_key method.
 */

static void ked_process_key(struct nc_scr *scr)
{
	struct ked *ked = ked_from_scr(scr);

	while (1) {
		int c = getch();

		if (c == ERR)
			return;

		/* DBGS("%d (%o)\n", c, c); */

		switch (c) {
		default:
			ked_move_cursor(ked, c);
			break;

		/* hot keys */
		case 2: { /* CTRL-B */
			struct pb_kexec_data *kd;

			form_driver(ked->ncf, REQ_VALIDATION);
			kd = ked_prepare_data(ked);
			ked->on_exit(ked, ked_boot, kd);
			nc_flush_keys();
			return;
		}
		case 27: /* ESC */
			ked->on_exit(ked, ked_cancel, NULL);
			nc_flush_keys();
			return;
		case '\n':
		case '\r': {
			struct pb_kexec_data *kd;

			form_driver(ked->ncf, REQ_VALIDATION);
			kd = ked_prepare_data(ked);
			ked->on_exit(ked, ked_update, kd);
			nc_flush_keys();
			return;
		}

		/* insert mode */
		case KEY_IC:
			ked_insert_mode_tog(ked);
			break;

		/* form nav */
		case KEY_PPAGE:
			ked_move_field(ked, REQ_FIRST_FIELD);
			break;
		case KEY_NPAGE:
			ked_move_field(ked, REQ_LAST_FIELD);
			break;
		case KEY_DOWN:
			ked_move_field(ked, REQ_NEXT_FIELD);
			break;
		case KEY_UP:
			ked_move_field(ked, REQ_PREV_FIELD);
			break;

		/* field nav */
		case KEY_HOME:
			ked_move_cursor(ked, REQ_BEG_FIELD);
			break;
		case KEY_END:
			ked_move_cursor(ked, REQ_END_FIELD);
			break;
		case KEY_LEFT:
			ked_move_cursor(ked, REQ_LEFT_CHAR);
			break;
		case KEY_RIGHT:
			ked_move_cursor(ked, REQ_RIGHT_CHAR);
			break;
		case KEY_BACKSPACE:
			if (ked_move_cursor(ked, REQ_LEFT_CHAR) == E_OK)
				ked_move_cursor(ked, REQ_DEL_CHAR);
			break;
		case KEY_DC:
			ked_move_cursor(ked, REQ_DEL_CHAR);
			break;
		}
	}
}

/**
 * ked_destructor - The talloc destructor for a ked.
 */

static int ked_destructor(void *arg)
{
	struct ked *ked = ked_from_arg(arg);
	FIELD **f;

	for (f = ked->fields; *f; f++)
		free_field(*f);

	free_form(ked->ncf);
	ked->scr.sig = pb_removed_sig;

	return 0;
}

static FIELD *ked_setup_field(unsigned int y, unsigned int x, char *str)
{
	FIELD *f;

	f = new_field(1, COLS - 1 - x, y, x, 0, 0);
	field_opts_off(f, O_STATIC | O_WRAP);
	set_max_field(f, 256);
	set_field_buffer(f, 0, str);
	set_field_status(f, 0);
	return f;
}

static FIELD *ked_setup_label(unsigned int y, unsigned int x, char *str)
{
	FIELD *f;

	f = new_field(1, strlen(str), y, x, 0, 0);
	field_opts_off(f, O_ACTIVE);
	set_field_buffer(f, 0, str);
	return f;
}

struct ked *ked_init(void *ui_ctx, const struct pb_kexec_data *kd,
	void (*on_exit)(struct ked *, enum ked_result, struct pb_kexec_data *))
{
	struct ked *ked;

	pb_log("%s: image:  '%s'\n", __func__, kd->image);
	pb_log("%s: initrd: '%s'\n", __func__, kd->initrd);
	pb_log("%s: args:   '%s'\n", __func__, kd->args);

	assert(on_exit);

	ked = talloc_zero(ui_ctx, struct ked);

	if (!ked)
		return NULL;

	talloc_set_destructor(ked, ked_destructor);

	nc_scr_init(&ked->scr, pb_ked_sig, 0, ui_ctx, ked_process_key,
		ked_post, ked_unpost, ked_resize);

	ked->scr.frame.title = talloc_strdup(ked, "Petitboot Option Editor");
	ked->scr.frame.help = talloc_strdup(ked,
		"ESC=cancel, Enter=accept, Ctrl-b=boot");

	ked->on_exit = on_exit;

	ked->fields = talloc_array(ked, FIELD *, 7);

	ked->fields[0] = ked_setup_field(0, 9, kd->image);
	ked->fields[1] = ked_setup_field(1, 9, kd->initrd);
	ked->fields[2] = ked_setup_field(2, 9, kd->args);
	ked->fields[3] = ked_setup_label(0, 1, "image:");
	ked->fields[4] = ked_setup_label(1, 1, "initrd:");
	ked->fields[5] = ked_setup_label(2, 1, "args:");
	ked->fields[6] = NULL;

	ked->ncf = new_form(ked->fields);

	set_form_win(ked->ncf, ked->scr.main_ncw);
	set_form_sub(ked->ncf, ked->scr.sub_ncw);

	return ked;
}
