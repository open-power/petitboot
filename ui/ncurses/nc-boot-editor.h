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

#include "ui/common/discover-client.h"

#include "types/types.h"
#include "nc-cui.h"

struct boot_editor;

struct boot_editor *boot_editor_init(struct cui *cui,
		struct pmenu_item *item,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *cui,
				struct pmenu_item *item,
				struct pb_boot_data *bd));

struct nc_scr *boot_editor_scr(struct boot_editor *boot_editor);

void boot_editor_update(struct boot_editor *boot_editor,
		const struct system_info *info);


#endif
