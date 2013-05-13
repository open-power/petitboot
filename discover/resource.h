#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdbool.h>

struct discover_boot_option;
struct discover_device;
struct device_handler;
struct pb_url;

/**
 * Data for local/remote resources. Resources may be "unresolved", in that
 * they refer to a device that is not yet present. Unresolved resources
 * simply contain parser-specific data (generally a device string parsed from
 * the config file), and may be resolved by the parser once new devices appear.
 */
struct resource {
	bool resolved;
	union {
		struct pb_url	*url;
		void		*info;
	};
};

/**
 * devpath resources.
 *
 * Most resources in config files will be in one of the following formats:
 *  - URLs
 *  - device-local filenames (ie, filenames on the currently-discovered dev)
 *  - other-device filenames (which speficy the device by a string format,
 *     using a dev:path format).
 *
 * The following definitions are a generic resource handler for these types
 * of resources. By creating resources with create_devpath_resource,
 * parsers can use resolve_devpath_resource as their resolve_resouce
 * callback.
 */

struct resource *create_devpath_resource(struct discover_boot_option *opt,
		struct discover_device *orig_device,
		const char *devpath);

struct resource *create_url_resource(struct discover_boot_option *opt,
		struct pb_url *url);

bool resolve_devpath_resource(struct device_handler *dev,
		struct resource *res);



#endif /* RESOURCE_H */

