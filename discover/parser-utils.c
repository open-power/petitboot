
#include <string.h>

#include <log/log.h>
#include <talloc/talloc.h>

#include "types/types.h"
#include "event.h"
#include "udev.h"
#include "device-handler.h"
#include "parser-utils.h"

void device_add_boot_option(struct device *device,
		struct boot_option *boot_option)
{
	pb_log("%s: %s\n", __func__, device->id);
	pb_log(" id     '%s'\n", boot_option->id);
	pb_log(" name   '%s'\n", boot_option->name);
	pb_log(" descr  '%s'\n", boot_option->description);
	pb_log(" icon   '%s'\n", boot_option->icon_file);
	pb_log(" image  '%s'\n", boot_option->boot_image_file);
	pb_log(" initrd '%s'\n", boot_option->initrd_file);
	pb_log(" args   '%s'\n", boot_option->boot_args);
	list_add(&device->boot_options, &boot_option->list);
	talloc_steal(device, boot_option);
}

const char *generic_icon_file(enum generic_icon_type type)
{
	switch (type) {
	case ICON_TYPE_DISK:
		return artwork_pathname("hdd.png");
	case ICON_TYPE_USB:
		return artwork_pathname("usbpen.png");
	case ICON_TYPE_OPTICAL:
		return artwork_pathname("cdrom.png");
	case ICON_TYPE_NETWORK:
	case ICON_TYPE_UNKNOWN:
		break;
	}
	return artwork_pathname("hdd.png");
}

enum generic_icon_type guess_device_type(struct discover_context *ctx)
{
	struct event *event;
	const char *type, *bus;

	event = ctx->event;

	type = event_get_param(event, "ID_TYPE");
	bus = event_get_param(event, "ID_BUS");

	if (type && streq(type, "cd"))
		return ICON_TYPE_OPTICAL;
	if (!bus)
		return ICON_TYPE_UNKNOWN;
	if (streq(bus, "usb"))
		return ICON_TYPE_USB;
	if (streq(bus, "ata") || streq(bus, "scsi"))
		return ICON_TYPE_DISK;
	return ICON_TYPE_UNKNOWN;
}
