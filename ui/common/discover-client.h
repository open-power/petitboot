#ifndef _DISCOVER_CLIENT_H
#define _DISCOVER_CLIENT_H

#include <pb-protocol/pb-protocol.h>

struct discover_client;

/**
 * struct discover_client_ops - Application supplied client info.
 * @device_add: PB_PROTOCOL_ACTION_ADD event callback.
 * @device_remove: PB_PROTOCOL_ACTION_REMOVE event callback.
 * @cb_arg: Client managed convenience variable passed to callbacks.
 */

struct discover_client_ops {
	int (*device_add)(struct device *device, void *arg);
	void (*device_remove)(struct device *device, void *arg);
	void *cb_arg;
};

struct discover_client *discover_client_init(
	const struct discover_client_ops *ops, void *cb_arg);

int discover_client_get_fd(const struct discover_client *client);

void discover_client_destroy(struct discover_client *client);

/**
 * Process data from the server.
 *
 * Will read from the client socket, and call device_add on any discovered
 * devices.
 * 
 */
int discover_client_process(struct discover_client *client);

/**
 * Get the number of devices that the discover client has stored. This
 * is the set of devices that have been added and not removed
 *
 * @param client The discover client
 * @return	 The number of devices that have been added.
 */
int discover_client_device_count(struct discover_client *client);

/**
 * Get the device at a specific index.
 * @param client A pointer to the discover client
 * @param index  The index of the device to retrieve
 * @return	 The device at the specified index, or NULL if none exists
 */
struct device *discover_client_get_device(struct discover_client *client,
		int index);
#endif
