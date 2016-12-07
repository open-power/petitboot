#ifndef _DISCOVER_SERVER_H
#define _DISCOVER_SERVER_H

#include <waiter/waiter.h>

struct discover_server;
struct device_handler;
struct boot_option;
struct status;
struct system_info;
struct device;
struct config;

struct discover_server *discover_server_init(struct waitset *waitset);

void discover_server_destroy(struct discover_server *server);

void discover_server_set_device_source(struct discover_server *server,
		struct device_handler *handler);

void discover_server_notify_device_add(struct discover_server *server,
		struct device *device);
void discover_server_notify_boot_option_add(struct discover_server *server,
		struct boot_option *option);
void discover_server_notify_device_remove(struct discover_server *server,
		struct device *device);
void discover_server_notify_boot_status(struct discover_server *server,
		struct status *status);
void discover_server_notify_system_info(struct discover_server *server,
		const struct system_info *sysinfo);
void discover_server_notify_config(struct discover_server *server,
		const struct config *config);
#endif /* _DISCOVER_SERVER_H */
