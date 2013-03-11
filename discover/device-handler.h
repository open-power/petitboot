#ifndef _DEVICE_HANDLER_H
#define _DEVICE_HANDLER_H

#include <list/list.h>

struct device_handler;
struct discover_device;
struct discover_server;
struct boot_option;
struct boot_command;
struct event;
struct device;

struct discover_device {
	struct device		*device;

	char			**links;
	int			n_links;

	char			*mount_path;
	char			*device_path;
};

struct discover_context {
	struct event		*event;
	struct discover_device	*device;
	struct list		boot_options;
};

struct device_handler *device_handler_init(struct discover_server *server,
		int dry_run);

void device_handler_destroy(struct device_handler *devices);

int device_handler_get_device_count(const struct device_handler *handler);
const struct device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index);

struct device *discover_context_device(struct discover_context *ctx);
void discover_context_add_boot_option(struct discover_context *ctx,
		struct boot_option *opt);

int device_handler_event(struct device_handler *handler, struct event *event);

void device_handler_boot(struct device_handler *handler,
		struct boot_command *cmd);

#endif /* _DEVICE_HANDLER_H */
