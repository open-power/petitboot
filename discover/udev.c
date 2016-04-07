
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

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
#include <types/types.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <system/system.h>

#include "event.h"
#include "udev.h"
#include "pb-discover.h"
#include "device-handler.h"
#include "cdrom.h"
#include "devmapper.h"

/* We set a default monitor buffer size, as we may not process monitor
 * events while performing device discvoery. systemd uses a 128M buffer, so
 * we'll do the same here */
static const int monitor_bufsize = 128 * 1024 * 1024;

struct pb_udev {
	struct udev *udev;
	struct udev_monitor *monitor;
	struct device_handler *handler;
};

static int udev_destructor(void *p)
{
	struct pb_udev *udev = p;

	if (udev->monitor) {
		udev_monitor_unref(udev->monitor);
		udev->monitor = NULL;
	}

	if (udev->udev) {
		udev_unref(udev->udev);
		udev->udev = NULL;
	}

	return 0;
}

static void udev_setup_device_params(struct udev_device *udev,
		struct discover_device *dev)
{
	struct udev_list_entry *list, *entry;

	list = udev_device_get_properties_list_entry(udev);
	if (!list)
		return;

	udev_list_entry_foreach(entry, list)
		discover_device_set_param(dev,
				udev_list_entry_get_name(entry),
				udev_list_entry_get_value(entry));
}

static int udev_handle_block_add(struct pb_udev *udev, struct udev_device *dev,
		const char *name)
{
	struct discover_device *ddev;
	unsigned int i = 0;
	const char *typestr;
	const char *uuid;
	const char *path;
	const char *node;
	const char *prop;
	const char *type;
	const char *devname;
	const char *ignored_types[] = {
		"linux_raid_member",
		"swap",
		"LVM2_member",
		NULL,
	};
	bool cdrom, usb;

	typestr = udev_device_get_devtype(dev);
	if (!typestr) {
		pb_debug("udev_device_get_devtype failed\n");
		return -1;
	}

	if (!(!strcmp(typestr, "disk") || !strcmp(typestr, "partition"))) {
		pb_log("SKIP %s: invalid type %s\n", name, typestr);
		return 0;
	}

	node = udev_device_get_devnode(dev);
	path = udev_device_get_devpath(dev);
	if (path && strstr(path, "virtual/block/loop")) {
		pb_log("SKIP: %s: ignored (path=%s)\n", name, path);
		return 0;
	}

	if (path && strstr(path, "virtual/block/ram")) {
		device_handler_add_ramdisk(udev->handler, node);
		return 0;
	}

	cdrom = node && !!udev_device_get_property_value(dev, "ID_CDROM");
	if (cdrom) {
		/* CDROMs require a little initialisation, to get
		 * petitboot-compatible tray behaviour */
		cdrom_init(node);
		if (!cdrom_media_present(node)) {
			pb_log("SKIP: %s: no media present\n", name);
			return 0;
		}
	}

	/* If our environment's udev can recognise them explictly skip any
	 * device mapper devices we encounter */
	devname = udev_device_get_property_value(dev, "DM_NAME");
	if (devname) {
		pb_debug("SKIP: dm-device %s\n", devname);
		return 0;
	}

	type = udev_device_get_property_value(dev, "ID_FS_TYPE");
	if (!type) {
		pb_log("SKIP: %s: no ID_FS_TYPE property\n", name);
		return 0;
	}

	while (ignored_types[i]) {
		if (!strncmp(type, ignored_types[i], strlen(ignored_types[i]))) {
			pb_log("SKIP: %s: ignore '%s' filesystem\n", name, type);
			return 0;
		}
		i++;
	}

	/* We may see multipath devices; they'll have the same uuid as an
	 * existing device, so only parse the first. */
	uuid = udev_device_get_property_value(dev, "ID_FS_UUID");
	if (uuid) {
		ddev = device_lookup_by_uuid(udev->handler, uuid);
		if (ddev) {
			pb_log("SKIP: %s UUID [%s] already present (as %s)\n",
					name, uuid, ddev->device->id);
			return 0;
		}
	}

	ddev = discover_device_create(udev->handler, name);

	ddev->device_path = talloc_strdup(ddev, node);

	if (uuid)
		ddev->uuid = talloc_strdup(ddev, uuid);
	prop = udev_device_get_property_value(dev, "ID_FS_LABEL");
	if (prop)
		ddev->label = talloc_strdup(ddev, prop);

	usb = !!udev_device_get_property_value(dev, "ID_USB_DRIVER");
	if (cdrom)
		ddev->device->type = DEVICE_TYPE_OPTICAL;
	else
		ddev->device->type = usb ? DEVICE_TYPE_USB : DEVICE_TYPE_DISK;

	udev_setup_device_params(dev, ddev);

	/* Create a snapshot for all disk devices */
	if ((ddev->device->type == DEVICE_TYPE_DISK ||
	     ddev->device->type == DEVICE_TYPE_USB))
		devmapper_init_snapshot(udev->handler, ddev);

	device_handler_discover(udev->handler, ddev);

	return 0;
}

static int udev_handle_dev_add(struct pb_udev *udev, struct udev_device *dev)
{
	const char *subsys;
	const char *name;

	name = udev_device_get_sysname(dev);
	if (!name) {
		pb_debug("udev_device_get_sysname failed\n");
		return -1;
	}

	subsys = udev_device_get_subsystem(dev);
	if (!subsys) {
		pb_debug("udev_device_get_subsystem failed\n");
		return -1;
	}

	if (device_lookup_by_id(udev->handler, name)) {
		pb_debug("device %s is already present?\n", name);
		return -1;
	}

	if (!strcmp(subsys, "block")) {
		return udev_handle_block_add(udev, dev, name);
	}

	pb_log("SKIP %s: unknown subsystem %s\n", name, subsys);
	return -1;
}


