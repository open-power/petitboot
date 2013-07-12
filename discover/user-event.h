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

#if !defined(_PB_DISCOVER_USER_EVENT_H)
#define _PB_DISCOVER_USER_EVENT_H

#include "device-handler.h"

#define PBOOT_USER_EVENT_SOCKET "/tmp/petitboot.ev"
#define PBOOT_USER_EVENT_SIZE (1 * 1024)

struct user_event;
struct waitset;

struct user_event *user_event_init(struct waitset *waitset,
		struct device_handler *handler);
void user_event_destroy(struct user_event *uev);

#endif
