
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>
#include <pb-protocol/pb-protocol.h>
#include <system/system.h>

#include "device-handler.h"
#include "discover-server.h"
#include "event.h"
#include "parser.h"
#include "udev.h"
#include "paths.h"

#define MOUNT_BIN "/bin/mount"

#define UMOUNT_BIN "/bin/umount"

struct device_handler {
	struct discover_server *server;

	struct device **devices;
	unsigned int n_devices;

	struct list contexts;
};

struct mount_map {
	char *device_path;
	char *mount_point;
};

/**
 * device_handler_add - Add a device to the handler device array.
 */

static void device_handler_add(struct device_handler *handler,
	struct device *device)
{
	handler->n_devices++;
	handler->devices = talloc_realloc(handler, handler->devices,
		struct device *, handler->n_devices);
	handler->devices[handler->n_devices - 1] = device;
}

/**
 * device_handler_remove - Remove a device from the handler device array.
 */

static void device_handler_remove(struct device_handler *handler,
	struct device *device)
{
	unsigned int i;

	for (i = 0; i < handler->n_devices; i++)
		if (handler->devices[i] == device)
			break;

	if (i == handler->n_devices) {
		assert(0 && "unknown device");
		return;
	}

	handler->n_devices--;
	memmove(&handler->devices[i], &handler->devices[i + 1],
		(handler->n_devices - i) * sizeof(handler->devices[0]));
	handler->devices = talloc_realloc(handler, handler->devices,
		struct device *, handler->n_devices);
}

/**
 * device_handler_find - Find a handler device by id.
 */

static struct device *device_handler_find(struct device_handler *handler,
	const char *id)
{
	unsigned int i;

	assert(id);

	for (i = 0; i < handler->n_devices; i++)
		if (handler->devices[i]->id
			&& streq(handler->devices[i]->id, id))
			return handler->devices[i];

	pb_log("%s: unknown device: %s\n", __func__, id);
	return NULL;
}

/**
 * device_handler_get_device_count - Get the count of current handler devices.
 */

int device_handler_get_device_count(const struct device_handler *handler)
{
	return handler->n_devices;
}

/**
 * device_handler_get_device - Get a handler device by index.
 */

const struct device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index)
{
	if (index >= handler->n_devices) {
		assert(0 && "bad index");
		return NULL;
	}

	return handler->devices[index];
}

static void setup_device_links(struct discover_context *ctx)
{
	struct link {
		char *env, *dir;
	} *link, links[] = {
		{
			.env = "ID_FS_UUID",
			.dir = "disk/by-uuid"
		},
		{
			.env = "ID_FS_LABEL",
			.dir = "disk/by-label"
		},
		{
			.env = NULL
		}
	};

	for (link = links; link->env; link++) {
		char *enc, *dir, *path;
		const char *value;

		value = event_get_param(ctx->event, link->env);
		if (!value || !*value)
			continue;

		enc = encode_label(ctx, value);
		dir = join_paths(ctx, mount_base(), link->dir);
		path = join_paths(ctx, dir, value);

		if (!pb_mkdir_recursive(dir)) {
			unlink(path);
			if (symlink(ctx->mount_path, path)) {
				pb_log("symlink(%s,%s): %s\n",
						ctx->mount_path, path,
						strerror(errno));
				talloc_free(path);
			} else {
				int i = ctx->n_links++;
				ctx->links = talloc_realloc(ctx,
						ctx->links, char *,
						ctx->n_links);
				ctx->links[i] = path;
			}

		}

		talloc_free(dir);
		talloc_free(enc);
	}
}

static void remove_device_links(struct discover_context *ctx)
{
	int i;

	for (i = 0; i < ctx->n_links; i++)
		unlink(ctx->links[i]);
}

