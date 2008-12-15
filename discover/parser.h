
#ifndef _PARSERS_H
#define _PARSERS_H

#include <stdarg.h>
#include "message.h"

struct parser {
	char *name;
	int priority;
	int (*parse)(const char *device);
	struct parser *next;
};

enum generic_icon_type {
	ICON_TYPE_DISK,
	ICON_TYPE_USB,
	ICON_TYPE_OPTICAL,
	ICON_TYPE_NETWORK,
	ICON_TYPE_UNKNOWN
};

#define streq(a,b) (!strcasecmp((a),(b)))

/* general functions provided by parsers.c */
void iterate_parsers(const char *devpath, const char *mountpoint);

void free_device(struct device *dev);
void free_boot_option(struct boot_option *opt);

const char *generic_icon_file(enum generic_icon_type type);

/* functions provided by udev-helper or the test wrapper */
void pb_log(const char *fmt, ...);

int mount_device(const char *dev_path);

char *resolve_path(const char *path, const char *current_dev);
const char *mountpoint_for_device(const char *dev_path);

enum generic_icon_type guess_device_type(void);

int add_device(const struct device *dev);
int add_boot_option(const struct boot_option *opt);

#endif /* _PARSERS_H */
