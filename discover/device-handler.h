#ifndef _DEVICE_HANDLER_H
#define _DEVICE_HANDLER_H

#include <stdbool.h>

#include <list/list.h>
#include <types/types.h>

struct device_handler;
struct discover_device;
struct discover_server;
struct boot_option;
struct boot_command;
struct event;
struct device;
struct waitset;
struct config;

struct discover_device {
	struct device		*device;

	char			**links;
	int			n_links;

	const char		*uuid;
	const char		*label;

	char			*mount_path;
	char			*root_path;
	const char		*device_path;
	struct ramdisk_device	*ramdisk;
	bool			mounted;
	bool			mounted_rw;
	bool			unmount;

	bool			notified;

	struct list		boot_options;
	struct list		params;
};

struct discover_boot_option {
	struct parser		*source;
	struct discover_device	*device;
	struct boot_option	*option;
	struct list_item	list;

	struct resource		*boot_image;
	struct resource		*initrd;
	struct resource		*dtb;
	struct resource		*icon;
};


struct discover_context {
	struct parser		*parser;
	struct event		*event;
	struct discover_device	*device;
	struct list		boot_options;
	struct pb_url		*conf_url;
	struct network		*network;
	void			*test_data;
};

struct ramdisk_device {
	const char	*path;
	char		*snapshot;
	char		*origin;
	char		*base;
	uint64_t	sectors;
};

struct device_handler *device_handler_init(struct discover_server *server,
		struct waitset *waitset, int dry_run);

void device_handler_destroy(struct device_handler *devices);

int device_handler_get_device_count(const struct device_handler *handler);
const struct discover_device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index);

struct discover_device *discover_device_create(struct device_handler *handler,
		const char *id);
void device_handler_add_device(struct device_handler *handler,
		struct discover_device *device);
void device_handler_add_ramdisk(struct device_handler *handler,
		const char *path);
struct ramdisk_device *device_handler_get_ramdisk(
		struct device_handler *handler);
void device_handler_release_ramdisk(struct discover_device *device);
int device_handler_discover(struct device_handler *handler,
		struct discover_device *dev);
int device_handler_dhcp(struct device_handler *handler,
		struct discover_device *dev, struct event *event);
int device_handler_conf(struct device_handler *handler,
		struct discover_device *dev, struct pb_url *url);
void device_handler_remove(struct device_handler *handler,
		struct discover_device *device);

void device_handler_boot_status(void *arg, struct boot_status *status);

struct discover_context *device_handler_discover_context_create(
		struct device_handler *handler,
		struct discover_device *device);
void device_handler_discover_context_commit(struct device_handler *handler,
		struct discover_context *ctx);

struct discover_boot_option *discover_boot_option_create(
		struct discover_context *ctx,
		struct discover_device *dev);
void discover_context_add_boot_option(struct discover_context *ctx,
		struct discover_boot_option *opt);

int device_handler_user_event(struct device_handler *handler,
				struct event *event);

struct discover_device *device_lookup_by_name(struct device_handler *handler,
		const char *name);
struct discover_device *device_lookup_by_uuid(struct device_handler *handler,
		const char *uuid);
struct discover_device *device_lookup_by_label(struct device_handler *handler,
		const char *label);
struct discover_device *device_lookup_by_id(struct device_handler *handler,
		const char *id);
struct discover_device *device_lookup_by_serial(
		struct device_handler *device_handler,
		const char *serial);

void discover_device_set_param(struct discover_device *device,
		const char *name, const char *value);
const char *discover_device_get_param(struct discover_device *device,
		const char *name);

void device_handler_boot(struct device_handler *handler,
		struct boot_command *cmd);
void device_handler_cancel_default(struct device_handler *handler);
void device_handler_update_config(struct device_handler *handler,
		struct config *config);
void device_handler_process_url(struct device_handler *handler,
		const char *url, const char *mac, const char *ip);
void device_handler_reinit(struct device_handler *handler);

int device_request_write(struct discover_device *dev, bool *release);
void device_release_write(struct discover_device *dev, bool release);
void device_sync_snapshots(struct device_handler *handler, const char *device);

#endif /* _DEVICE_HANDLER_H */
