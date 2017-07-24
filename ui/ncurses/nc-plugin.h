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

#ifndef _NC_PLUGIN_H
#define _NC_PLUGIN_H

#include "nc-cui.h"

struct plugin_screen;

struct plugin_screen *plugin_screen_init(struct cui *cui,
		struct pmenu_item *item,
                void (*on_exit)(struct cui *));

struct nc_scr *plugin_screen_scr(struct plugin_screen *screen);
void plugin_screen_update(struct plugin_screen *screen);

int plugin_install_plugin(struct pmenu_item *item);

#endif /* defined _NC_PLUGIN_H */
