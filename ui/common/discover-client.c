
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <asm/byteorder.h>

#include <talloc/talloc.h>
#include <log/log.h>

#include "ui/common/discover-client.h"
#include "pb-protocol/pb-protocol.h"

struct discover_client {
	int fd;
	struct discover_client_ops ops;
	int n_devices;
	struct device **devices;
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

	client->n_devices = 0;
	client->devices = NULL;

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

static void add_device(struct discover_client *client, struct device *device)
{
	client->n_devices++;
	client->devices = talloc_realloc(client, client->devices,
			struct device *, client->n_devices);

	client->devices[client->n_devices - 1] = device;
	talloc_steal(client, device);

	client->ops.add_device(device, client->ops.cb_arg);
}

static void remove_device(struct discover_client *client, const char *id)
{
	struct device *device = NULL;
	int i;

	for (i = 0; i < client->n_devices; i++) {
		if (!strcmp(client->devices[i]->id, id)) {
			device = client->devices[i];
			break;
		}
	}

	if (!device)
		return;

	/* remove the device from the client's device array */
	client->n_devices--;
	memmove(&client->devices[i], &client->devices[i+1],
			client->n_devices - i);
	client->devices = talloc_realloc(client, client->devices,
			struct device *, client->n_devices);

	/* notify the UI */
	client->ops.remove_device(device, client->ops.cb_arg);

	talloc_free(device);
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

		add_device(client, dev);
		break;
	case PB_PROTOCOL_ACTION_REMOVE:
		dev_id = pb_protocol_deserialise_string(client, message);
		if (!dev_id) {
			pb_log("%s: no device id?\n", __func__);
			return 0;
		}
		remove_device(client, dev_id);
		break;
	default:
		pb_log("%s: unknown action %d\n", __func__, message->action);
	}


	return 0;
}

/* accessors for discovered devices */
int discover_client_device_count(struct discover_client *client)
{
	return client->n_devices;
}

struct device *discover_client_get_device(struct discover_client *client,
		int index)
{
	if (index < 0 || index >= client->n_devices)
		return NULL;

	return client->devices[index];
}
