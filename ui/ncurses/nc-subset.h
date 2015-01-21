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

#ifndef _NC_SUBSET_H
#define _NC_SUBSET_H

#include "nc-cui.h"
#include "nc-widgets.h"

struct subset_screen;

struct subset_screen *subset_screen_init(struct cui *cui,
		struct nc_scr *current_scr,
		const char *title_suffix,
		void *subset,
		void (*on_exit)(struct cui *));

struct nc_scr *subset_screen_scr(struct subset_screen *screen);
struct nc_scr *subset_screen_return_scr(struct subset_screen *screen);
void subset_screen_update(struct subset_screen *screen);

#endif /* defined _NC_SUBSET_H */
