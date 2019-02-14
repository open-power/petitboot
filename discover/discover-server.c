#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <asm/byteorder.h>
#include <grp.h>
#include <sys/stat.h>

#include <pb-config/pb-config.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <log/log.h>
#include <crypt/crypt.h>
#include <i18n/i18n.h>

#include "pb-protocol/pb-protocol.h"
#include "list/list.h"

#include "device-handler.h"
#include "discover-server.h"
#include "platform.h"
#include "sysinfo.h"

struct discover_server {
	int socket;
	struct waitset *waitset;
	struct waiter *waiter;
	struct list clients;
	struct list status;
	struct device_handler *device_handler;
	bool restrict_clients;
};

struct client {
	struct discover_server *server;
	struct list_item list;
	struct waiter *waiter;
	int fd;
	bool remote_closed;
	bool can_modify;
	struct waiter *auth_waiter;
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

	if (client->waiter)
		waiter_remove(client->waiter);

	if (client->auth_waiter)
		waiter_remove(client->auth_waiter);

	list_remove(&client->list);

	return 0;

}

static void print_clients(struct discover_server *server)
	__attribute__((unused));

static void print_clients(struct discover_server *server)
{
	struct client *client;

	pb_debug("current clients [%p,%p,%p]:\n",
			&server->clients.head,
			server->clients.head.prev,
			server->clients.head.next);
	list_for_each_entry(&server->clients, client, list)
		pb_debug("\t[%p,%p,%p] client: %d\n", &client->list,
				client->list.prev, client->list.next,
				client->fd);
}

static int client_write_message(
		struct discover_server *server __attribute__((unused)),
		struct client *client, struct pb_protocol_message *message)
{
	int rc;

	if (client->remote_closed)
		return -1;

	rc = pb_protocol_write_message(client->fd, message);
	if (rc)
		client->remote_closed = true;

	return rc;
}

static int write_device_add_message(struct discover_server *server,
		struct client *client, const struct device *dev)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_device_len(dev);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_DEVICE_ADD, len);
	if (!message)
		return -1;

	pb_protocol_serialise_device(dev, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_boot_option_add_message(struct discover_server *server,
		struct client *client, const struct boot_option *opt)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_boot_option_len(opt);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_BOOT_OPTION_ADD, len);
	if (!message)
		return -1;

	pb_protocol_serialise_boot_option(opt, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_plugin_option_add_message(struct discover_server *server,
		struct client *client, const struct plugin_option *opt)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_plugin_option_len(opt);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_PLUGIN_OPTION_ADD, len);
	if (!message)
		return -1;

	pb_protocol_serialise_plugin_option(opt, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_plugins_remove_message(struct discover_server *server,
		struct client *client)
{
	struct pb_protocol_message *message;

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_PLUGINS_REMOVE, 0);
	if (!message)
		return -1;

	/* No payload so nothing to serialise */

	return client_write_message(server, client, message);
}

static int write_device_remove_message(struct discover_server *server,
		struct client *client, char *dev_id)
{
	struct pb_protocol_message *message;
	int len;

	len = strlen(dev_id) + sizeof(uint32_t);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_DEVICE_REMOVE, len);
	if (!message)
		return -1;

	pb_protocol_serialise_string(message->payload, dev_id);

	return client_write_message(server, client, message);
}

static int write_boot_status_message(struct discover_server *server,
		struct client *client, const struct status *status)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_boot_status_len(status);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_STATUS, len);
	if (!message)
		return -1;

	pb_protocol_serialise_boot_status(status, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_system_info_message(struct discover_server *server,
		struct client *client, const struct system_info *sysinfo)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_system_info_len(sysinfo);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_SYSTEM_INFO, len);
	if (!message)
		return -1;

	pb_protocol_serialise_system_info(sysinfo, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_config_message(struct discover_server *server,
		struct client *client, const struct config *config)
{
	struct pb_protocol_message *message;
	int len;

	len = pb_protocol_config_len(config);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_CONFIG, len);
	if (!message)
		return -1;

	pb_protocol_serialise_config(config, message->payload, len);

	return client_write_message(server, client, message);
}

static int write_authenticate_message(struct discover_server *server,
		struct client *client)
{
	struct pb_protocol_message *message;
	struct auth_message auth_msg;
	int len;

	auth_msg.op = AUTH_MSG_RESPONSE;
	auth_msg.authenticated = client->can_modify;

	len = pb_protocol_authenticate_len(&auth_msg);

	message = pb_protocol_create_message(client,
			PB_PROTOCOL_ACTION_AUTHENTICATE, len);
	if (!message)
		return -1;

