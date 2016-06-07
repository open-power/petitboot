
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>

#include "pb-config.h"

static struct interface_config *config_copy_interface(struct config *ctx,
		struct interface_config *src)
{
	struct interface_config *dest = talloc_zero(ctx,
						struct interface_config);

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
		dest->static_config.url =
			talloc_strdup(dest, src->static_config.url);
		break;
	}

	return dest;
}

struct config *config_copy(void *ctx, const struct config *src)
{
	struct config *dest;
	unsigned int i;

	dest = talloc_zero(ctx, struct config);
	dest->autoboot_enabled = src->autoboot_enabled;
	dest->autoboot_timeout_sec = src->autoboot_timeout_sec;
	dest->safe_mode = src->safe_mode;

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

	dest->n_autoboot_opts = src->n_autoboot_opts;
	dest->autoboot_opts = talloc_array(dest, struct autoboot_option,
					dest->n_autoboot_opts);

	for (i = 0; i < src->n_autoboot_opts; i++) {
		dest->autoboot_opts[i].boot_type =
			src->autoboot_opts[i].boot_type;
		if (src->autoboot_opts[i].boot_type == BOOT_DEVICE_TYPE)
			dest->autoboot_opts[i].type =
				src->autoboot_opts[i].type;
		else
			dest->autoboot_opts[i].uuid =
				talloc_strdup(dest, src->autoboot_opts[i].uuid);
	}

	dest->ipmi_bootdev = src->ipmi_bootdev;
	dest->ipmi_bootdev_persistent = src->ipmi_bootdev_persistent;

	dest->allow_writes = src->allow_writes;

	dest->n_tty = src->n_tty;
	if (src->tty_list)
		dest->tty_list = talloc_array(dest, char *, src->n_tty);
	for (i = 0; i < src->n_tty && src->n_tty; i++)
		dest->tty_list[i] = talloc_strdup(dest->tty_list,
						src->tty_list[i]);

	if (src->boot_tty)
		dest->boot_tty = talloc_strdup(dest, src->boot_tty);

	if (src->lang && strlen(src->lang))
		dest->lang = talloc_strdup(dest, src->lang);
	else
		dest->lang = NULL;

	return dest;
}
