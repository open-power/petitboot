
#include <string.h>

#include <log/log.h>
#include <talloc/talloc.h>

#include "types/types.h"
#include "event.h"
#include "udev.h"
#include "device-handler.h"
#include "parser-utils.h"

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
