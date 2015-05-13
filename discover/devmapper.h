#ifndef _DEVMAPPER_H
#define _DEVMAPPER_H

#include "device-handler.h"

int devmapper_init_snapshot(struct device_handler *handler,
		     struct discover_device *device);
int devmapper_destroy_snapshot(struct discover_device *device);
int devmapper_merge_snapshot(struct discover_device *device);

#endif /* _DEVMAPPER_H */
