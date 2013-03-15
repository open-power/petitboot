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

	char			*uuid;
	char			*label;

	char			*mount_path;
	char			*device_path;

	struct list		boot_options;
};

struct discover_boot_option {
	struct parser		*source;
	struct discover_device	*device;
	struct boot_option	*option;
	struct list_item	list;

	struct resource		*boot_image;
	struct resource		*initrd;
	struct resource		*icon;
};


struct discover_context {
	struct parser		*parser;
	struct event		*event;
	struct discover_device	*device;
	struct list		boot_options;
};

struct device_handler *device_handler_init(struct discover_server *server,
		int dry_run);

void device_handler_destroy(struct device_handler *devices);

int device_handler_get_device_count(const struct device_handler *handler);
const struct discover_device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index);

struct device *discover_context_device(struct discover_context *ctx);
struct discover_boot_option *discover_boot_option_create(
		struct discover_context *ctx,
		struct discover_device *dev);
void discover_context_add_boot_option(struct discover_context *ctx,
		struct discover_boot_option *opt);

int device_handler_event(struct device_handler *handler, struct event *event);

struct discover_device *device_lookup_by_name(struct device_handler *handler,
		const char *name);
struct discover_device *device_lookup_by_path(struct device_handler *handler,
		const char *path);
struct discover_device *device_lookup_by_uuid(struct device_handler *handler,
		const char *uuid);
struct discover_device *device_lookup_by_label(struct device_handler *handler,
		const char *label);
struct discover_device *device_lookup_by_id(struct device_handler *handler,
		const char *id);

void device_handler_boot(struct device_handler *handler,
		struct boot_command *cmd);

#endif /* _DEVICE_HANDLER_H */
