#ifndef NETWORK_H
#define NETWORK_H

struct network;
struct device_handler;
struct waitset;

struct network *network_init(struct device_handler *handler,
		struct waitset *waitset, bool dry_run);
int network_shutdown(struct network *network);

#endif /* NETWORK_H */

