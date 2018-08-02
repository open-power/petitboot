
#define _GNU_SOURCE

#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <log/log.h>
#include <file/file.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <url/url.h>

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

	if (config->disable_snapshots)
		pb_log(" dm-snapshots disabled\n");

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
			pb_log("    url:  %s\n", ifconf->static_config.url);

		}
	}
	for (i = 0; i < config->network.n_dns_servers; i++)
		pb_log("  dns server %s\n", config->network.dns_servers[i]);

	for (i = 0; i < config->n_autoboot_opts; i++) {
		if (config->autoboot_opts[i].boot_type == BOOT_DEVICE_TYPE)
			pb_log("  boot device %d: %s\n", i,
			       device_type_name(config->autoboot_opts[i].type));
		else
			pb_log("  boot device %d: uuid: %s\n",
			       i, config->autoboot_opts[i].uuid);
	}

	pb_log("  IPMI boot device 0x%02x%s\n", config->ipmi_bootdev,
			config->ipmi_bootdev_persistent ? " (persistent)" : "");

	pb_log("  Modifications allowed to disks: %s\n",
			config->allow_writes ? "yes" : "no");

	pb_log("  Default UI to boot on: %s\n",
		config->boot_console ?: "none set");
	if (config->manual_console)
		pb_log("    (Manually set)\n");

	if (config->http_proxy)
		pb_log("  HTTP Proxy: %s\n", config->http_proxy);
	if (config->https_proxy)
		pb_log("  HTTPS Proxy: %s\n", config->https_proxy);


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
	const char *lang;

	config->autoboot_enabled = true;
	config->autoboot_timeout_sec = 10;
	config->autoboot_enabled = true;
	config->network.interfaces = NULL;
	config->network.n_interfaces = 0;
	config->network.dns_servers = NULL;
	config->network.n_dns_servers = 0;
	config->http_proxy = NULL;
	config->https_proxy = NULL;
	config->safe_mode = false;
	config->allow_writes = true;
	config->disable_snapshots = false;

	config->n_consoles = 0;
	config->consoles = NULL;
	config->boot_console = NULL;

	config->n_autoboot_opts = 2;
	config->autoboot_opts = talloc_array(config, struct autoboot_option,
						config->n_autoboot_opts);
	config->autoboot_opts[0].boot_type = BOOT_DEVICE_TYPE;
	config->autoboot_opts[0].type = DEVICE_TYPE_NETWORK;
	config->autoboot_opts[1].boot_type = BOOT_DEVICE_TYPE;
	config->autoboot_opts[1].type = DEVICE_TYPE_ANY;

	config->ipmi_bootdev = 0;
	config->ipmi_bootdev_persistent = false;

	config->debug = config_debug_on_cmdline();

	lang = setlocale(LC_ALL, NULL);
	pb_log("lang: %s\n", lang);
	if (lang && strlen(lang))
		config->lang = talloc_strdup(config, lang);
	else
		config->lang = NULL;

}

