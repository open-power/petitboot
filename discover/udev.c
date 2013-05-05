
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <log/log.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <system/system.h>

#include "event.h"
#include "udev.h"
#include "pb-discover.h"
#include "device-handler.h"

#if defined(DEBUG)
#define DBG(fmt, args...) pb_log("DBG: " fmt, ## args)
#define DBGS(fmt, args...) \
	pb_log("DBG:%s:%d: " fmt, __func__, __LINE__, ## args)
#else
#define DBG(fmt, args...)
#define DBGS(fmt, args...)
#endif

struct pb_udev {
	struct udev *udev;
	struct udev_monitor *monitor;
	struct device_handler *handler;
};

static int udev_destructor(void *p)
{
	struct pb_udev *udev = p;

	udev_monitor_unref(udev->monitor);
	udev->monitor = NULL;

	udev_unref(udev->udev);
	udev->udev = NULL;

	return 0;
}

static void print_device_properties(struct udev_device *dev)
{
	struct udev_list_entry *list, *entry;

	assert(dev);

	if (1) {
		list = udev_device_get_properties_list_entry(dev);

		assert(list);

		udev_list_entry_foreach(entry, list)
			DBG("property: %s - %s\n",
				udev_list_entry_get_name(entry),
				udev_device_get_property_value(dev,
					udev_list_entry_get_name(entry)));
	}
}

static int udev_handle_dev_action(struct udev_device *dev, const char *action)
{
	const char *devtype;
	const char *devpath;
	const char *devnode;
	struct pb_udev *udev;
	struct event *event;
	enum event_action eva = 0;

	assert(dev);
	assert(action);

	devtype = udev_device_get_devtype(dev); /* DEVTYPE */

	if (!devtype) {
		pb_log("udev_device_get_devtype failed\n");
		return -1;
	}

	devpath = udev_device_get_devpath(dev); /* DEVPATH */

	if (!devpath) {
		pb_log("udev_device_get_devpath failed\n");
		return -1;
	}

	devnode = udev_device_get_devnode(dev); /* DEVNAME */

	if (!devnode) {
		pb_log("udev_device_get_devnode failed\n");
		return -1;
	}

	print_device_properties(dev);

	/* Ignore non disk or partition, ram, loop. */

	if (!(strstr(devtype, "disk") || strstr(devtype, "partition"))
		|| strstr(devpath, "virtual/block/loop")
		|| strstr(devpath, "virtual/block/ram")) {
		pb_log("SKIP: %s - %s\n", devtype, devnode);
		return 0;
	}

	if (!strcmp(action, "add")) {
		pb_log("ADD: %s - %s\n", devtype, devnode);
		eva = EVENT_ACTION_ADD;
	} else if (!strcmp(action, "remove")) {
		pb_log("REMOVE: %s - %s\n", devtype, devnode);
		eva = EVENT_ACTION_REMOVE;
	} else {
		pb_log("SKIP: %s: %s - %s\n", action, devtype, devnode);
		return 0;
	}

	event = talloc(NULL, struct event);

	event->type = EVENT_TYPE_UDEV;
	event->action = eva;
	event->device = devpath;

	event->n_params = 1;
	event->params = talloc(event, struct param);
	event->params->name = "DEVNAME";
	event->params->value = devnode;

	udev = udev_get_userdata(udev_device_get_udev(dev));
	assert(udev);

	device_handler_event(udev->handler, event);

	talloc_free(event);
	return 0;
}

static int udev_enumerate(struct udev *udev)
{
	int result;
	struct udev_list_entry *list, *entry;
	struct udev_enumerate *enumerate;

	enumerate = udev_enumerate_new(udev);

	if (!enumerate) {
		pb_log("udev_enumerate_new failed\n");
		return -1;
	}

	result = udev_enumerate_add_match_subsystem(enumerate, "block");

	if (result) {
		pb_log("udev_enumerate_add_match_subsystem failed\n");
		goto fail;
	}

	udev_enumerate_scan_devices(enumerate);

	list = udev_enumerate_get_list_entry(enumerate);

	if (!list) {
		pb_log("udev_enumerate_get_list_entry failed\n");
		goto fail;
	}

	udev_list_entry_foreach(entry, list) {
		const char *syspath;
		struct udev_device *dev;

		syspath = udev_list_entry_get_name(entry);
		dev = udev_device_new_from_syspath(udev, syspath);

		udev_handle_dev_action(dev, "add");

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	return 0;

fail:
	udev_enumerate_unref(enumerate);
	return -1;
}

static int udev_setup_monitor(struct udev *udev, struct udev_monitor **monitor)
{
	int result;
	struct udev_monitor *m;

	*monitor = NULL;
	m = udev_monitor_new_from_netlink(udev, "udev");

	if (!m) {
		pb_log("udev_monitor_new_from_netlink failed\n");
		goto out_err;
	}

	result = udev_monitor_filter_add_match_subsystem_devtype(m, "block",
		NULL);

	if (result) {
		pb_log("udev_monitor_filter_add_match_subsystem_devtype failed\n");
		goto out_err;
	}

	result = udev_monitor_enable_receiving(m);

	if (result) {
		pb_log("udev_monitor_enable_receiving failed\n");
		goto out_err;
	}

	*monitor = m;
	return 0;

out_err:
	udev_monitor_unref(m);
	return -1;
}

/*
 * udev_process - waiter callback for monitor netlink.
 */

static int udev_process(void *arg)
{
	struct udev_monitor *monitor = arg;
	struct udev_device *dev;
	const char *action;
	int result;

	dev = udev_monitor_receive_device(monitor);

	if (!dev) {
		pb_log("udev_monitor_receive_device failed\n");
		return -1;
	}

	action = udev_device_get_action(dev);

	if (!action) {
		pb_log("udev_device_get_action failed\n");
		goto fail;
	}

	result = udev_handle_dev_action(dev, action);

	udev_device_unref(dev);
	return result;

fail:
	udev_device_unref(dev);
	return -1;
}

static void udev_log_fn(struct udev __attribute__((unused)) *udev,
	int __attribute__((unused)) priority, const char *file, int line,
	const char *fn, const char *format, va_list args)
{
      pb_log("libudev: %s %s:%d: ", fn, file, line);
      vfprintf(pb_log_get_stream(), format, args);
}

struct pb_udev *udev_init(struct waitset *waitset,
	struct device_handler *handler)
{
	int result;
	struct pb_udev *udev = talloc(NULL, struct pb_udev);

	talloc_set_destructor(udev, udev_destructor);
	udev->handler = handler;

	udev->udev = udev_new();

	if (!udev->udev) {
		pb_log("udev_new failed\n");
		goto fail_new;
	}

	udev_set_userdata(udev->udev, udev);

	udev_set_log_fn(udev->udev, udev_log_fn);

	result = udev_enumerate(udev->udev);

	if (result)
		goto fail_enumerate;

	result = udev_setup_monitor(udev->udev, &udev->monitor);

	if (result)
		goto fail_monitor;

	waiter_register(waitset, udev_monitor_get_fd(udev->monitor), WAIT_IN,
		udev_process, udev->monitor);

	pb_log("%s: waiting on udev\n", __func__);

	return udev;

fail_monitor:
fail_enumerate:
	udev_unref(udev->udev);
fail_new:
	talloc_free(udev);
	return NULL;
}

void udev_destroy(struct pb_udev *udev)
{
	talloc_free(udev);
}