static int mount_device(struct discover_context *ctx)
{
	const char *mountpoint;
	const char *argv[6];

	if (!ctx->mount_path) {
		mountpoint = mountpoint_for_device(ctx->device_path);
		ctx->mount_path = talloc_strdup(ctx, mountpoint);
	}

	if (pb_mkdir_recursive(ctx->mount_path))
		pb_log("couldn't create mount directory %s: %s\n",
				ctx->mount_path, strerror(errno));

	argv[0] = MOUNT_BIN;
	argv[1] = ctx->device_path;
	argv[2] = ctx->mount_path;
	argv[3] = "-o";
	argv[4] = "ro";
	argv[5] = NULL;

	if (pb_run_cmd(argv))
		goto out_rmdir;

	setup_device_links(ctx);
	return 0;

out_rmdir:
	pb_rmdir_recursive(mount_base(), ctx->mount_path);
	return -1;
}

static int umount_device(struct discover_context *ctx)
{
	int status;
	pid_t pid;

	remove_device_links(ctx);

	pid = fork();
	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (pid == 0) {
		execl(UMOUNT_BIN, UMOUNT_BIN, ctx->mount_path, NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		pb_log("%s: waitpid failed: %s\n", __func__,
				strerror(errno));
		return -1;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;

	pb_rmdir_recursive(mount_base(), ctx->mount_path);

	return 0;
}

static struct discover_context *find_context(struct device_handler *handler,
		const char *id)
{
	struct discover_context *ctx;

	list_for_each_entry(&handler->contexts, ctx, list) {
		if (!strcmp(ctx->id, id))
			return ctx;
	}

	return NULL;
}

static int destroy_context(void *arg)
{
	struct discover_context *ctx = arg;

	list_remove(&ctx->list);
	umount_device(ctx);

	return 0;
}

static int handle_add_udev_event(struct device_handler *handler,
		struct event *event)
{
	struct discover_context *ctx;
	const char *devname;
	int rc;

	/* create our context */
	ctx = talloc(handler, struct discover_context);
	ctx->event = event;
	ctx->mount_path = NULL;
	ctx->links = NULL;
	ctx->n_links = 0;

	ctx->id = talloc_strdup(ctx, event->device);

	devname = event_get_param(ctx->event, "DEVNAME");
	if (!devname) {
		pb_log("no devname for %s?\n", event->device);
		return 0;
	}

	ctx->device_path = talloc_strdup(ctx, devname);

	rc = mount_device(ctx);
	if (rc) {
		talloc_free(ctx);
		return 0;
	}

	list_add(&handler->contexts, &ctx->list);
	talloc_set_destructor(ctx, destroy_context);

	/* set up the top-level device */
	ctx->device = talloc_zero(ctx, struct device);
	ctx->device->id = talloc_strdup(ctx->device, ctx->id);
	list_init(&ctx->device->boot_options);

	/* run the parsers */
	iterate_parsers(ctx);

	/* add device to handler device array */
	device_handler_add(handler, ctx->device);

	discover_server_notify_add(handler->server, ctx->device);

	return 0;
}

static int handle_remove_udev_event(struct device_handler *handler,
		struct event *event)
{
	struct discover_context *ctx;

	ctx = find_context(handler, event->device);
	if (!ctx)
		return 0;

	discover_server_notify_remove(handler->server, ctx->device);

	/* remove device from handler device array */
	device_handler_remove(handler, ctx->device);

	talloc_free(ctx);

	return 0;
}

int device_handler_event(struct device_handler *handler,
		struct event *event)
{
	int rc = 0;

	switch (event->type) {
	case EVENT_TYPE_UDEV:
		switch (event->action) {
		case EVENT_ACTION_ADD:
			rc = handle_add_udev_event(handler, event);
			break;
		case EVENT_ACTION_REMOVE:
			rc = handle_remove_udev_event(handler, event);
			break;
		default:
			pb_log("%s unknown action: %d\n", __func__,
				event->action);
			break;
		}
		break;
	case EVENT_TYPE_USER:
		break;
	default:
		pb_log("%s unknown type: %d\n", __func__, event->type);
		break;
	}

	return rc;
}

struct device_handler *device_handler_init(struct discover_server *server)
{
	struct device_handler *handler;

	handler = talloc(NULL, struct device_handler);
	handler->devices = NULL;
	handler->n_devices = 0;
	handler->server = server;

	list_init(&handler->contexts);

	/* set up our mount point base */
	pb_mkdir_recursive(mount_base());

	parser_init();

	return handler;
}

void device_handler_destroy(struct device_handler *handler)
{
	talloc_free(handler);
}
