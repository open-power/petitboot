/*
 *  Copyright (C) 2018 IBM Corporation
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

#ifndef _NC_AUTH_H
#define _NC_AUTH_H

#include "nc-cui.h"

struct auth_screen;

struct auth_screen *auth_screen_init(struct cui *cui,
		WINDOW *pad, bool set_password,
		const struct device *dev,
		void (callback)(struct nc_scr *),
		void (*on_exit)(struct cui *));

struct nc_scr *auth_screen_scr(struct auth_screen *screen);
struct nc_scr *auth_screen_return_scr(struct auth_screen *screen);

#endif /* define _NC_AUTH_H */
