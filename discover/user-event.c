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
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <log/log.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>

#include "device-handler.h"
#include "event.h"
#include "user-event.h"


struct user_event {
	struct device_handler *handler;
	int socket;
};

static void user_event_print_event(struct event __attribute__((unused)) *event)
{
	const char *action, *params[] = {
		"name", "image", "args",
		NULL,
	};
	int i;

	action = event->action == EVENT_ACTION_ADD ? "add" : "remove";

	pb_log("user_event %s event:\n", action);
	pb_log("\tdevice: %s\n", event->device);

	for (i = 0; params[i]; i++)
		pb_log("\t%-12s => %s\n",
				params[i], event_get_param(event, params[i]));
}

static void user_event_handle_message(struct user_event *uev, char *buf,
	int len)
{
	int result;
	struct event *event;

	event = talloc(uev, struct event);
	event->type = EVENT_TYPE_USER;

	result = event_parse_ad_message(event, buf, len);

	if (result)
		return;

	user_event_print_event(event);
	device_handler_event(uev->handler, event);
	talloc_free(event);

	return;
}

static int user_event_process(void *arg)
{
	struct user_event *uev = arg;
	char buf[PBOOT_USER_EVENT_SIZE];
	int len;

	len = recvfrom(uev->socket, buf, sizeof(buf), 0, NULL, NULL);

	if (len < 0) {
		pb_log("%s: socket read failed: %s", __func__, strerror(errno));
		return 0;
	}

	if (len == 0) {
		pb_log("%s: empty", __func__);
		return 0;
	}

	pb_log("%s: %u bytes\n", __func__, len);

	user_event_handle_message(uev, buf, len);

	return 0;
}

static int user_event_destructor(void *arg)
{
	struct user_event *uev = arg;

	pb_log("%s\n", __func__);

	if (uev->socket >= 0)
		close(uev->socket);

	return 0;
}

struct user_event *user_event_init(struct waitset *waitset,
		struct device_handler *handler)
{
	struct sockaddr_un addr;
	struct user_event *uev;

	unlink(PBOOT_USER_EVENT_SOCKET);

	uev = talloc(NULL, struct user_event);

	uev->handler = handler;

	uev->socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (uev->socket < 0) {
		pb_log("%s: Error creating event handler socket: %s\n",
			__func__, strerror(errno));
		goto out_err;
	}

	talloc_set_destructor(uev, user_event_destructor);

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_USER_EVENT_SOCKET);

	if (bind(uev->socket, (struct sockaddr *)&addr, sizeof(addr))) {
		pb_log("Error binding event handler socket: %s\n",
			strerror(errno));
	}

	waiter_register(waitset, uev->socket, WAIT_IN, user_event_process, uev);

	pb_log("%s: waiting on %s\n", __func__, PBOOT_USER_EVENT_SOCKET);

	return uev;

out_err:
	talloc_free(uev);
	return NULL;
}

/**
 * user_event_trigger - Trigger known user events
 *
 * SIGUSR1 causes udhcpc to renew the current lease or obtain a new lease.
 */

void user_event_trigger(struct user_event __attribute__((unused)) *uev)
{
	/* FIXME: todo */
}

void user_event_destroy(struct user_event *uev)
{
	talloc_free(uev);
}
