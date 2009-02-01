#ifndef _DISCOVER_CLIENT_H
#define _DISCOVER_CLIENT_H

#include <pb-protocol/pb-protocol.h>

struct discover_client;

/**
 * struct discover_client_ops - Application supplied client info.
 * @add_device: PB_PROTOCOL_ACTION_ADD event callback.
 * @remove_device: PB_PROTOCOL_ACTION_REMOVE event callback.
 * @cb_arg: Client managed convenience variable passed to callbacks.
 */

struct discover_client_ops {
	int (*add_device)(const struct device *device, void *arg);
	void (*remove_device)(const struct device *device, void *arg);
	void *cb_arg;
};

struct discover_client *discover_client_init(
	const struct discover_client_ops *ops);

int discover_client_get_fd(const struct discover_client *client);

void discover_client_destroy(struct discover_client *client);

/**
 * Process data from the server.
 *
 * Will read from the client socket, and call add_device on any discovered
 * devices.
 * 
 */
int discover_client_process(struct discover_client *client);

#endif
