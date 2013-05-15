
#include <talloc/talloc.h>
#include <types/types.h>

#include "device-handler.h"


void discover_server_notify_device_add(struct discover_server *server,
		struct device *device)
{
	(void)server;
	(void)device;
}

void discover_server_notify_boot_option_add(struct discover_server *server,
		struct boot_option *option)
{
	(void)server;
	(void)option;
}

void discover_server_notify_device_remove(struct discover_server *server,
		struct device *device)
{
	(void)server;
	(void)device;
}

