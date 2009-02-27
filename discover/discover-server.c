
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <asm/byteorder.h>

#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <log/log.h>

#include "pb-protocol/pb-protocol.h"
#include "list/list.h"

#include "device-handler.h"
#include "discover-server.h"

struct discover_server {
	int socket;
	struct waiter *waiter;
	struct list clients;
	struct device_handler *device_handler;
};

struct client {
	struct list_item list;
	int fd;
};


static int server_destructor(void *arg)
{
	struct discover_server *server = arg;

	if (server->waiter)
		waiter_remove(server->waiter);

	if (server->socket >= 0)
		close(server->socket);

	return 0;
}

static int client_destructor(void *arg)
{
	struct client *client = arg;

	if (client->fd >= 0)
		close(client->fd);

	list_remove(&client->list);

	return 0;

}

static void print_clients(struct discover_server *server)
	__attribute__((unused));

static void print_clients(struct discover_server *server)
{
	struct client *client;

	pb_log("current clients [%p,%p,%p]:\n",
			&server->clients.head,
			server->clients.head.prev,
			server->clients.head.next);
	list_for_each_entry(&server->clients, client, list)
		pb_log("\t[%p,%p,%p] client: %d\n", &client->list,
				client->list.prev, client->list.next,
				client->fd);
}

static int client_write_message(
		struct discover_server *server __attribute__((unused)),
		struct client *client, struct pb_protocol_message *message)
{
	int rc;

	rc = pb_protocol_write_message(client->fd, message);
	if (rc)
		talloc_free(client);

	return rc;
}

static int write_add_message(struct discover_server *server,
		struct client *client, const struct device *dev)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_device_len(dev);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_ADD, len);
	if (!message)
		return -1;

	pb_protocol_serialise_device(dev, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_remove_message(struct discover_server *server,
		struct client *client, char *dev_id)
{
	struct pb_protocol_message *message;
	int len;

	len = strlen(dev_id) + sizeof(uint32_t);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_REMOVE, len);
	if (!message)
		return -1;

	pb_protocol_serialise_string(message->payload, dev_id);

	return client_write_message(server, client, message);
}

static int discover_server_process(void *arg)
{
	struct discover_server *server = arg;
	const struct device *devices;
	struct client *client;
	int fd, i, n_devices;

	/* accept the incoming connection */
	fd = accept(server->socket, NULL, 0);
	if (!fd) {
		pb_log("accept: %s\n", strerror(errno));
		return 0;
	}

	/* add to our list of clients */
	client = talloc(server, struct client);
	list_add(&server->clients, &client->list);

	talloc_set_destructor(client, client_destructor);

	client->fd = fd;

	/* send existing devices to client */
	n_devices = device_handler_get_current_devices(server->device_handler,
			&devices);
	for (i = 0; i < n_devices; i++)
		write_add_message(server, client, &devices[i]);

	return 0;
}

void discover_server_notify_add(struct discover_server *server,
		struct device *device)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_add_message(server, client, device);

}

void discover_server_notify_remove(struct discover_server *server,
		struct device *device)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_remove_message(server, client, device->id);

}

void discover_server_set_device_source(struct discover_server *server,
		struct device_handler *handler)
{
	server->device_handler = handler;
}

struct discover_server *discover_server_init(void)
{
	struct discover_server *server;
	struct sockaddr_un addr;

	server = talloc(NULL, struct discover_server);
	if (!server)
		return NULL;

	server->waiter = NULL;
	list_init(&server->clients);

	unlink(PB_SOCKET_PATH);

	server->socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server->socket < 0) {
		pb_log("error creating server socket: %s\n", strerror(errno));
		goto out_err;
	}

	talloc_set_destructor(server, server_destructor);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PB_SOCKET_PATH);

	if (bind(server->socket, (struct sockaddr *)&addr, sizeof(addr))) {
		pb_log("error binding server socket: %s\n", strerror(errno));
		goto out_err;
	}

	if (listen(server->socket, 8)) {
		pb_log("server socket listen: %s\n", strerror(errno));
		goto out_err;
	}

	server->waiter = waiter_register(server->socket, WAIT_IN,
			discover_server_process, server);

	return server;

out_err:
	talloc_free(server);
	return NULL;
}

void discover_server_destroy(struct discover_server *server)
{
	talloc_free(server);
}

