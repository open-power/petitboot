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

#if !defined(_PB_JOYSTICK_H)
#define _PB_JOYSTICK_H

#include <linux/joystick.h>

/**
 * struct pjs - Petitboot joystick event handler.
 * @map: Routine to map from a Linux struct js_event to a ui key code.
 */

struct pjs {
	int fd;
	int (*map)(const struct js_event *e);
};

struct pjs *pjs_init(void *ctx, int (*map)(const struct js_event *));
int pjs_process_event(const struct pjs *pjs);

static inline struct pjs *pjs_from_arg(void *arg)
{
	return arg;
}

static inline int pjs_get_fd(const struct pjs *pjs)
{
	return pjs->fd;
}

#endif
