
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "parser.h"

void pb_log(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


int mount_device(const char *dev_path, char *mount_path)
{
	pb_log("attempt to mount device (%s) not supported\n", dev_path);
	return -1;
}

int add_device(const struct device *dev)
{
	printf("device added:\n");
	printf("\tid: %s\n", dev->id);
	printf("\tname: %s\n", dev->name);
	printf("\tdescription: %s\n", dev->description);
	printf("\tboot_image: %s\n", dev->icon_file);
	return 0;
}

int add_boot_option(const struct boot_option *opt)
{
	printf("option added:\n");
	printf("\tname: %s\n", opt->name);
	printf("\tdescription: %s\n", opt->description);
	printf("\tboot_image: %s\n", opt->boot_image_file);
	printf("\tinitrd: %s\n", opt->initrd_file);
	printf("\tboot_args: %s\n", opt->boot_args);
	return 0;
}

enum generic_icon_type guess_device_type(void)
{
	return ICON_TYPE_UNKNOWN;
}

int main(int argc, char **argv)
{
	const char *dev = "/dev/null";

	if (argc != 2) {
		fprintf(stderr, "usage: %s <fake-mountpoint>\n", argv[0]);
		return EXIT_FAILURE;
	}

	iterate_parsers(dev, argv[1]);

	return EXIT_SUCCESS;
}
