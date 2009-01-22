
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <asm/byteorder.h>

#include <talloc/talloc.h>
#include <log.h>

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

struct discover_client* discover_client_init(
	const struct discover_client_ops *ops)
{
	struct discover_client *client;
	struct sockaddr_un addr;

	client = talloc(NULL, struct discover_client);
	if (!client)
		return NULL;

	memcpy(&client->ops, ops, sizeof(client->ops));

	client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (!client->fd < 0) {
		pb_log("%s: socket: %s\n", __func__, strerror(errno));
		goto out_err;
	}

	talloc_set_destructor(client, discover_client_destructor);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PB_SOCKET_PATH);

	if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr))) {
		pb_log("%s: connect: %s\n", __func__, strerror(errno));
		goto out_err;
	}

	return client;

out_err:
	talloc_free(client);
	return NULL;
}

int discover_client_get_fd(const struct discover_client *client)
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
		return -1;

	switch (message->action) {
	case PB_PROTOCOL_ACTION_ADD:
		dev = pb_protocol_deserialise_device(client, message);
		if (!dev) {
			pb_log("%s: no device?\n", __func__);
			return 0;
		}
		client->ops.add_device(dev, client->ops.cb_arg);
		talloc_free(dev);
		break;
	case PB_PROTOCOL_ACTION_REMOVE:
		dev_id = pb_protocol_deserialise_string(client, message);
		if (!dev_id) {
			pb_log("%s: no device id?\n", __func__);
			return 0;
		}
		client->ops.remove_device(dev_id, client->ops.cb_arg);
		break;
	default:
		pb_log("%s: unknown action %d\n", __func__, message->action);
	}


	return 0;
}
