
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <asm/byteorder.h>

#include <talloc/talloc.h>

#include "ui/common/discover-client.h"
#include "pb-protocol/pb-protocol.h"

struct discover_client {
	int fd;
	struct discover_client_ops ops;
};

static int discover_client_destructor(void *arg)
{
	struct discover_client *client = arg;

	if (client->fd >= 0)
		close(client->fd);

	return 0;
}

struct discover_client* discover_client_init(struct discover_client_ops *ops)
{
	struct discover_client *client;
	struct sockaddr_un addr;

	client = talloc(NULL, struct discover_client);
	if (!client)
		return NULL;

	memcpy(&client->ops, ops, sizeof(client->ops));

	client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (!client->fd < 0) {
		perror("socket");
		goto out_err;
	}

	talloc_set_destructor(client, discover_client_destructor);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PB_SOCKET_PATH);

	if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr))) {
		perror("connect");
		goto out_err;
	}

	return client;

out_err:
	talloc_free(client);
	return NULL;
}

int discover_client_get_fd(struct discover_client *client)
{
	return client->fd;
}

void discover_client_destroy(struct discover_client *client)
{
	talloc_free(client);
}

int discover_client_process(struct discover_client *client)
{
	struct pb_protocol_message *message;
	struct device *dev;
	char *dev_id;

	message = pb_protocol_read_message(client, client->fd);

	if (!message)
		return 0;

	switch (message->action) {
	case PB_PROTOCOL_ACTION_ADD:
		dev = pb_protocol_deserialise_device(client, message);
		if (!dev) {
			printf("no device?\n");
			return 0;
		}
		client->ops.add_device(dev);
		talloc_free(dev);
		break;
	case PB_PROTOCOL_ACTION_REMOVE:
		dev_id = pb_protocol_deserialise_string(client, message);
		if (!dev_id) {
			printf("no device id?\n");
			return 0;
		}
		client->ops.remove_device(dev_id);
		break;
	default:
		printf("unknown action %d\n", message->action);
	}


	return 0;
}
