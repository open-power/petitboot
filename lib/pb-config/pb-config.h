#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>
#include <stdint.h>

#include <types/types.h>

#define HWADDR_SIZE	6

struct interface_config {
	uint8_t	hwaddr[HWADDR_SIZE];
	bool	ignore;
	enum {
		CONFIG_METHOD_DHCP,
		CONFIG_METHOD_STATIC,
	} method;
	union {
		struct {
		} dhcp_config;
		struct {
			char *address;
			char *gateway;
		} static_config;
	};
};

struct network_config {
	struct interface_config	**interfaces;
	int			n_interfaces;
	const char		**dns_servers;
	int			n_dns_servers;
};

struct boot_priority {
	enum device_type	type;
};

struct config {
	bool			autoboot_enabled;
	int			autoboot_timeout_sec;
	struct network_config	network;
	struct boot_priority	*boot_priorities;
	int			n_boot_priorities;
};


int config_init(void *ctx);
const struct config *config_get(void);
void config_set_autoboot(bool autoboot_enabled);
int config_fini(void);

#endif /* CONFIGURATION_H */

