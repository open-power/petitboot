#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <talloc/talloc.h>

#include "paths.h"

#define DEVICE_MOUNT_BASE (LOCAL_STATE_DIR "/petitboot/mnt")

struct mount_map {
	char *dev, *mnt;
};

static struct mount_map *mount_map;
static int mount_map_size;

static int is_prefix(const char *str, const char *prefix)
{
	return !strncmp(str, prefix, strlen(prefix));
}

static int is_prefix_ignorecase(const char *str, const char *prefix)
{
	return !strncasecmp(str, prefix, strlen(prefix));
}

const char *mount_base(void)
{
	return DEVICE_MOUNT_BASE;
}

char *encode_label(void *alloc_ctx, const char *label)
{
	char *str, *c;
	int i;

	/* the label can be expanded by up to four times */
	str = talloc_size(alloc_ctx, strlen(label) * 4 + 1);
	c = str;

	for (i = 0; i < strlen(label); i++) {

		if (label[i] == '/' || label[i] == '\\') {
			sprintf(c, "\\x%02x", label[i]);
			c += 4;
			continue;
		}

		*(c++) = label[i];
	}

	*c = '\0';

	return str;
}

char *parse_device_path(void *alloc_ctx,
		const char *dev_str, const char *cur_dev)
{
	char *dev, tmp[256], *enc;

	if (is_prefix_ignorecase(dev_str, "uuid=")) {
		dev = talloc_asprintf(alloc_ctx, "/dev/disk/by-uuid/%s",
				dev_str + strlen("uuid="));
		return dev;
	}

	if (is_prefix_ignorecase(dev_str, "label=")) {
		enc = encode_label(NULL, dev_str + strlen("label="));
		dev = talloc_asprintf(alloc_ctx, "/dev/disk/by-label/%s", enc);
		talloc_free(enc);
		return dev;
	}

	/* normalise '/dev/foo' to 'foo' for easy comparisons, we'll expand
	 * back before returning.
	 */
	if (is_prefix(dev_str, "/dev/"))
		dev_str += strlen("/dev/");

	/* PS3 hack: if we're reading from a ps3dx device, and we refer to
	 * a sdx device, remap to ps3dx */
	if (cur_dev && is_prefix(cur_dev, "/dev/ps3d")
			&& is_prefix(dev_str, "sd")) {
		snprintf(tmp, 255, "ps3d%s", dev_str + 2);
		dev_str = tmp;
	}

	return join_paths(alloc_ctx, "/dev", dev_str);
}

const char *mountpoint_for_device(const char *dev)
{
	int i;

	if (is_prefix(dev, "/dev/"))
		dev += strlen("/dev/");

	/* check existing entries in the map */
	for (i = 0; i < mount_map_size; i++)
		if (!strcmp(mount_map[i].dev, dev))
			return mount_map[i].mnt;

	/* no existing entry, create a new one */
	i = mount_map_size++;
	mount_map = talloc_realloc(NULL, mount_map,
			struct mount_map, mount_map_size);

	mount_map[i].dev = talloc_strdup(mount_map, dev);
	mount_map[i].mnt = join_paths(mount_map, DEVICE_MOUNT_BASE, dev);
	return mount_map[i].mnt;
}

char *resolve_path(void *alloc_ctx, const char *path, const char *current_dev)
{
	char *ret;
	const char *devpath, *sep;

	sep = strchr(path, ':');
	if (!sep) {
		devpath = mountpoint_for_device(current_dev);
		ret = join_paths(alloc_ctx, devpath, path);
	} else {
		/* parse just the device name into dev */
		char *tmp, *dev;
		tmp = talloc_strndup(NULL, path, sep - path);
		dev = parse_device_path(NULL, tmp, current_dev);

		devpath = mountpoint_for_device(dev);
		ret = join_paths(alloc_ctx, devpath, sep + 1);

		talloc_free(dev);
		talloc_free(tmp);
	}

	return ret;
}

char *join_paths(void *alloc_ctx, const char *a, const char *b)
{
	char *full_path;

	full_path = talloc_array(alloc_ctx, char, strlen(a) + strlen(b) + 2);

	strcpy(full_path, a);
	if (b[0] != '/' && a[strlen(a) - 1] != '/')
		strcat(full_path, "/");
	strcat(full_path, b);

	return full_path;
}

