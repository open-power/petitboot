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

#include <assert.h>
#include <form.h>

#include "pb-protocol/pb-protocol.h"
#include "ui/common/ui-system.h"
#include "nc-scr.h"

enum ked_attr_field {
	ked_attr_field_normal = A_NORMAL,
	ked_attr_field_selected = A_REVERSE,
};

enum ked_attr_cursor {
	ked_attr_cursor_ins = A_NORMAL,
	ked_attr_cursor_ovl = A_NORMAL | A_UNDERLINE,
};

/**
 * enum ked_result - Result code for ked:on_exit().
 * @ked_cancel: The user canceled the operation.
 * @ked_update: The args were updated.
 * @ked_boot: The user requested a boot of this item.
 */

enum ked_result {
	ked_cancel,
	ked_update,
	ked_boot,
};

/**
 * struct ked - kexec args editor.
 */

struct ked {
	struct nc_scr scr;
	FORM *ncf;
	FIELD **fields;
	enum ked_attr_cursor attr_cursor;
	void (*on_exit)(struct ked *ked, enum ked_result result,
		struct pb_kexec_data *kd);
};

struct ked *ked_init(void *ui_ctx, const struct pb_kexec_data *kd,
	void (*on_exit)(struct ked *, enum ked_result, struct pb_kexec_data *));

#endif
