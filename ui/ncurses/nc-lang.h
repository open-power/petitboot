/*
 *  Copyright (C) 2014 IBM Corporation
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

#ifndef _NC_LANG_H
#define _NC_LANG_H

#include "types/types.h"
#include "nc-cui.h"

struct lang_screen;

struct lang_screen *lang_screen_init(struct cui *cui,
		const struct config *config,
		void (*on_exit)(struct cui *));

struct nc_scr *lang_screen_scr(struct lang_screen *screen);
void lang_screen_update(struct lang_screen *screen,
		const struct config *config);

#endif /* defined _NC_LANG_H */
