#ifndef _DISCOVER_CLIENT_H
#define _DISCOVER_CLIENT_H

#include <pb-protocol/pb-protocol.h>
#include "ui/common/device.h"

struct discover_client;

struct discover_client_ops {
	int	(*add_device)(struct device *);
	void	(*remove_device)(char *);
};

struct discover_client *discover_client_init(struct discover_client_ops *ops);

int discover_client_get_fd(struct discover_client *client);

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