static int udev_handle_dev_remove(struct pb_udev *udev, struct udev_device *dev)
{
	struct discover_device *ddev;
	const char *name;

	name = udev_device_get_sysname(dev);
	if (!name) {
		pb_debug("udev_device_get_sysname failed\n");
		return -1;
	}

	ddev = device_lookup_by_id(udev->handler, name);
	if (!ddev)
		return 0;

	device_handler_remove(udev->handler, ddev);

	return 0;
}

/* returns true if further event processing should stop (eg., we've
 * ejected the cdrom)
 */
static bool udev_handle_cdrom_events(struct pb_udev *udev,
		struct udev_device *dev, struct discover_device *ddev)
{
	const char *node;
	bool eject = false;

	node = udev_device_get_devnode(dev);

	/* handle CDROM eject requests */
	if (udev_device_get_property_value(dev, "DISK_EJECT_REQUEST")) {
		pb_debug("udev: eject request\n");

		/* If the device is mounted, cdrom_id's own eject request may
		 * have failed. So, we'll need to do our own here.
		 */
		if (ddev) {
			eject = ddev->mounted;
			udev_handle_dev_remove(udev, dev);
		}

		if (eject)
			cdrom_eject(node);

		return true;
	}

	if (udev_device_get_property_value(dev, "DISK_MEDIA_CHANGE")) {
		if (cdrom_media_present(node))
			udev_handle_dev_add(udev, dev);
		else
			udev_handle_dev_remove(udev, dev);
		return true;
	}

	return false;
}

static int udev_handle_dev_change(struct pb_udev *udev, struct udev_device *dev)
{
	struct discover_device *ddev;
	const char *name;
	int rc = 0;

	name = udev_device_get_sysname(dev);

	ddev = device_lookup_by_id(udev->handler, name);

	/* if this is a CDROM device, process eject & media change requests;
	 * these may stop further processing */
	if (!udev_device_get_property_value(dev, "ID_CDROM")) {
		if (udev_handle_cdrom_events(udev, dev, ddev))
			return 0;
	}

	/* if this is a new device, treat it as an add */
	if (!ddev)
		rc = udev_handle_dev_add(udev, dev);

	return rc;
}

static int udev_handle_dev_action(struct udev_device *dev, const char *action)
{
	struct pb_udev *udev = udev_get_userdata(udev_device_get_udev(dev));
	struct udev_list_entry *list;
	const char *name;

	list = udev_device_get_properties_list_entry(dev);
	name = udev_device_get_sysname(dev);

	pb_debug("udev: action %s, device %s\n", action, name);
	pb_debug("udev: properties:\n");

	for (; list; list = udev_list_entry_get_next(list))
		pb_debug("\t%-20s: %s\n", udev_list_entry_get_name(list),
				udev_list_entry_get_value(list));

	if (!strcmp(action, "add"))
		return udev_handle_dev_add(udev, dev);

	else if (!strcmp(action, "remove"))
		return udev_handle_dev_remove(udev, dev);

	else if (!strcmp(action, "change"))
		return udev_handle_dev_change(udev, dev);

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

	result = udev_enumerate_add_match_is_initialized(enumerate);
	if (result) {
		pb_log("udev_enumerate_add_match_is_initialised failed\n");
		goto fail;
	}

	udev_enumerate_scan_devices(enumerate);

	list = udev_enumerate_get_list_entry(enumerate);

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

	result = udev_monitor_set_receive_buffer_size(m, monitor_bufsize);
	if (result) {
		pb_log("udev_monitor_set_rx_bufsize(%d) failed\n",
			monitor_bufsize);
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

	dev = udev_monitor_receive_device(monitor);
	if (!dev) {
		pb_log("udev_monitor_receive_device failed\n");
		return -1;
	}

	action = udev_device_get_action(dev);

	if (!action) {
		pb_log("udev_device_get_action failed\n");
	} else {
		udev_handle_dev_action(dev, action);
	}

	udev_device_unref(dev);
	return 0;
}

static void udev_log_fn(struct udev __attribute__((unused)) *udev,
	int __attribute__((unused)) priority, const char *file, int line,
	const char *fn, const char *format, va_list args)
{
      pb_log("libudev: %s %s:%d: ", fn, file, line);
      vfprintf(pb_log_get_stream(), format, args);
}

struct pb_udev *udev_init(struct device_handler *handler,
		struct waitset *waitset)
{
	struct pb_udev *udev;
	int result;

	udev = talloc_zero(handler, struct pb_udev);
	talloc_set_destructor(udev, udev_destructor);
	udev->handler = handler;

	udev->udev = udev_new();

	if (!udev->udev) {
		pb_log("udev_new failed\n");
		goto fail;
	}

	udev_set_userdata(udev->udev, udev);

	udev_set_log_fn(udev->udev, udev_log_fn);

	result = udev_setup_monitor(udev->udev, &udev->monitor);
	if (result)
		goto fail;

	result = udev_enumerate(udev->udev);
	if (result)
		goto fail;

	waiter_register_io(waitset, udev_monitor_get_fd(udev->monitor), WAIT_IN,
		udev_process, udev->monitor);

	pb_debug("%s: waiting on udev\n", __func__);

	return udev;

fail:
	talloc_free(udev);
	return NULL;
}

void udev_reinit(struct pb_udev *udev)
{
	pb_log("udev: reinit requested, starting enumeration\n");
	udev_enumerate(udev->udev);
}
