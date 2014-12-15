
#define _GNU_SOURCE

#include <fcntl.h>
#include <string.h>

#include <log/log.h>
#include <file/file.h>
#include <types/types.h>
#include <talloc/talloc.h>

#include "platform.h"

void			*platform_ctx;
static struct platform	*platform;
static struct config	*config;

static const char *kernel_cmdline_debug = "petitboot.debug";

static void dump_config(struct config *config)
{
	unsigned int i;

	pb_log("configuration:\n");

	if (config->autoboot_enabled)
		pb_log(" autoboot: enabled, %d sec\n",
				config->autoboot_timeout_sec);
	else
		pb_log(" autoboot: disabled\n");

	if (config->network.n_interfaces || config->network.n_dns_servers)
		pb_log(" network configuration:\n");

	if (config->safe_mode)
		pb_log(" safe mode: active\n");

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

	if (config->boot_device)
		pb_log("  boot device %s\n", config->boot_device);

	if (config->n_boot_priorities)
		pb_log(" boot priority order:\n");

	for (i = 0; i < config->n_boot_priorities; i++) {
		struct boot_priority *prio = &config->boot_priorities[i];
		pb_log(" %10s: %d\n", device_type_name(prio->type),
					prio->priority);
	}

	pb_log("  IPMI boot device 0x%02x%s\n", config->ipmi_bootdev,
			config->ipmi_bootdev_persistent ? " (persistent)" : "");


	pb_log(" language: %s\n", config->lang ?: "");
}

static bool config_debug_on_cmdline(void)
{
	char buf[600];
	int rc, fd;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0)
		return false;

	rc = read(fd, buf, sizeof(buf));
	close(fd);

	if (rc <= 0)
		return false;

	return memmem(buf, rc, kernel_cmdline_debug,
			strlen(kernel_cmdline_debug)) != NULL;
}

void config_set_defaults(struct config *config)
{
	config->autoboot_enabled = true;
	config->autoboot_timeout_sec = 10;
	config->autoboot_enabled = true;
	config->network.interfaces = NULL;
	config->network.n_interfaces = 0;
	config->network.dns_servers = NULL;
	config->network.n_dns_servers = 0;
	config->boot_device = NULL;
	config->safe_mode = false;
	config->lang = NULL;

	config->n_boot_priorities = 2;
	config->boot_priorities = talloc_array(config, struct boot_priority,
						config->n_boot_priorities);
	config->boot_priorities[0].type = DEVICE_TYPE_NETWORK;
	config->boot_priorities[0].priority = 0;
	config->boot_priorities[1].type = DEVICE_TYPE_ANY;
	config->boot_priorities[1].priority = 1;

	config->ipmi_bootdev = 0;
	config->ipmi_bootdev_persistent = false;

	config->debug = config_debug_on_cmdline();
}

int platform_init(void *ctx)
{
	extern struct platform *__start_platforms,  *__stop_platforms;
	struct platform **p;

	platform_ctx = talloc_new(ctx);

	for (p = &__start_platforms; p < &__stop_platforms; p++) {
		if (!(*p)->probe(*p, platform_ctx))
			continue;
		platform = *p;
		break;
	}

	config = talloc(platform_ctx, struct config);
	config_set_defaults(config);

	if (platform) {
		pb_log("Detected platform type: %s\n", platform->name);
		if (platform->load_config)
			platform->load_config(platform, config);
	} else {
		pb_log("No platform type detected, some platform-specific "
				"functionality will be disabled\n");
	}

	dump_config(config);

	return 0;
}

const struct platform *platform_get(void)
{
	return platform;
}

void platform_pre_boot(void)
{
	const struct config *config = config_get();

	if (platform && config && platform->pre_boot)
		platform->pre_boot(platform, config);
}

int platform_get_sysinfo(struct system_info *info)
{
	if (platform && platform->get_sysinfo)
		return platform->get_sysinfo(platform, info);
	return -1;
}

int config_set(struct config *newconfig)
{
	int rc;

	if (!platform || !platform->save_config)
		return -1;

	if (newconfig == config)
		return 0;

	pb_log("new configuration data received\n");
	dump_config(newconfig);

	rc = platform->save_config(platform, newconfig);

	if (!rc)
		config = talloc_steal(platform_ctx, newconfig);
	else
		pb_log("error saving new configuration; changes lost\n");

	return rc;
}

/* A non-exported function to allow the test infrastructure to initialise
 * (and change) the configuration variables */
struct parser_test;
struct config __attribute__((unused)) *test_config_init(
		struct parser_test *test);
struct config *test_config_init(struct parser_test *test)
{
	config = talloc(test, struct config);
	config_set_defaults(config);
	return config;
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

int platform_fini(void)
{
	talloc_free(platform_ctx);
	return 0;
}
