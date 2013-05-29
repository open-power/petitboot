
#include <log/log.h>
#include <talloc/talloc.h>

#include "pb-config.h"

#include "storage.h"

static struct config		*config;
static struct config_storage	*storage;


static void config_set_defaults(struct config *config)
{
	config->autoboot_enabled = true;
	config->network_configs = NULL;
	config->n_network_configs = 0;
}

static void dump_config(struct config *config)
{
	int i;

	pb_log("configuration:\n");

	pb_log(" autoboot enabled: %s\n",
			config->autoboot_enabled ? "yes" : "no");

	if (config->n_network_configs > 0)
		pb_log(" network configuration:\n");

	for (i = 0; i < config->n_network_configs; i++) {
		struct network_config *netconf = config->network_configs[i];

		pb_log("  interface %02x:%02x:%02x:%02x:%02x:%02x\n",
				netconf->hwaddr[0], netconf->hwaddr[1],
				netconf->hwaddr[2], netconf->hwaddr[3],
				netconf->hwaddr[4], netconf->hwaddr[5]);

		if (netconf->ignore) {
			pb_log("   ignore\n");
			continue;
		}

		if (netconf->method == CONFIG_METHOD_DHCP) {
			pb_log("   dhcp\n");

		} else if (netconf->method == CONFIG_METHOD_STATIC) {
			pb_log("   static:\n");
			pb_log("    ip:  %s\n", netconf->static_config.address);
			pb_log("    gw:  %s\n", netconf->static_config.gateway);
			pb_log("    dns: %s\n", netconf->static_config.dns);

		}
	}
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

int config_fini(void)
{
	talloc_free(config);
	return 0;
}
