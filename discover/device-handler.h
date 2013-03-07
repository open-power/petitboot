#ifndef _DEVICE_HANDLER_H
#define _DEVICE_HANDLER_H

#include <list/list.h>

struct device_handler;
struct discover_server;
struct boot_command;
struct event;
struct device;

struct discover_context {
	char *id;
	char *device_path;
	char *mount_path;
	struct event *event;
	struct device *device;
	char **links;
	int n_links;

	struct list_item list;
};

struct device_handler *device_handler_init(struct discover_server *server,
		int dry_run);

void device_handler_destroy(struct device_handler *devices);

int device_handler_get_device_count(const struct device_handler *handler);
const struct device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index);

int device_handler_event(struct device_handler *handler, struct event *event);

void device_handler_boot(struct device_handler *handler,
		struct boot_command *cmd);

#endif /* _DEVICE_HANDLER_H */
