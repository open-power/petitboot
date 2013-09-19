
#include <log/log.h>
#include <talloc/talloc.h>

#include "pb-config.h"

#include "storage.h"

static struct config		*config;
static struct config_storage	*storage;


static void config_set_defaults(struct config *config)
{
	config->autoboot_enabled = true;
	config->autoboot_timeout_sec = 10;
	config->network.interfaces = NULL;
	config->network.n_interfaces = 0;
	config->network.dns_servers = NULL;
	config->network.n_dns_servers = 0;
}

static void dump_config(struct config *config)
{
	int i;

	pb_log("configuration:\n");

	if (config->autoboot_enabled)
		pb_log(" autoboot: enabled, %d sec\n",
				config->autoboot_timeout_sec);
	else
		pb_log(" autoboot: disabled\n");

	if (config->network.n_interfaces || config->network.n_dns_servers)
		pb_log(" network configuration:\n");

	for (i = 0; i < config->network.n_interfaces; i++) {
		struct interface_config *ifconf =
			config->network.interfaces[i];

		pb_log("  interface %02x:%02x:%02x:%02x:%02x:%02x\n",
				ifconf->hwaddr[0], ifconf->hwaddr[1],
				ifconf->hwaddr[2], ifconf->hwaddr[3],
				ifconf->hwaddr[4], ifconf->hwaddr[5]);

		if (ifconf->ignore) {
			pb_log("   ignore\n");
			continue;
		}

		if (ifconf->method == CONFIG_METHOD_DHCP) {
			pb_log("   dhcp\n");

		} else if (ifconf->method == CONFIG_METHOD_STATIC) {
			pb_log("   static:\n");
			pb_log("    ip:  %s\n", ifconf->static_config.address);
			pb_log("    gw:  %s\n", ifconf->static_config.gateway);

		}
	}
	for (i = 0; i < config->network.n_dns_servers; i++)
		pb_log("  dns server %s\n", config->network.dns_servers[i]);
}

int config_init(void *ctx)
{
	config = talloc(ctx, struct config);
	config_set_defaults(config);

	storage = create_powerpc_nvram_storage(config);

	storage->load(storage, config);

	dump_config(config);

	return 0;
}

const struct config *config_get(void)
{
	return config;
}

void config_set_autoboot(bool autoboot_enabled)
{
	config->autoboot_enabled = autoboot_enabled;

	pb_log("set autoboot: %s\n",
			config->autoboot_enabled ? "enabled" : "disabled");
}

int config_fini(void)
{
	talloc_free(config);
	return 0;
}
