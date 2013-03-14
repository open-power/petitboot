
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
#include <types/types.h>
#include <system/system.h>

#include "device-handler.h"
#include "discover-server.h"
#include "event.h"
#include "parser.h"
#include "udev.h"
#include "paths.h"
#include "boot.h"

struct device_handler {
	struct discover_server	*server;
	int			dry_run;

	struct discover_device	**devices;
	unsigned int		n_devices;
};

/**
 * context_commit - Commit a temporary discovery context to the handler,
 * and notify the clients about any new options / devices
 */
static void context_commit(struct device_handler *handler,
		struct discover_context *ctx)
{
	struct discover_device *dev = ctx->device;
	struct discover_boot_option *opt, *tmp;
	unsigned int i, existing_device;

	/* do we already have this device? */
	for (i = 0; i < handler->n_devices; i++) {
		if (ctx->device == handler->devices[i]) {
			existing_device = 1;
			break;
		}
	}

	/* if not already present, add the device to the handler's array */
	if (!existing_device) {
		handler->n_devices++;
		handler->devices = talloc_realloc(handler, handler->devices,
			struct discover_device *, handler->n_devices);
		handler->devices[handler->n_devices - 1] = dev;
		talloc_steal(handler, dev);

		discover_server_notify_device_add(handler->server, dev->device);
	}


	/* move boot options from the context to the device */
	list_for_each_entry_safe(&ctx->boot_options, opt, tmp, list) {
		list_remove(&opt->list);
		list_add(&dev->boot_options, &opt->list);
		talloc_steal(dev, opt);
		discover_server_notify_boot_option_add(handler->server,
							opt->option);
	}
}

void discover_context_add_boot_option(struct discover_context *ctx,
		struct discover_boot_option *boot_option)
{
	list_add(&ctx->boot_options, &boot_option->list);
	talloc_steal(ctx, boot_option);
}

/**
 * device_handler_remove - Remove a device from the handler device array.
 */

static void device_handler_remove(struct device_handler *handler,
	struct discover_device *device)
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
		struct discover_device *, handler->n_devices);

	discover_server_notify_device_remove(handler->server, device->device);

	talloc_free(device);
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

const struct discover_device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index)
{
	if (index >= handler->n_devices) {
		assert(0 && "bad index");
		return NULL;
	}

	return handler->devices[index];
}

static void setup_device_links(struct discover_device *dev)
{
	struct link {
		const char *dir, *val;
	} *link, links[] = {
		{
			.dir = "disk/by-uuid",
			.val = dev->uuid,
		},
		{
			.dir = "disk/by-label",
			.val = dev->label,
		},
		{
			.dir = NULL
		}
	};

	for (link = links; link->dir; link++) {
		char *enc, *dir, *path;

		if (!link->val || !*link->val)
			continue;

		enc = encode_label(dev, link->val);
		dir = join_paths(dev, mount_base(), link->dir);
		path = join_paths(dev, dir, enc);

		if (!pb_mkdir_recursive(dir)) {
			unlink(path);
			if (symlink(dev->mount_path, path)) {
				pb_log("symlink(%s,%s): %s\n",
						dev->mount_path, path,
						strerror(errno));
				talloc_free(path);
			} else {
				int i = dev->n_links++;
				dev->links = talloc_realloc(dev,
						dev->links, char *,
						dev->n_links);
				dev->links[i] = path;
			}

		}

		talloc_free(dir);
		talloc_free(enc);
	}
}

static void remove_device_links(struct discover_device *dev)
{
	int i;

	for (i = 0; i < dev->n_links; i++)
		unlink(dev->links[i]);
}

static int mount_device(struct discover_device *dev)
{
	const char *mountpoint;
	const char *argv[6];

	if (!dev->mount_path) {
		mountpoint = mountpoint_for_device(dev->device_path);
		dev->mount_path = talloc_strdup(dev, mountpoint);
	}

	if (pb_mkdir_recursive(dev->mount_path))
		pb_log("couldn't create mount directory %s: %s\n",
				dev->mount_path, strerror(errno));

	argv[0] = pb_system_apps.mount;
	argv[1] = dev->device_path;
	argv[2] = dev->mount_path;
	argv[3] = "-o";
	argv[4] = "ro";
	argv[5] = NULL;

	if (pb_run_cmd(argv, 1, 0)) {

		/* Retry mount without ro option. */

		argv[0] = pb_system_apps.mount;
		argv[1] = dev->device_path;
		argv[2] = dev->mount_path;
		argv[3] = NULL;

		if (pb_run_cmd(argv, 1, 0))
			goto out_rmdir;
	}

	setup_device_links(dev);
	return 0;

out_rmdir:
	pb_rmdir_recursive(mount_base(), dev->mount_path);
	return -1;
}

