/*
 *  Copyright (C) 2016 IBM Corporation
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

#ifndef _NC_STATUSLOG_H
#define _NC_STATUSLOG_H

#include "nc-cui.h"

struct statuslog;
struct statuslog_screen;

struct statuslog *statuslog_init(struct cui *cui);
void statuslog_append_steal(struct cui *cui, struct statuslog *statuslog,
		struct status *status);

struct statuslog_screen *statuslog_screen_init(struct cui *cui,
		void (*on_exit)(struct cui *));

struct nc_scr *statuslog_screen_scr(struct statuslog_screen *screen);

#endif /* defined _NC_STATUSLOG_H */
