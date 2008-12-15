
#include <talloc/talloc.h>
#include <pb-protocol/pb-protocol.h>

#include "device-handler.h"

struct device_handler {
	struct discover_server *server;

	struct device *devices;
	int n_devices;
};

struct device_handler *device_handler_init(struct discover_server *server)
{
	struct device_handler *handler;

	handler = talloc(NULL, struct device_handler);
	handler->devices = NULL;
	handler->n_devices = 0;

	return handler;
}

void device_handler_destroy(struct device_handler *devices)
{
	talloc_free(devices);
}

static struct boot_option options[] = {
	{
		.id = "1.1",
		.name = "meep one",
		.description = "meep description one",
		.icon_file = "meep.one.png",
		.boot_args = "root=/dev/sda1",
	},
};

static struct device device = {
	.id = "1",
	.name = "meep",
	.description = "meep description",
	.icon_file = "meep.png",
	.n_options = 1,
	.options = options,
};

int device_handler_get_current_devices(struct device_handler *handler,
		struct device **devices)

{
	*devices = &device;
	return 1;
}


int device_handler_event(struct device_handler *handler,
		struct udev_event *event)
{
	return 0;
}
