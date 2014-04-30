
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>

#include "pb-config.h"

static struct interface_config *config_copy_interface(struct config *ctx,
		struct interface_config *src)
{
	struct interface_config *dest = talloc(ctx, struct interface_config);

	memcpy(dest->hwaddr, src->hwaddr, sizeof(src->hwaddr));
	dest->ignore = src->ignore;

	if (dest->ignore)
		return dest;

	dest->method = src->method;

	switch (src->method) {
	case CONFIG_METHOD_DHCP:
		break;
	case CONFIG_METHOD_STATIC:
		dest->static_config.address =
			talloc_strdup(dest, src->static_config.address);
		dest->static_config.gateway =
			talloc_strdup(dest, src->static_config.gateway);
		break;
	}

	return dest;
}

struct config *config_copy(void *ctx, const struct config *src)
{
	struct config *dest;
	unsigned int i;

	dest = talloc(ctx, struct config);
	dest->autoboot_enabled = src->autoboot_enabled;
	dest->autoboot_timeout_sec = src->autoboot_timeout_sec;

	dest->network.n_interfaces = src->network.n_interfaces;
	dest->network.interfaces = talloc_array(dest, struct interface_config *,
					dest->network.n_interfaces);
	dest->network.n_dns_servers = src->network.n_dns_servers;
	dest->network.dns_servers = talloc_array(dest, const char *,
					dest->network.n_dns_servers);

	for (i = 0; i < src->network.n_interfaces; i++)
		dest->network.interfaces[i] = config_copy_interface(dest,
				src->network.interfaces[i]);

	for (i = 0; i < src->network.n_dns_servers; i++)
		dest->network.dns_servers[i] = talloc_strdup(dest,
				src->network.dns_servers[i]);

	dest->n_boot_priorities = src->n_boot_priorities;
	dest->boot_priorities = talloc_array(dest, struct boot_priority,
			src->n_boot_priorities);

	for (i = 0; i < src->n_boot_priorities; i++) {
		dest->boot_priorities[i].priority =
					src->boot_priorities[i].priority;
		dest->boot_priorities[i].type = src->boot_priorities[i].type;
	}

	if (src->boot_device && strlen(src->boot_device))
		dest->boot_device = talloc_strdup(dest, src->boot_device);
	else
		dest->boot_device = NULL;

	return dest;
}
