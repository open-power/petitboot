
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <array-size/array-size.h>

#include "pb-config.h"
#include "storage.h"

struct interface_config net1 = {
	.hwaddr = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 },
	.method = CONFIG_METHOD_DHCP,
};

struct interface_config net2 = {
	.hwaddr = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x56 },
	.method = CONFIG_METHOD_STATIC,
	.static_config = {
		.address = "192.168.0.2/24",
		.gateway = "192.168.0.1",
	},
};

struct interface_config *interface_configs[] = { &net1, &net2 };
const char *dns_servers[] = { "192.168.1.1", "192.168.1.2" };

struct config test_config = {
	.autoboot_enabled = true,
	.network = {
		.interfaces = interface_configs,
		.n_interfaces = ARRAY_SIZE(interface_configs),
		.dns_servers = dns_servers,
		.n_dns_servers = ARRAY_SIZE(dns_servers),
	}
};

struct test_storage {
	struct config_storage	storage;
	struct param		*params;
	int			n_params;
};

static int load(struct config_storage *st __attribute__((unused)),
		struct config *config)
{
	memcpy(config, &test_config, sizeof(test_config));
	return 0;
}

static struct test_storage st = {
	.storage = {
		.load  = load,
	},
};

struct config_storage *create_test_storage(void *ctx __attribute__((unused)))
{
	return &st.storage;
}
