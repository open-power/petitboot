
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pb-config.h"
#include "storage.h"

struct network_config net1 = {
	.hwaddr = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 },
	.method = CONFIG_METHOD_DHCP,
};

struct network_config net2 = {
	.hwaddr = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x56 },
	.method = CONFIG_METHOD_STATIC,
	.static_config = {
		.address = "192.168.0.2/24",
		.gateway = "192.168.0.1",
	},
};

struct network_config *network_configs[] = { &net1, &net2 };

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

struct config test_config = {
	.autoboot_enabled = true,
	.network_configs = network_configs,
	.n_network_configs = ARRAY_SIZE(network_configs),
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
