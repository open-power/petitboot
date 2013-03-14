#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdbool.h>

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

#endif /* RESOURCE_H */

