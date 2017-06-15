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

uint8_t *find_mac_by_name(void *ctx, struct network *network,
		const char *name);

void network_mark_interface_ready(struct device_handler *handler,
		int ifindex, const char *ifname, uint8_t *mac, int hwsize);

#endif /* NETWORK_H */

