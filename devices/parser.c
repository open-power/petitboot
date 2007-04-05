
#include <petitboot-paths.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

extern struct parser native_parser;
extern struct parser yaboot_parser;
extern struct parser kboot_parser;

/* array of parsers, ordered by priority */
static struct parser *parsers[] = {
	&native_parser,
	&yaboot_parser,
	&kboot_parser,
	NULL
};

void iterate_parsers(const char *devpath, const char *mountpoint)
{
	int i;

	pb_log("trying parsers for %s@%s\n", devpath, mountpoint);

	for (i = 0; parsers[i]; i++) {
		pb_log("\ttrying parser '%s'\n", parsers[i]->name);
		/* just use a dummy device path for now */
		if (parsers[i]->parse(devpath, mountpoint))
			/*return*/;
	}
	pb_log("\tno boot_options found\n");
}

/* convenience functions for parsers */
void free_device(struct device *dev)
{
	if (!dev)
		return;
	if (dev->id)
		free(dev->id);
	if (dev->name)
		free(dev->name);
	if (dev->description)
		free(dev->description);
	if (dev->icon_file)
		free(dev->icon_file);
	free(dev);
}

void free_boot_option(struct boot_option *opt)
{
	if (!opt)
		return;
	if (opt->name)
		free(opt->name);
	if (opt->description)
		free(opt->description);
	if (opt->icon_file)
		free(opt->icon_file);
	if (opt->boot_image_file)
		free(opt->boot_image_file);
	if (opt->initrd_file)
		free(opt->initrd_file);
	if (opt->boot_args)
		free(opt->boot_args);
	free(opt);
}

char *join_paths(const char *a, const char *b)
{
	char *full_path;

	full_path = malloc(strlen(a) + strlen(b) + 2);

	strcpy(full_path, a);
	if (b[0] != '/')
		strcat(full_path, "/");
	strcat(full_path, b);

	return full_path;
}

const char *generic_icon_file(enum generic_icon_type type)
{
	switch (type) {
	case ICON_TYPE_DISK:
		return artwork_pathname("hdd.png");
	case ICON_TYPE_USB:
		return artwork_pathname("usbpen.png");
	case ICON_TYPE_OPTICAL:
		return artwork_pathname("cdrom.png");
	case ICON_TYPE_NETWORK:
	case ICON_TYPE_UNKNOWN:
		break;
	}
	return artwork_pathname("hdd.png");
}

