#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "paths.h"

static char *mount_base;

struct device_map {
	char *dev, *mnt;
};

#define DEVICE_MAP_SIZE 32
static struct device_map device_map[DEVICE_MAP_SIZE];

static char *encode_label(const char *label)
{
	char *str, *c;
	int i;

	/* the label can be expanded by up to four times */
	str = malloc(strlen(label) * 4 + 1);
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

char *parse_device_path(const char *dev_str, const char *cur_dev)
{
	char *dev, tmp[256], *enc;

	if (!strncasecmp(dev_str, "uuid=", 5)) {
		asprintf(&dev, "/dev/disk/by-uuid/%s", dev_str + 5);
		return dev;
	}

	if (!strncasecmp(dev_str, "label=", 6)) {
		enc = encode_label(dev_str + 6);
		asprintf(&dev, "/dev/disk/by-label/%s", enc);
		free(enc);
		return dev;
	}

	/* normalise '/dev/foo' to 'foo' for easy comparisons, we'll expand
	 * back before returning.
	 */
	if (!strncmp(dev_str, "/dev/", 5))
		dev_str += 5;

	/* PS3 hack: if we're reading from a ps3dx device, and we refer to
	 * a sdx device, remap to ps3dx */
	if (cur_dev && !strncmp(cur_dev, "/dev/ps3d", 9)
			&& !strncmp(dev_str, "sd", 2)) {
		snprintf(tmp, 255, "ps3d%s", dev_str + 2);
		dev_str = tmp;
	}

	return join_paths("/dev", dev_str);
}

const char *mountpoint_for_device(const char *dev)
{
	int i;

	if (!strncmp(dev, "/dev/", 5))
		dev += 5;

	/* check existing entries in the map */
	for (i = 0; (i < DEVICE_MAP_SIZE) && device_map[i].dev; i++)
		if (!strcmp(device_map[i].dev, dev))
			return device_map[i].mnt;

	if (i == DEVICE_MAP_SIZE)
		return NULL;

	device_map[i].dev = strdup(dev);
	device_map[i].mnt = join_paths(mount_base, dev);
	return device_map[i].mnt;
}

char *resolve_path(const char *path, const char *current_dev)
{
	char *ret;
	const char *devpath, *sep;

	sep = strchr(path, ':');
	if (!sep) {
		devpath = mountpoint_for_device(current_dev);
		ret = join_paths(devpath, path);
	} else {
		/* parse just the device name into dev */
		char *tmp, *dev;
		tmp = strndup(path, sep - path);
		dev = parse_device_path(tmp, current_dev);

		devpath = mountpoint_for_device(dev);
		ret = join_paths(devpath, sep + 1);

		free(dev);
		free(tmp);
	}

	return ret;
}

void set_mount_base(const char *path)
{
	if (mount_base)
		free(mount_base);
	mount_base = strdup(path);
}

char *join_paths(const char *a, const char *b)
{
	char *full_path;

	full_path = malloc(strlen(a) + strlen(b) + 2);

	strcpy(full_path, a);
	if (b[0] != '/' && a[strlen(a) - 1] != '/')
		strcat(full_path, "/");
	strcat(full_path, b);

	return full_path;
}

