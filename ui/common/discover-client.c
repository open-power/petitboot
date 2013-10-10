
#include <assert.h>
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

#include "discover-client.h"
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

void discover_client_destroy(struct discover_client *client)
{
	talloc_free(client);
}

static struct device *find_device(struct discover_client *client,
		const char *id)
{
	int i;

	for (i = 0; i < client->n_devices; i++) {
		struct device *dev = client->devices[i];
		if (!strcmp(dev->id, id))
			return dev;
	}

	return NULL;
}

static void device_add(struct discover_client *client, struct device *device)
{
	client->n_devices++;
	client->devices = talloc_realloc(client, client->devices,
			struct device *, client->n_devices);

	client->devices[client->n_devices - 1] = device;
	talloc_steal(client, device);

	if (client->ops.device_add)
		client->ops.device_add(device, client->ops.cb_arg);
}

static void boot_option_add(struct discover_client *client,
		struct boot_option *opt)
{
	struct device *dev;

	dev = find_device(client, opt->device_id);

	/* we require that devices are already present before any boot options
	 * are added */
	assert(dev);

	talloc_steal(dev, opt);

	if (client->ops.boot_option_add)
		client->ops.boot_option_add(dev, opt, client->ops.cb_arg);
}

static void device_remove(struct discover_client *client, const char *id)
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
			(client->n_devices - i) * sizeof(client->devices[0]));
	client->devices = talloc_realloc(client, client->devices,
			struct device *, client->n_devices);

	/* notify the UI */
	client->ops.device_remove(device, client->ops.cb_arg);

	talloc_free(device);
}

static void update_status(struct discover_client *client,
		struct boot_status *status)
{
	if (client->ops.update_status)
		client->ops.update_status(status, client->ops.cb_arg);
	talloc_free(status);
}

static void update_sysinfo(struct discover_client *client,
		struct system_info *sysinfo)
{
	if (client->ops.update_sysinfo)
		client->ops.update_sysinfo(sysinfo, client->ops.cb_arg);
	talloc_free(sysinfo);
}

static int discover_client_process(void *arg)
{
	struct discover_client *client = arg;
	struct pb_protocol_message *message;
	struct system_info *sysinfo;
	struct boot_status *status;
	struct boot_option *opt;
	struct device *dev;
	char *dev_id;
	int rc;

	message = pb_protocol_read_message(client, client->fd);

	if (!message)
		return -1;

	switch (message->action) {
	case PB_PROTOCOL_ACTION_DEVICE_ADD:
		dev = talloc_zero(client, struct device);
		list_init(&dev->boot_options);

		rc = pb_protocol_deserialise_device(dev, message);
		if (rc) {
			pb_log("%s: no device?\n", __func__);
			return 0;
		}

		device_add(client, dev);
		break;
	case PB_PROTOCOL_ACTION_BOOT_OPTION_ADD:
		opt = talloc_zero(client, struct boot_option);

		rc = pb_protocol_deserialise_boot_option(opt, message);
		if (rc) {
			pb_log("%s: no boot_option?\n", __func__);
			return 0;
		}

		boot_option_add(client, opt);
		break;
	case PB_PROTOCOL_ACTION_DEVICE_REMOVE:
		dev_id = pb_protocol_deserialise_string(client, message);
		if (!dev_id) {
			pb_log("%s: no device id?\n", __func__);
			return 0;
		}
		device_remove(client, dev_id);
		break;
	case PB_PROTOCOL_ACTION_STATUS:
		status = talloc_zero(client, struct boot_status);

		rc = pb_protocol_deserialise_boot_status(status, message);
		if (rc) {
			pb_log("%s: invalid status message?\n", __func__);
			return 0;
		}
		update_status(client, status);
		break;
	case PB_PROTOCOL_ACTION_SYSTEM_INFO:
		sysinfo = talloc_zero(client, struct system_info);

		rc = pb_protocol_deserialise_system_info(sysinfo, message);
		if (rc) {
			pb_log("%s: invalid sysinfo message?\n", __func__);
			return 0;
		}
		update_sysinfo(client, sysinfo);
		break;
	default:
		pb_log("%s: unknown action %d\n", __func__, message->action);
	}


	return 0;
}

struct discover_client* discover_client_init(struct waitset *waitset,
	const struct discover_client_ops *ops, void *cb_arg)
{
	struct discover_client *client;
	struct sockaddr_un addr;

	client = talloc(NULL, struct discover_client);
	if (!client)
		return NULL;

	memcpy(&client->ops, ops, sizeof(client->ops));
	client->ops.cb_arg = cb_arg;

	client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client->fd < 0) {
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

	waiter_register_io(waitset, client->fd, WAIT_IN,
			discover_client_process, client);

	return client;

out_err:
	talloc_free(client);
	return NULL;
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

static void create_boot_command(struct boot_command *command,
		const struct device *device __attribute__((unused)),
		const struct boot_option *boot_option,
		const struct pb_boot_data *data)
{
	command->option_id = boot_option ? boot_option->id : NULL;
	command->boot_image_file = data->image;
	command->initrd_file = data->initrd;
	command->dtb_file = data->dtb;
	command->boot_args = data->args;
}

int discover_client_boot(struct discover_client *client,
		const struct device *device,
		const struct boot_option *boot_option,
		const struct pb_boot_data *data)
{
	struct pb_protocol_message *message;
	struct boot_command boot_command;
	int len, rc;

	create_boot_command(&boot_command, device, boot_option, data);

	len = pb_protocol_boot_len(&boot_command);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_BOOT, len);

	if (!message)
		return -1;

	pb_protocol_serialise_boot_command(&boot_command,
			message->payload, len);

	rc = pb_protocol_write_message(client->fd, message);

	return rc;
}

int discover_client_cancel_default(struct discover_client *client)
{
	struct pb_protocol_message *message;

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_CANCEL_DEFAULT, 0);

	if (!message)
		return -1;

	return pb_protocol_write_message(client->fd, message);
}