static int umount_device(struct discover_device *dev)
{
	int status;
	pid_t pid;

	remove_device_links(dev);

	if (!dev->mount_path)
		return 0;

	pid = fork();
	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (pid == 0) {
		execl(pb_system_apps.umount, pb_system_apps.umount,
						dev->mount_path, NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		pb_log("%s: waitpid failed: %s\n", __func__,
				strerror(errno));
		return -1;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;

	pb_rmdir_recursive(mount_base(), dev->mount_path);

	return 0;
}

static struct discover_device *find_device(struct device_handler *handler,
		const char *id)
{
	struct discover_device *dev;
	unsigned int i;

	for (i = 0; i < handler->n_devices; i++) {
		dev = handler->devices[i];
		if (!strcmp(dev->device->id, id))
			return dev;
	}

	return NULL;
}

static int destroy_device(void *arg)
{
	struct discover_device *dev = arg;

	umount_device(dev);

	return 0;
}

static struct discover_device *discover_device_create(
		struct device_handler *handler,
		struct discover_context *ctx,
		struct event *event)
{
	struct discover_device *dev;
	const char *devname;

	dev = find_device(handler, event->device);
	if (dev)
		return dev;

	dev = talloc_zero(ctx, struct discover_device);
	dev->device = talloc_zero(dev, struct device);
	list_init(&dev->boot_options);

	devname = event_get_param(ctx->event, "DEVNAME");
	if (devname)
		dev->device_path = talloc_strdup(dev, devname);

	dev->device->id = talloc_strdup(dev, event->device);

	talloc_set_destructor(dev, destroy_device);

	return dev;
}

struct discover_boot_option *discover_boot_option_create(
		struct discover_context *ctx,
		struct discover_device *device)
{
	struct discover_boot_option *opt;

	opt = talloc_zero(ctx, struct discover_boot_option);
	opt->option = talloc_zero(opt, struct boot_option);
	opt->device = device;

	return opt;
}


static int handle_add_udev_event(struct device_handler *handler,
		struct event *event)
{
	struct discover_context *ctx;
	struct discover_device *dev;
	const char *param;
	int rc;

	/* create our context */
	ctx = talloc(handler, struct discover_context);
	ctx->event = event;
	list_init(&ctx->boot_options);

	/* create our top-level device */
	dev = discover_device_create(handler, ctx, event);

	ctx->device = dev;

	/* try to parse UUID and labels */
	param = event_get_param(ctx->event, "ID_FS_UUID");
	if (param)
		dev->uuid = talloc_strdup(dev, param);

	param = event_get_param(ctx->event, "ID_FS_LABEL");
	if (param)
		dev->label = talloc_strdup(dev, param);

	rc = mount_device(dev);
	if (rc) {
		talloc_free(ctx);
		return 0;
	}

	/* run the parsers. This will populate the ctx's boot_option list. */
	iterate_parsers(ctx);

	/* add discovered stuff to the handler */
	context_commit(handler, ctx);

	talloc_free(ctx);

	return 0;
}

static int handle_remove_udev_event(struct device_handler *handler,
		struct event *event)
{
	struct discover_device *dev;

	dev = find_device(handler, event->device);
	if (!dev)
		return 0;

	/* remove device from handler device array */
	device_handler_remove(handler, dev);

	return 0;
}

static int handle_add_user_event(struct device_handler *handler,
		struct event *event)
{
	struct discover_context *ctx;
	struct discover_device *dev;
	int rc;

	assert(event->device);

	ctx = talloc(handler, struct discover_context);
	ctx->event = event;
	list_init(&ctx->boot_options);

	dev = discover_device_create(handler, ctx, event);
	ctx->device = dev;

	rc = parse_user_event(ctx, event);

	if (!rc)
		context_commit(handler, ctx);

	return rc;
}

static int handle_remove_user_event(struct device_handler *handler,
		struct event *event)
{
	struct discover_device *dev = find_device(handler, event->device);

	if (!dev)
		return 0;

	/* remove device from handler device array */
	device_handler_remove(handler, dev);

	return 0;
}

typedef int (*event_handler)(struct device_handler *, struct event *);

static event_handler handlers[EVENT_TYPE_MAX][EVENT_ACTION_MAX] = {
	[EVENT_TYPE_UDEV] = {
		[EVENT_ACTION_ADD]	= handle_add_udev_event,
		[EVENT_ACTION_REMOVE]	= handle_remove_udev_event,
	},
	[EVENT_TYPE_USER] = {
		[EVENT_ACTION_ADD]	= handle_add_user_event,
		[EVENT_ACTION_REMOVE]	= handle_remove_user_event,
	}
};

int device_handler_event(struct device_handler *handler,
		struct event *event)
{
	if (event->type >= EVENT_TYPE_MAX ||
			event->action >= EVENT_ACTION_MAX ||
			!handlers[event->type][event->action]) {
		pb_log("%s unknown type/action: %d/%d\n", __func__,
				event->type, event->action);
		return 0;
	}

	return handlers[event->type][event->action](handler, event);
}

struct device_handler *device_handler_init(struct discover_server *server,
		int dry_run)
{
	struct device_handler *handler;

	handler = talloc(NULL, struct device_handler);
	handler->devices = NULL;
	handler->n_devices = 0;
	handler->server = server;
	handler->dry_run = dry_run;

	/* set up our mount point base */
	pb_mkdir_recursive(mount_base());

	parser_init();

	return handler;
}

void device_handler_destroy(struct device_handler *handler)
{
	talloc_free(handler);
}

static int device_match_path(struct discover_device *dev, const char *path)
{
	return !strcmp(dev->device_path, path);
}

static int device_match_uuid(struct discover_device *dev, const char *uuid)
{
	return dev->uuid && !strcmp(dev->uuid, uuid);
}

static int device_match_label(struct discover_device *dev, const char *label)
{
	return dev->label && !strcmp(dev->label, label);
}

static int device_match_id(struct discover_device *dev, const char *id)
{
	return !strcmp(dev->device->id, id);
}

static struct discover_device *device_lookup(
		struct device_handler *device_handler,
		int (match_fn)(struct discover_device *, const char *),
		const char *str)
{
	struct discover_device *dev;
	unsigned int i;

	if (!str)
		return NULL;

	for (i = 0; i < device_handler->n_devices; i++) {
		dev = device_handler->devices[i];

		if (match_fn(dev, str))
			return dev;
	}

	return NULL;
}

struct discover_device *device_lookup_by_name(struct device_handler *handler,
		const char *name)
{
	struct discover_device *dev;
	char *path;

	if (strncmp(name, "/dev/", strlen("/dev/")))
		path = talloc_asprintf(NULL, "/dev/%s", name);
	else
		path = talloc_strdup(NULL, name);

	dev = device_lookup_by_path(handler, path);

	talloc_free(path);

	return dev;
}

struct discover_device *device_lookup_by_path(
		struct device_handler *device_handler,
		const char *path)
{
	return device_lookup(device_handler, device_match_path, path);
}

struct discover_device *device_lookup_by_uuid(
		struct device_handler *device_handler,
		const char *uuid)
{
	return device_lookup(device_handler, device_match_uuid, uuid);
}

struct discover_device *device_lookup_by_label(
		struct device_handler *device_handler,
		const char *label)
{
	return device_lookup(device_handler, device_match_label, label);
}

struct discover_device *device_lookup_by_id(
		struct device_handler *device_handler,
		const char *id)
{
	return device_lookup(device_handler, device_match_id, id);
}

static struct discover_boot_option *find_boot_option_by_id(
		struct device_handler *handler, const char *id)
{
	unsigned int i;

	for (i = 0; i < handler->n_devices; i++) {
		struct discover_device *dev = handler->devices[i];
		struct discover_boot_option *opt;

		list_for_each_entry(&dev->boot_options, opt, list)
			if (!strcmp(opt->option->id, id))
				return opt;
	}

	return NULL;
}

void device_handler_boot(struct device_handler *handler,
		struct boot_command *cmd)
{
	struct discover_boot_option *opt;

	opt = find_boot_option_by_id(handler, cmd->option_id);

	boot(handler, opt->option, cmd, handler->dry_run);
}
