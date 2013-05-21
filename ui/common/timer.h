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

#if !defined(_PB_UI_TIMER_H)
#define _PB_UI_TIMER_H

#include <waiter/waiter.h>

/**
 * struct ui_timer - UI timeout.
 */

struct ui_timer {
	unsigned int timeout;
	struct waiter *waiter;
	struct waitset *waitset;
	void (*update_display)(struct ui_timer *timer, unsigned int timeout);
	void (*handle_timeout)(struct ui_timer *timer);
};

void ui_timer_init(struct waitset *set, struct ui_timer *timer,
		unsigned int seconds);
void ui_timer_kick(struct ui_timer *timer);
void ui_timer_disable(struct ui_timer *timer);

#endif
