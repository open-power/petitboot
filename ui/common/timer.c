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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include "log/log.h"
#include "timer.h"

/**
 * ui_timer_init - Initialize the timer for use.
 * @seconds: The final timeout value in seconds.
 */

void ui_timer_init(struct waitset *waitset, struct ui_timer *timer,
		unsigned int seconds)
{
	pb_log_fn("%u\n", seconds);
	timer->timeout = seconds;
	timer->waitset = waitset;
}

/**
 * ui_timer_kick - Kickstart the next timer interval.
 */

static int timer_cb(void *arg)
{
	struct ui_timer *timer = arg;

	timer->handle_timeout(timer);
	timer->waiter = NULL;
	return 0;
}

void ui_timer_kick(struct ui_timer *timer)
{
	if (timer->update_display)
		timer->update_display(timer, timer->timeout);

	if (timer->waiter)
		waiter_remove(timer->waiter);

	timer->waiter = waiter_register_timeout(timer->waitset,
			timer->timeout * 1000, timer_cb, timer);
}

/**
 * ui_timer_disable - Stop and disable the timer.
 */

void ui_timer_disable(struct ui_timer *timer)
{
	if (!timer->waiter)
		return;

	pb_log("%s\n", __func__);
	waiter_remove(timer->waiter);
	timer->waiter = NULL;
}
