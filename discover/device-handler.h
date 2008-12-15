#ifndef _DEVICE_HANDLER_H
#define _DEVICE_HANDLER_H

struct device_handler;
struct discover_server;
struct udev_event;
struct device;

struct device_handler *device_handler_init(struct discover_server *server);

void device_handler_destroy(struct device_handler *devices);

int device_handler_get_current_devices(struct device_handler *handler,
		struct device **devices);

int device_handler_event(struct device_handler *handler,
		struct udev_event *event);

#endif /* _DEVICE_HANDLER_H */
