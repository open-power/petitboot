
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <log/log.h>

#include "udev.h"
#include "pb-discover.h"
#include "device-handler.h"

#define PBOOT_DEVICE_SOCKET "/tmp/petitboot.udev"

#define max(a, b) ((a) > (b) ? (a) : (b))

struct udev {
	struct device_handler *handler;
	int socket;
};

static void parse_event_params(struct udev_event *event, char *buf, int len)
{
	int param_len, name_len, value_len;
	struct param *param;
	char *sep;

	for (; len > 0; len -= param_len + 1, buf += param_len + 1) {

		/* find the length of the whole parameter */
		param_len = strnlen(buf, len);
		if (!param_len) {
			/* multiple NULs? skip over */
			param_len = 1;
			continue;
		}

		/* find the separator */
		sep = memchr(buf, '=', param_len);
		if (!sep)
			continue;

		name_len = sep - buf;
		value_len = param_len - name_len - 1;

		/* update the params array */
		event->params = talloc_realloc(event, event->params,
					struct param, ++event->n_params);
		param = &event->params[event->n_params - 1];

		param->name = talloc_strndup(event, buf, name_len);
		param->value = talloc_strndup(event, sep + 1, value_len);
	}
}

const char *udev_event_param(struct udev_event *event, const char *name)
{
	int i;

	for (i = 0; i < event->n_params; i++)
		if (!strcasecmp(event->params[i].name, name))
			return event->params[i].value;

	return NULL;
}

static void print_event(struct udev_event *event)
{
	const char *action, *params[] = {
		"DEVNAME", "ID_TYPE", "ID_BUS", "ID_FS_UUID", "ID_FS_LABEL",
		NULL,
	};
	int i;

	action = event->action == UDEV_ACTION_ADD ? "add" : "remove";

	pb_log("udev %s event:\n", action);
	pb_log("\tdevice: %s\n", event->device);

	for (i = 0; params[i]; i++)
		pb_log("\t%-12s => %s\n",
				params[i], udev_event_param(event, params[i]));

}

static void handle_udev_message(struct udev *udev, char *buf, int len)
{
	char *sep, *device;
	enum udev_action action;
	struct udev_event *event;
	int device_len;

	/* we should see an <action>@<device>\0 at the head of the buffer */
	sep = strchr(buf, '@');
	if (!sep)
		return;

	/* terminate the action string */
	*sep = '\0';
	len -= sep - buf + 1;

	if (!strcmp(buf, "add")) {
		action = UDEV_ACTION_ADD;

	} else if (!strcmp(buf, "remove")) {
		action = UDEV_ACTION_REMOVE;

	} else {
		return;
	}

	/* initialise the device string */
	device = sep + 1;
	device_len = strnlen(device, len);
	if (!device_len)
		return;

	/* now we have an action and a device, we can construct an event */
	event = talloc(udev, struct udev_event);
	event->action = action;
	event->device = talloc_strndup(event, device, device_len);
	event->n_params = 0;
	event->params = NULL;

	len -= device_len + 1;
	parse_event_params(event, device + device_len + 1, len);

	print_event(event);

	device_handler_event(udev->handler, event);

	talloc_free(event);

	return;
}

static int udev_process(void *arg)
{
	struct udev *udev = arg;
	char buf[4096];
	int len;

	len = recvfrom(udev->socket, buf, sizeof(buf), 0, NULL, NULL);

	if (len < 0) {
		pb_log("udev socket read failed: %s", strerror(errno));
		return -1;
	}

	if (len == 0)
		return 0;

	handle_udev_message(udev, buf, len);

	return 0;
}

static int udev_destructor(void *p)
{
	struct udev *udev = p;

	if (udev->socket >= 0)
		close(udev->socket);

	return 0;
}

struct udev *udev_init(struct device_handler *handler)
{
	struct sockaddr_un addr;
	struct udev *udev;

	unlink(PBOOT_DEVICE_SOCKET);

	udev = talloc(NULL, struct udev);

	udev->handler = handler;

	udev->socket = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (udev->socket < 0) {
		pb_log("Error creating udev socket: %s\n", strerror(errno));
		goto out_err;
	}

	talloc_set_destructor(udev, udev_destructor);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_DEVICE_SOCKET);

	if (bind(udev->socket, (struct sockaddr *)&addr, sizeof(addr))) {
		pb_log("Error binding udev socket: %s\n", strerror(errno));
		goto out_err;
	}

	waiter_register(udev->socket, WAIT_IN, udev_process, udev);

	pb_log("%s: waiting on %s\n", __func__, PBOOT_DEVICE_SOCKET);

	return udev;

out_err:
	talloc_free(udev);
	return NULL;
}

int udev_trigger(struct udev __attribute__((unused)) *udev)
{
	int rc = system("/sbin/udevadm trigger --subsystem-match=block");

	if (rc)
		pb_log("udev trigger failed: %d (%d)\n", rc, WEXITSTATUS(rc));

	return WEXITSTATUS(rc);
}

void udev_destroy(struct udev *udev)
{
	talloc_free(udev);
}
