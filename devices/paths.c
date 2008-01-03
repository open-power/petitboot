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

const char *mountpoint_for_device(const char *dev_path)
{
	int i;
	const char *basename;

	/* shorten '/dev/foo' to 'foo' */
	basename = strrchr(dev_path, '/');
	if (basename)
		basename++;
	else
		basename = dev_path;

	/* check existing entries in the map */
	for (i = 0; (i < DEVICE_MAP_SIZE) && device_map[i].dev; i++)
		if (!strcmp(device_map[i].dev, basename))
			return device_map[i].mnt;

	if (i == DEVICE_MAP_SIZE)
		return NULL;

	device_map[i].dev = strdup(dev_path);
	asprintf(&device_map[i].mnt, "%s/%s", mount_base, basename);
	return device_map[i].mnt;
}

char *resolve_path(const char *path, const char *current_mountpoint)
{
	char *ret;
	const char *devpath, *sep;

	sep = strchr(path, ':');
	if (!sep) {
		devpath = current_mountpoint;
		asprintf(&ret, "%s/%s", devpath, path);
	} else {
		/* copy just the device name into tmp */
		char *dev = strndup(path, sep - path);
		devpath = mountpoint_for_device(dev);
		asprintf(&ret, "%s/%s", devpath, sep + 1);
		free(dev);
	}

	return ret;
}

void set_mount_base(const char *path)
{
	if (mount_base)
		free(mount_base);
	mount_base = strdup(path);
}

