#ifndef NETWORK_H
#define NETWORK_H

struct network;
struct device_handler;
struct discover_device;
struct waitset;

struct network *network_init(struct device_handler *handler,
		struct waitset *waitset, bool dry_run);
int network_shutdown(struct network *network);

void network_register_device(struct network *network,
		struct discover_device *dev);
void network_unregister_device(struct network *network,
		struct discover_device *dev);

#endif /* NETWORK_H */

