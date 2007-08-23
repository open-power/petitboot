#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "parser.h"

void pb_log(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

int mount_device(const char *dev_path)
{
	printf("[mount] %s\n", dev_path);
	return 0;
}

static int device_idx;
static int option_idx;

int add_device(const struct device *dev)
{
	printf("[dev %2d] id: %s\n", device_idx, dev->id);
	printf("[dev %2d] name: %s\n", device_idx, dev->name);
	printf("[dev %2d] description: %s\n", device_idx, dev->description);
	printf("[dev %2d] boot_image: %s\n", device_idx, dev->icon_file);

	device_idx++;
	option_idx = 0;
	return 0;
}


int add_boot_option(const struct boot_option *opt)
{
	if (!device_idx) {
		fprintf(stderr, "Option (%s) added before device\n",
				opt->name);
		exit(EXIT_FAILURE);
	}

	printf("[opt %2d] name: %s\n", option_idx, opt->name);
	printf("[opt %2d] description: %s\n", option_idx, opt->description);
	printf("[opt %2d] boot_image: %s\n", option_idx, opt->boot_image_file);
	printf("[opt %2d] initrd: %s\n", option_idx, opt->initrd_file);
	printf("[opt %2d] boot_args: %s\n", option_idx, opt->boot_args);

	option_idx++;

	return 0;
}

enum generic_icon_type guess_device_type(void)
{
	return ICON_TYPE_UNKNOWN;
}

static char *mountpoint;

/* pretend that all devices are mounted at our original mountpoint */
const char *mountpoint_for_device(const char *dev_path)
{
	return mountpoint;
}

char *resolve_path(const char *path, const char *default_mountpoint)
{
	char *sep, *ret;
	const char *devpath;

	sep = strchr(path, ':');
	if (!sep) {
		devpath = default_mountpoint;
		asprintf(&ret, "%s/%s", devpath, path);
	} else {
		char *tmp = strndup(path, sep - path);
		devpath = mountpoint_for_device(path);
		asprintf(&ret, "%s/%s", devpath, sep + 1);
		free(tmp);
	}

	return ret;
}

int main(int argc, char **argv)
{
	const char *dev = "sda1";

	if (argc != 2) {
		fprintf(stderr, "usage: %s <fake-mountpoint>\n", argv[0]);
		return EXIT_FAILURE;
	}

	mountpoint = argv[1];

	iterate_parsers(dev, mountpoint);

	return EXIT_SUCCESS;
}
