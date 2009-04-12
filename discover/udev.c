
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

#include "event.h"
#include "udev.h"
#include "pb-discover.h"
#include "device-handler.h"

#define PBOOT_DEVICE_SOCKET "/tmp/petitboot.udev"

#define max(a, b) ((a) > (b) ? (a) : (b))

struct udev {
	struct device_handler *handler;
	int socket;
};

static void udev_print_event(struct event *event)
{
	const char *action, *params[] = {
		"DEVNAME", "ID_TYPE", "ID_BUS", "ID_FS_UUID", "ID_FS_LABEL",
		NULL,
	};
	int i;

	action = event->action == EVENT_ACTION_ADD ? "add" : "remove";

	pb_log("udev %s event:\n", action);
	pb_log("\tdevice: %s\n", event->device);

	for (i = 0; params[i]; i++)
		pb_log("\t%-12s => %s\n",
				params[i], event_get_param(event, params[i]));

}

static void udev_handle_message(struct udev *udev, char *buf, int len)
{
	int result;
	struct event *event;

	event = talloc(udev, struct event);
	event->type = EVENT_TYPE_UDEV;

	result = event_parse_ad_message(event, buf, len);

	if (result)
		return;

	udev_print_event(event);
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

	udev_handle_message(udev, buf, len);

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