	pb_protocol_serialise_authenticate(&auth_msg, message->payload, len);

	return client_write_message(server, client, message);
}

static int client_auth_timeout(void *arg)
{
	struct client *client = arg;
	int rc;

	client->auth_waiter = NULL;
	client->can_modify = false;

	rc = write_authenticate_message(client->server, client);
	if (rc)
		pb_log("failed to send client auth timeout\n");

	return 0;
}

static int discover_server_handle_auth_message(struct client *client,
		struct auth_message *auth_msg)
{
	struct status *status;
	char *hash;
	int rc;

	status = talloc_zero(client, struct status);

	switch (auth_msg->op) {
	case AUTH_MSG_REQUEST:
		if (!crypt_check_password(auth_msg->password)) {
			rc = -1;
			pb_log("Client failed to authenticate\n");
			status->type = STATUS_ERROR;
			status->message = talloc_asprintf(status,
					_("Password incorrect"));
		} else {
			client->can_modify = true;
			rc = write_authenticate_message(client->server,
					client);
			if (client->auth_waiter)
				waiter_remove(client->auth_waiter);
			client->auth_waiter = waiter_register_timeout(
					client->server->waitset,
					300000, /* 5 min */
					client_auth_timeout, client);
			pb_log("Client authenticated\n");
			status->type = STATUS_INFO;
			status->message = talloc_asprintf(status,
					_("Authenticated successfully"));
		}
		break;
	case AUTH_MSG_SET:
		if (client->server->restrict_clients) {
			if (!crypt_check_password(auth_msg->set_password.password)) {
				rc = -1;
				pb_log("Wrong password for set request\n");
				status->type = STATUS_ERROR;
				status->message = talloc_asprintf(status,
						_("Password incorrect"));
				break;
			}
		}

		rc = crypt_set_password(auth_msg,
				auth_msg->set_password.new_password);
		if (rc) {
			pb_log("Failed to set password\n");
			status->type = STATUS_ERROR;
			status->message = talloc_asprintf(status,
					_("Error setting password"));
		} else {
			if (!auth_msg->set_password.new_password ||
				!strlen(auth_msg->set_password.new_password)) {
				platform_set_password("");
				discover_server_set_auth_mode(client->server,
						false);
				pb_log("Password cleared\n");
			} else {
				hash = crypt_get_hash(auth_msg);
				platform_set_password(hash);
				talloc_free(hash);
				discover_server_set_auth_mode(client->server,
						true);
			}
			pb_log("System password changed\n");
			status->type = STATUS_ERROR;
			status->message = talloc_asprintf(status,
					_("Password updated successfully"));
		}
		break;
	case AUTH_MSG_DECRYPT:
		if (!client->can_modify) {
			pb_log("Unauthenticated client tried to open encrypted device %s\n",
					auth_msg->decrypt_dev.device_id);
			rc = -1;
			status->type = STATUS_ERROR;
			status->message = talloc_asprintf(status,
					_("Must authenticate before opening encrypted device"));
			break;
		}

		device_handler_open_encrypted_dev(client->server->device_handler,
				auth_msg->decrypt_dev.password,
				auth_msg->decrypt_dev.device_id);
		break;
	default:
		pb_log("%s: unknown op\n", __func__);
		rc = -1;
		break;
	}

	if (status->message)
		write_boot_status_message(client->server, client, status);
	talloc_free(status);

	return rc;
}

