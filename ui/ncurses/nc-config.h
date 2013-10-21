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

#ifndef _NC_CONFIG_H
#define _NC_CONFIG_H

#include "types/types.h"
#include "nc-cui.h"

struct config_screen;

struct config_screen *config_screen_init(struct cui *cui,
		const struct config *config,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *));

struct nc_scr *config_screen_scr(struct config_screen *screen);
void config_screen_update(struct config_screen *screen,
		const struct config *config);

#endif /* defined _NC_CONFIG_H */