int platform_init(void *ctx)
{
	extern struct platform *__start_platforms,  *__stop_platforms;
	struct platform **p;

	platform_ctx = talloc_new(ctx);

	for (p = &__start_platforms; p < &__stop_platforms; p++) {
		pb_debug("%s: Try platform %s\n", __func__, (*p)->name);
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

static int parse_hwaddr(struct interface_config *ifconf, const char *str)
{
	int i;

	if (strlen(str) != strlen("00:00:00:00:00:00"))
		return -1;

	for (i = 0; i < HWADDR_SIZE; i++) {
		char byte[3], *endp;
		unsigned long x;

		byte[0] = str[i * 3 + 0];
		byte[1] = str[i * 3 + 1];
		byte[2] = '\0';

		x = strtoul(byte, &endp, 16);
		if (endp != byte + 2)
			return -1;

		ifconf->hwaddr[i] = x & 0xff;
	}

	return 0;
}

static int config_parse_one_interface(struct config *config, char *confstr)
{
	struct interface_config *ifconf;
	char *tok, *tok_gw, *tok_url, *saveptr;

	ifconf = talloc_zero(config, struct interface_config);

	if (!confstr || !strlen(confstr))
		goto out_err;

	/* first token should be the mac address */
	tok = strtok_r(confstr, ",", &saveptr);
	if (!tok)
		goto out_err;

	if (parse_hwaddr(ifconf, tok))
		goto out_err;

	/* second token is the method */
	tok = strtok_r(NULL, ",", &saveptr);
	if (!tok || !strlen(tok) || !strcmp(tok, "ignore")) {
		ifconf->ignore = true;

	} else if (!strcmp(tok, "dhcp")) {
		ifconf->method = CONFIG_METHOD_DHCP;

	} else if (!strcmp(tok, "static")) {
		ifconf->method = CONFIG_METHOD_STATIC;

		/* ip/mask, [optional] gateway, [optional] url */
		tok = strtok_r(NULL, ",", &saveptr);
		if (!tok)
			goto out_err;
		ifconf->static_config.address =
			talloc_strdup(ifconf, tok);

		/*
		 * If a url is set but not a gateway, we can accidentally
		 * interpret the url as the gateway. To avoid changing the
		 * parameter format check if the "gateway" is actually a
		 * pb-url if it's the last token.
		 */
		tok_gw = strtok_r(NULL, ",", &saveptr);
		tok_url = strtok_r(NULL, ",", &saveptr);

		if (tok_gw) {
			if (tok_url || !is_url(tok_gw))
				ifconf->static_config.gateway =
					talloc_strdup(ifconf, tok_gw);
			else
					tok_url = tok_gw;
		}

		if (tok_url)
			ifconf->static_config.url =
				talloc_strdup(ifconf, tok_url);
	} else {
		pb_log("Unknown network configuration method %s\n", tok);
		goto out_err;
	}

	config->network.interfaces = talloc_realloc(config,
			config->network.interfaces,
			struct interface_config *,
			++config->network.n_interfaces);

	config->network.interfaces[config->network.n_interfaces - 1] = ifconf;

	return 0;
out_err:
	talloc_free(ifconf);
	return -1;
}

static int config_parse_one_dns(struct config *config, char *confstr)
{
	char *tok, *saveptr = NULL;

	for (tok = strtok_r(confstr, ",", &saveptr); tok;
			tok = strtok_r(NULL, ",", &saveptr)) {

		char *server = talloc_strdup(config, tok);

		config->network.dns_servers = talloc_realloc(config,
				config->network.dns_servers, const char *,
				++config->network.n_dns_servers);

		config->network.dns_servers[config->network.n_dns_servers - 1]
				= server;
	}

	return 0;
}

static void config_populate_network(struct config *config, const char *cval)
{
	char *val, *saveptr = NULL;
	int i;

	if (!cval || !strlen(cval))
		return;

	val = talloc_strdup(config, cval);

	for (i = 0; ; i++) {
		char *tok;

		tok = strtok_r(i == 0 ? val : NULL, " ", &saveptr);
		if (!tok)
			break;

		if (!strncasecmp(tok, "dns,", strlen("dns,")))
			config_parse_one_dns(config, tok + strlen("dns,"));
		else
			config_parse_one_interface(config, tok);

	}

	talloc_free(val);
}

static int read_bootdev(void *ctx, char **pos, struct autoboot_option *opt)
{
	char *delim = strchr(*pos, ' ');
	int len, prefix = 0, rc = -1;
	enum device_type type;

	if (!strncmp(*pos, "uuid:", strlen("uuid:"))) {
		prefix = strlen("uuid:");
		opt->boot_type = BOOT_DEVICE_UUID;
	} else if (!strncmp(*pos, "mac:", strlen("mac:"))) {
		prefix = strlen("mac:");
		opt->boot_type = BOOT_DEVICE_UUID;
	} else {
		type = find_device_type(*pos);
		if (type != DEVICE_TYPE_UNKNOWN) {
			opt->type = type;
			opt->boot_type = BOOT_DEVICE_TYPE;
			rc = 0;
		}
	}

	if (opt->boot_type == BOOT_DEVICE_UUID) {
		if (delim)
			len = (int)(delim - *pos) - prefix;
		else
			len = strlen(*pos) - prefix;

		if (len) {
			opt->uuid = talloc_strndup(ctx, *pos + prefix, len);
			rc = 0;
		}
	}

	/* Always advance pointer to next option or end */
	if (delim)
		*pos = delim + 1;
	else
		*pos += strlen(*pos);

	return rc;
}

static void config_populate_bootdev(struct config *config,
	const struct param_list *pl)
{
	struct autoboot_option *opt, *new = NULL;
	char *pos, *end;
	unsigned int n_new = 0;
	const char *val;

	/* Check for ordered bootdevs */
	val = param_list_get_value(pl, "petitboot,bootdevs");
	if (!val || !strlen(val)) {
		pos = end = NULL;
	} else {
		pos = talloc_strdup(config, val);
		end = strchr(pos, '\0');
	}

	while (pos && pos < end) {
		opt = talloc(config, struct autoboot_option);

		if (read_bootdev(config, &pos, opt)) {
			pb_log("bootdev config is in an unknown format "
			       "(expected uuid:... or mac:...)\n");
			talloc_free(opt);
			continue;
		}

		new = talloc_realloc(config, new, struct autoboot_option,
				     n_new + 1);
		new[n_new] = *opt;
		n_new++;
		talloc_free(opt);

	}

	if (!n_new) {
		/* If autoboot has been disabled, clear the default options */
		if (!config->autoboot_enabled) {
			talloc_free(config->autoboot_opts);
			config->n_autoboot_opts = 0;
		}
		return;
	}

	talloc_free(config->autoboot_opts);
	config->autoboot_opts = new;
	config->n_autoboot_opts = n_new;
}

void config_populate_all(struct config *config, const struct param_list *pl)
{
	const char *val;
	char *end;
	unsigned long timeout;

	/* if the "auto-boot?' property is present and "false", disable auto
	 * boot */
	val = param_list_get_value(pl, "auto-boot?");
	config->autoboot_enabled = !val || strcmp(val, "false");

	val = param_list_get_value(pl, "petitboot,timeout");
	if (val) {
		timeout = strtoul(val, &end, 10);
		if (end != val) {
			if (timeout >= INT_MAX)
				timeout = INT_MAX;
			config->autoboot_timeout_sec = (int)timeout;
		}
	}

	val = param_list_get_value(pl, "petitboot,language");
	config->lang = val ? talloc_strdup(config, val) : NULL;

	val = param_list_get_value(pl, "petitboot,network");
	config_populate_network(config, val);

	config_populate_bootdev(config, pl);

	if (!config->debug) {
		val = param_list_get_value(pl, "petitboot,debug?");
		config->debug = val && !strcmp(val, "true");
	}

	val = param_list_get_value(pl, "petitboot,write?");
	if (val)
		config->allow_writes = !strcmp(val, "true");

	val = param_list_get_value(pl, "petitboot,snapshots?");
	if (val)
		config->disable_snapshots = !strcmp(val, "false");

	val = param_list_get_value(pl, "petitboot,console");
	if (val)
		config->boot_console = talloc_strdup(config, val);
	/* If a full path is already set we don't want to override it */
	config->manual_console = config->boot_console &&
					!strchr(config->boot_console, '[');

	val = param_list_get_value(pl, "petitboot,http_proxy");
	if (val)
		config->http_proxy = talloc_strdup(config, val);
	val = param_list_get_value(pl, "petitboot,https_proxy");
	if (val)
		config->https_proxy = talloc_strdup(config, val);
}