static int discover_server_process_message(void *arg)
{
	struct autoboot_option *autoboot_opt;
	struct pb_protocol_message *message;
	struct boot_command *boot_command;
	struct auth_message *auth_msg;
	struct status *status;
	struct client *client = arg;
	struct config *config;
	char *url;
	int rc;

	message = pb_protocol_read_message(client, client->fd);

	if (!message) {
		talloc_free(client);
		return 0;
	}

	/*
	 * If crypt support is enabled, non-authorised clients can only delay
	 * boot, not configure options or change the default boot option.
	 */
	if (!client->can_modify) {
		switch (message->action) {
		case PB_PROTOCOL_ACTION_BOOT:
			boot_command = talloc(client, struct boot_command);

			rc = pb_protocol_deserialise_boot_command(boot_command,
					message);
			if (rc) {
				pb_log("%s: no boot command?", __func__);
				return 0;
			}

			device_handler_boot(client->server->device_handler,
					client->can_modify, boot_command);
			break;
		case PB_PROTOCOL_ACTION_CANCEL_DEFAULT:
			device_handler_cancel_default(client->server->device_handler);
			break;
		case PB_PROTOCOL_ACTION_AUTHENTICATE:
			auth_msg = talloc(client, struct auth_message);
			rc = pb_protocol_deserialise_authenticate(
					auth_msg, message);
			if (rc) {
				pb_log("Couldn't parse client's auth request\n");
				break;
			}

			rc = discover_server_handle_auth_message(client,
					auth_msg);
			talloc_free(auth_msg);
			break;
		default:
			pb_log("non-root client tried to perform action %d\n",
					message->action);
			status = talloc_zero(client, struct status);
			if (status) {
				status->type = STATUS_ERROR;
				status->message = talloc_asprintf(status,
						"Client must run as root to make changes");
				write_boot_status_message(client->server, client,
						status);
				talloc_free(status);
			}
		}
		return 0;
	}

	switch (message->action) {
	case PB_PROTOCOL_ACTION_BOOT:
		boot_command = talloc(client, struct boot_command);

		rc = pb_protocol_deserialise_boot_command(boot_command,
				message);
		if (rc) {
			pb_log_fn("no boot command?\n");
			return 0;
		}

		device_handler_boot(client->server->device_handler,
				client->can_modify, boot_command);
		break;

	case PB_PROTOCOL_ACTION_CANCEL_DEFAULT:
		device_handler_cancel_default(client->server->device_handler);
		break;

	case PB_PROTOCOL_ACTION_REINIT:
		device_handler_reinit(client->server->device_handler);
		break;

	case PB_PROTOCOL_ACTION_CONFIG:
		config = talloc_zero(client, struct config);

		rc = pb_protocol_deserialise_config(config, message);
		if (rc) {
			pb_log_fn("no config?\n");
			return 0;
		}

		device_handler_update_config(client->server->device_handler,
				config);
		break;

	case PB_PROTOCOL_ACTION_ADD_URL:
		url = pb_protocol_deserialise_string((void *) client, message);

		device_handler_process_url(client->server->device_handler,
				url, NULL, NULL);
		break;

	case PB_PROTOCOL_ACTION_PLUGIN_INSTALL:
		url = pb_protocol_deserialise_string((void *) client, message);

		device_handler_install_plugin(client->server->device_handler,
				url);
		break;

	case PB_PROTOCOL_ACTION_TEMP_AUTOBOOT:
		autoboot_opt = talloc_zero(client, struct autoboot_option);
		rc = pb_protocol_deserialise_temp_autoboot(autoboot_opt,
				message);
		if (rc) {
			pb_log("can't parse temporary autoboot message\n");
			return 0;
		}

		device_handler_apply_temp_autoboot(
				client->server->device_handler,
				autoboot_opt);
		break;

	/* For AUTH_MSG_SET */
	case PB_PROTOCOL_ACTION_AUTHENTICATE:
		auth_msg = talloc(client, struct auth_message);
		rc = pb_protocol_deserialise_authenticate(
				auth_msg, message);
		if (rc) {
			pb_log("Couldn't parse client's auth request\n");
			break;
		}

		rc = discover_server_handle_auth_message(client, auth_msg);
		talloc_free(auth_msg);
		break;
	default:
		pb_log_fn("invalid action %d\n", message->action);
		return 0;
	}


	return 0;
}

void discover_server_set_auth_mode(struct discover_server *server,
		bool restrict_clients)
{
	struct client *client;

	server->restrict_clients = restrict_clients;

	list_for_each_entry(&server->clients, client, list) {
		client->can_modify = !restrict_clients;
		write_authenticate_message(server, client);
	}
}

