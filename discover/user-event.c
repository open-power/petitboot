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
#include <url/url.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>

#include "device-handler.h"
#include "resource.h"
#include "event.h"
#include "user-event.h"


struct user_event {
	struct device_handler *handler;
	int socket;
};

static const char *event_action_name(enum event_action action)
{
	switch (action) {
	case EVENT_ACTION_ADD:
		return "add";
	case EVENT_ACTION_REMOVE:
		return "remove";
	case EVENT_ACTION_CONF:
		return "conf";
	default:
		break;
	}

	return "unknown";
}

static void user_event_print_event(struct event __attribute__((unused)) *event)
{
	int i;

	pb_log("user_event %s event:\n", event_action_name(event->action));
	pb_log("\tdevice: %s\n", event->device);

	for (i = 0; i < event->n_params; i++)
		pb_log("\t%-12s => %s\n",
			event->params[i].name, event->params[i].value);
}

static enum conf_method parse_conf_method(const char *str)
{

	if (!strcasecmp(str, "dhcp")) {
		return CONF_METHOD_DHCP;
	}
	return CONF_METHOD_UNKNOWN;
}

static struct resource *user_event_resource(struct discover_boot_option *opt,
		struct event *event, const char *param_name)
{
	struct resource *res;
	struct pb_url *url;
	const char *val;

	val = event_get_param(event, param_name);
	if (!val)
		return NULL;

	url = pb_url_parse(opt, val);
	if (!url)
		return NULL;

	res = create_url_resource(opt, url);
	if (!res) {
		talloc_free(url);
		return NULL;
	}

	return res;
}

static int parse_user_event(struct discover_context *ctx, struct event *event)
{
	struct discover_boot_option *d_opt;
	struct boot_option *opt;
	struct device *dev;
	const char *p;

	dev = ctx->device->device;

	d_opt = discover_boot_option_create(ctx, ctx->device);
	opt = d_opt->option;

	if (!d_opt)
		goto fail;

	p = event_get_param(event, "name");

	if (!p) {
		pb_log("%s: no name found\n", __func__);
		goto fail;
	}

	opt->id = talloc_asprintf(opt, "%s#%s", dev->id, p);
	opt->name = talloc_strdup(opt, p);

	d_opt->boot_image = user_event_resource(d_opt, event, "image");
	if (!d_opt->boot_image) {
		pb_log("%s: no boot image found for %s!\n", __func__,
				opt->name);
		goto fail;
	}

	d_opt->initrd = user_event_resource(d_opt, event, "initrd");

	p = event_get_param(event, "args");

	if (p)
		opt->boot_args = talloc_strdup(opt, p);

	opt->description = talloc_asprintf(opt, "%s %s", opt->boot_image_file,
		opt->boot_args ? : "");

	if (event_get_param(event, "default"))
		opt->is_default = true;

	discover_context_add_boot_option(ctx, d_opt);

	return 0;

fail:
	talloc_free(d_opt);
	return -1;
}

static int user_event_conf(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_device *dev;
	enum conf_method method;
	struct pb_url *url;
	const char *val;

	val = event_get_param(event, "url");
	if (!val)
		return 0;

	url = pb_url_parse(event, val);
	if (!url)
		return 0;

	val = event_get_param(event, "method");
	if (!val)
		return 0;

	method = parse_conf_method(val);
	if (method == CONF_METHOD_UNKNOWN)
		return 0;

	dev = discover_device_create(handler, event->device);

	device_handler_conf(handler, dev, url, method);

	return 0;
}

static int user_event_add(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_context *ctx;
	struct discover_device *dev;

	dev = discover_device_create(handler, event->device);
	ctx = device_handler_discover_context_create(handler, dev);

	parse_user_event(ctx, event);

	device_handler_discover_context_commit(handler, ctx);

	talloc_free(ctx);

	return 0;
}

static int user_event_remove(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_device *dev;

	dev = device_lookup_by_id(handler, event->device);
	if (!dev)
		return 0;

	device_handler_remove(handler, dev);

	return 0;
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

	switch (event->action) {
	case EVENT_ACTION_ADD:
		result = user_event_add(uev, event);
		break;
	case EVENT_ACTION_REMOVE:
		result = user_event_remove(uev, event);
		break;
	case EVENT_ACTION_CONF:
		result = user_event_conf(uev, event);
		break;
	default:
		break;
	}

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

	waiter_register_io(waitset, uev->socket, WAIT_IN,
			user_event_process, uev);

	pb_log("%s: waiting on %s\n", __func__, PBOOT_USER_EVENT_SOCKET);

	return uev;

out_err:
	talloc_free(uev);
	return NULL;
}

void user_event_destroy(struct user_event *uev)
{
	talloc_free(uev);
}
