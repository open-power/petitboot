
#include "message.h"

int add_device(const struct device *dev);

int add_boot_option(const struct boot_option *opt);
void free_boot_option(struct boot_option *opt);

int mount_device(const char *dev_path, char *mount_path);

struct parser {
	char *name;
	int priority;
	int (*parse)(const char *devicepath, const char *mountpoint);
	struct parser *next;
};

enum generic_icon_type {
	ICON_TYPE_DISK,
	ICON_TYPE_USB,
	ICON_TYPE_OPTICAL,
	ICON_TYPE_NETWORK,
	ICON_TYPE_UNKNOWN
};

enum generic_icon_type guess_device_type(void);
const char *generic_icon_file(enum generic_icon_type type);

#define streq(a,b) (!strcasecmp((a),(b)))