static int discover_server_process_connection(void *arg)
{
	struct discover_server *server = arg;
	struct statuslog_entry *entry;
	int fd, rc, i, n_devices, n_plugins;
	struct client *client;
	struct ucred ucred;
	socklen_t len;

	/* accept the incoming connection */
	fd = accept(server->socket, NULL, NULL);
	if (fd < 0) {
		pb_log("accept: %s\n", strerror(errno));
		return 0;
	}

	/* add to our list of clients */
	client = talloc_zero(server, struct client);
	list_add(&server->clients, &client->list);

	talloc_set_destructor(client, client_destructor);

	client->fd = fd;
	client->server = server;
	client->waiter = waiter_register_io(server->waitset, client->fd,
				WAIT_IN, discover_server_process_message,
				client);

	/*
	 * get some info on the connecting process - if the client is being
	 * run as root allow them to make changes
	 */
	if (server->restrict_clients) {
		len = sizeof(struct ucred);
		rc = getsockopt(client->fd, SOL_SOCKET, SO_PEERCRED, &ucred,
				&len);
		if (rc) {
			pb_log("Failed to get socket info - restricting client\n");
			client->can_modify = false;
		} else {
			pb_log("Client details: pid: %d, uid: %d, egid: %d\n",
					ucred.pid, ucred.uid, ucred.gid);
			client->can_modify = ucred.uid == 0;
		}
	} else
		client->can_modify = true;

	/* send auth status to client */
	rc = write_authenticate_message(server, client);
	if (rc)
		return 0;

	/* send sysinfo to client */
	rc = write_system_info_message(server, client, system_info_get());
	if (rc)
		return 0;

	/* send config to client */
	rc = write_config_message(server, client, config_get());
	if (rc)
		return 0;

	/* send existing devices to client */
	n_devices = device_handler_get_device_count(server->device_handler);
	for (i = 0; i < n_devices; i++) {
		const struct discover_boot_option *opt;
		const struct discover_device *device;

		device = device_handler_get_device(server->device_handler, i);
		rc = write_device_add_message(server, client, device->device);
		if (rc)
			return 0;

		list_for_each_entry(&device->boot_options, opt, list) {
			rc = write_boot_option_add_message(server, client,
					opt->option);
			if (rc)
				return 0;
		}
	}

	/* send status backlog to client */
	list_for_each_entry(&server->status, entry, list)
		write_boot_status_message(server, client, entry->status);

	/* send installed plugins to client */
	n_plugins = device_handler_get_plugin_count(server->device_handler);
	for (i = 0; i < n_plugins; i++) {
		const struct plugin_option *plugin;

		plugin = device_handler_get_plugin(server->device_handler, i);
		write_plugin_option_add_message(server, client, plugin);
	}

	return 0;
}

void discover_server_notify_device_add(struct discover_server *server,
		struct device *device)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_device_add_message(server, client, device);

}

void discover_server_notify_boot_option_add(struct discover_server *server,
		struct boot_option *boot_option)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_boot_option_add_message(server, client, boot_option);
}

void discover_server_notify_device_remove(struct discover_server *server,
		struct device *device)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_device_remove_message(server, client, device->id);

}

void discover_server_notify_boot_status(struct discover_server *server,
		struct status *status)
{
	struct statuslog_entry *entry;
	struct client *client;

	/* Duplicate the status struct to add to the backlog */
	entry = talloc(server, struct statuslog_entry);
	if (!entry) {
		pb_log("Failed to allocated saved status!\n");
	} else {
		entry->status = talloc(entry, struct status);
		if (entry->status) {
			entry->status->type = status->type;
			entry->status->message = talloc_strdup(entry->status,
							       status->message);
			entry->status->backlog = true;
			list_add_tail(&server->status, &entry->list);
		} else {
			talloc_free(entry);
		}
	}

	list_for_each_entry(&server->clients, client, list)
		write_boot_status_message(server, client, status);
}

void discover_server_notify_system_info(struct discover_server *server,
		const struct system_info *sysinfo)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_system_info_message(server, client, sysinfo);
}

void discover_server_notify_config(struct discover_server *server,
		const struct config *config)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_config_message(server, client, config);
}

void discover_server_notify_plugin_option_add(struct discover_server *server,
		struct plugin_option *opt)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_plugin_option_add_message(server, client, opt);
}

void discover_server_notify_plugins_remove(struct discover_server *server)
{
	struct client *client;

	list_for_each_entry(&server->clients, client, list)
		write_plugins_remove_message(server, client);
}

void discover_server_set_device_source(struct discover_server *server,
		struct device_handler *handler)
{
	server->device_handler = handler;
}

struct discover_server *discover_server_init(struct waitset *waitset)
{
	struct discover_server *server;
	struct sockaddr_un addr;
	struct group *group;

	server = talloc(NULL, struct discover_server);
	if (!server)
		return NULL;

	server->waiter = NULL;
	server->waitset = waitset;
	list_init(&server->clients);
	list_init(&server->status);

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

	/* Allow all clients to communicate on this socket */
	group = getgrnam("petitgroup");
	if (group) {
		chown(PB_SOCKET_PATH, 0, group->gr_gid);
		chmod(PB_SOCKET_PATH, 0660);
	}

	if (listen(server->socket, 8)) {
		pb_log("server socket listen: %s\n", strerror(errno));
		goto out_err;
	}

	server->waiter = waiter_register_io(server->waitset, server->socket,
			WAIT_IN, discover_server_process_connection, server);

	return server;

out_err:
	talloc_free(server);
	return NULL;
}

void discover_server_destroy(struct discover_server *server)
{
	talloc_free(server);
}

