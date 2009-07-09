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

#define _GNU_SOURCE
#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include "log/log.h"
#include "timer.h"

/**
 * ui_timer_init - Initialize the timer for use.
 * @seconds: The final timeout value in seconds.
 */

void ui_timer_init(struct ui_timer *timer, unsigned int seconds)
{
	pb_log("%s: %u\n", __func__, seconds);
	assert(!timer->disabled);
	timer->timeout = seconds;
}

/**
 * ui_timer_next - Calculate the next timer interval.
 */

static unsigned int ui_timer_next(unsigned int seconds)
{
	unsigned int next;

	if (seconds == 0) {
		next = 0;
		goto done;
	}

	if (seconds <= 10) {
		next = 1;
		goto done;
	}

	if (seconds <= 60) {
		next = seconds % 5;
		next = next ? next : 5;
		goto done;
	}

	next = seconds % 10;
	next = next ? next : 10;

done:
	pb_log("%s: %u = %u\n", __func__, seconds, next);
	return next;
}

/**
 * ui_timer_kick - Kickstart the next timer interval.
 */

void ui_timer_kick(struct ui_timer *timer)
{
	unsigned int next;

	if(timer->disabled)
		return;

	if (timer->update_display)
		timer->update_display(timer, timer->timeout);

	next = ui_timer_next(timer->timeout);
	timer->timeout -= next;

	if (next) {
		alarm(next);
		return;
	}

	pb_log("%s: timed out\n", __func__);

	ui_timer_disable(timer);
	timer->handle_timeout(timer);
}

/**
 * ui_timer_disable - Stop and disable the timer.
 */

void ui_timer_disable(struct ui_timer *timer)
{
	if (timer->disabled)
		return;

	pb_log("%s\n", __func__);
	timer->disabled = 1;
	timer->timeout = UINT_MAX;
	alarm(0);
}

/**
 * ui_timer_sigalrm
 *
 * Called at SIGALRM.
 */

void ui_timer_sigalrm(struct ui_timer *timer)
{
	timer->signaled = 1;
}

/**
 * ui_timer_process_sig - Process a timer signal
 */

void ui_timer_process_sig(struct ui_timer *timer)
{
	while (timer->signaled) {
		timer->signaled = 0;
		ui_timer_kick(timer);
	}
}
