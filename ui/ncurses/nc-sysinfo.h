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

#ifndef _NC_SYSINFO_H
#define _NC_SYSINFO_H

#include "types/types.h"
#include "nc-cui.h"

struct sysinfo_screen;

struct sysinfo_screen *sysinfo_screen_init(struct cui *cui,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *));

struct nc_scr *sysinfo_screen_scr(struct sysinfo_screen *screen);
void sysinfo_screen_update(struct sysinfo_screen *screen,
		const struct system_info *sysinfo);

#endif /* defined _NC_SYSINFO_H */
