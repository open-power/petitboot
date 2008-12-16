
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <talloc/talloc.h>
#include <pb-protocol/pb-protocol.h>

#include "device-handler.h"
#include "udev.h"
#include "log.h"
#include "paths.h"

#define MOUNT_BIN "/bin/mount"

struct device_handler {
	struct discover_server *server;

	struct device *devices;
	int n_devices;
};

struct discover_context {
	char *device_path;
	char *mount_path;
	struct udev_event *event;
	struct device *device;
	char **links;
	int n_links;
};

struct mount_map {
	char *device_path;
	char *mount_point;
};


static struct boot_option options[] = {
	{
		.id = "1.1",
		.name = "meep one",
		.description = "meep description one",
		.icon_file = "meep.one.png",
		.boot_args = "root=/dev/sda1",
	},
};

static struct device device = {
	.id = "1",
	.name = "meep",
	.description = "meep description",
	.icon_file = "meep.png",
	.n_options = 1,
	.options = options,
};

int device_handler_get_current_devices(struct device_handler *handler,
		struct device **devices)

{
	*devices = &device;
	return 1;
}

static int mkdir_recursive(const char *dir)
{
	struct stat statbuf;
	char *str, *sep;
	int mode = 0755;

	if (!*dir)
		return 0;

	if (!stat(dir, &statbuf)) {
		if (!S_ISDIR(statbuf.st_mode)) {
			pb_log("%s: %s exists, but isn't a directory\n",
					__func__, dir);
			return -1;
		}
		return 0;
	}

	str = talloc_strdup(NULL, dir);
	sep = strchr(*str == '/' ? str + 1 : str, '/');

	while (1) {

		/* terminate the path at sep */
		if (sep)
			*sep = '\0';

		if (mkdir(str, mode) && errno != EEXIST) {
			pb_log("mkdir(%s): %s\n", str, strerror(errno));
			return -1;
		}

		if (!sep)
			break;

		/* reset dir to the full path */
		strcpy(str, dir);
		sep = strchr(sep + 1, '/');
	}

	talloc_free(str);

	return 0;
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

		value = udev_event_param(ctx->event, link->env);
		if (!value)
			continue;

		enc = encode_label(ctx, value);
		dir = join_paths(ctx, mount_base(), link->dir);
		path = join_paths(ctx, dir, value);

		if (!mkdir_recursive(dir)) {
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

static int mount_device(struct discover_context *ctx)
{
	const char *mountpoint;
	struct stat statbuf;
	int status;
	pid_t pid;

	if (!ctx->mount_path) {
		mountpoint = mountpoint_for_device(ctx->device_path);
		ctx->mount_path = talloc_strdup(ctx, mountpoint);
	}

	if (stat(ctx->mount_path, &statbuf)) {
		if (mkdir(ctx->mount_path, 0755)) {
			pb_log("couldn't create mount directory %s: %s\n",
					ctx->mount_path, strerror(errno));
			return -1;
		}
	} else {
		if (!S_ISDIR(statbuf.st_mode)) {
			pb_log("mountpoint %s exists, but isn't a directory\n",
					ctx->mount_path);
			return -1;
		}
	}

	pid = fork();
	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (pid == 0) {
		execl(MOUNT_BIN, MOUNT_BIN, ctx->device_path, ctx->mount_path,
				"-o", "ro", NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		pb_log("%s: waitpid failed: %s\n", __func__,
				strerror(errno));
		return -1;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;

	setup_device_links(ctx);
	return 0;
}

static int handle_add_event(struct device_handler *handler,
		struct udev_event *event)
{
	struct discover_context *ctx;
	const char *devname;
	int rc;

	/* create our context */
	ctx = talloc(NULL, struct discover_context);
	ctx->event = event;
	ctx->mount_path = NULL;
	ctx->links = NULL;
	ctx->n_links = 0;

	devname = udev_event_param(ctx->event, "DEVNAME");
	if (!devname) {
		pb_log("no devname for %s?\n", event->device);
		return 0;
	}

	ctx->device_path = talloc_strdup(ctx, devname);

	rc = mount_device(ctx);
	if (rc) {
		pb_log("mount_device failed for %s\n", event->device);
		talloc_free(ctx);
		return 0;
	}

	talloc_free(ctx);

	return 0;
}

static int handle_remove_event(struct device_handler *handler,
		struct udev_event *event)
{
	return 0;
}

int device_handler_event(struct device_handler *handler,
		struct udev_event *event)
{
	int rc;

	switch (event->action) {
	case UDEV_ACTION_ADD:
		rc = handle_add_event(handler, event);
		break;

	case UDEV_ACTION_REMOVE:
		rc = handle_remove_event(handler, event);
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

	/* set up our mount point base */
	mkdir_recursive(mount_base());

	return handler;
}

void device_handler_destroy(struct device_handler *devices)
{
	talloc_free(devices);
}

