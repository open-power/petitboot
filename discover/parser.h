#ifndef _PARSER_H
#define _PARSER_H

#include <stdbool.h>

struct discover_context;
struct device_handler;
struct resource;

/**
 * Our config parser.
 *
 * Each parser is responsible for creating discover_boot_options from config
 * files found on new devices. The boot items discovered during parse will have
 * 'resources' attached (see @discover_boot_option), which may already be
 * resolved (in the case of a device-local filename, or a URL), or unresolved
 * (in the case of a filename on another device).
 *
 * If the boot option contains references to unresolved resources, the
 * device handler will not inform clients about the boot options, as
 * they're not properly "available" at this stage. The handler will attempt to
 * resolve them whenever new devices are discovered, by calling the parser's
 * resolve_resource function. Once a boot option's resources are full resolved,
 * the option can be sent to clients.
 */
struct parser {
	char			*name;
	const char * const	*filenames;
	int			(*parse)(struct discover_context *ctx,
						char *buf, int len);
	bool			(*resolve_resource)(
						struct device_handler *handler,
						struct resource *res);
};

enum generic_icon_type {
	ICON_TYPE_DISK,
	ICON_TYPE_USB,
	ICON_TYPE_OPTICAL,
	ICON_TYPE_NETWORK,
	ICON_TYPE_UNKNOWN
};

#define streq(a,b) (!strcasecmp((a),(b)))

void parser_init(void);

void iterate_parsers(struct discover_context *ctx);
int parse_user_event(struct discover_context *ctx, struct event *event);

#endif /* _PARSER_H */
