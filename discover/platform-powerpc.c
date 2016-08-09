
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <asm/byteorder.h>

#include <file/file.h>
#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>
#include <process/process.h>
#include <types/types.h>

#include "hostboot.h"
#include "platform.h"
#include "ipmi.h"
#include "dt.h"

static const char *partition = "common";
static const char *sysparams_dir = "/sys/firmware/opal/sysparams/";
static const char *devtree_dir = "/proc/device-tree/";
static const int ipmi_timeout = 5000; /* milliseconds. */

struct param {
	char			*name;
	char			*value;
	bool			modified;
	struct list_item	list;
};

struct platform_powerpc {
	struct list	params;
	struct ipmi	*ipmi;
	bool		ipmi_bootdev_persistent;
	int		(*get_ipmi_bootdev)(
				struct platform_powerpc *platform,
				uint8_t *bootdev, bool *persistent);
	int		(*clear_ipmi_bootdev)(
				struct platform_powerpc *platform,
				bool persistent);
	int 		(*set_os_boot_sensor)(
				struct platform_powerpc *platform);
	void		(*get_platform_versions)(struct system_info *info);
};

static const char *known_params[] = {
	"auto-boot?",
	"petitboot,network",
	"petitboot,timeout",
	"petitboot,bootdev",
	"petitboot,bootdevs",
	"petitboot,language",
	"petitboot,debug?",
	"petitboot,write?",
	"petitboot,snapshots?",
	"petitboot,tty",
	NULL,
};

#define to_platform_powerpc(p) \
	(struct platform_powerpc *)(p->platform_data)

/* a partition max a max size of 64k * 16bytes = 1M */
static const int max_partition_size = 64 * 1024 * 16;

static bool param_is_known(const char *param, unsigned int len)
{
	const char *known_param;
	unsigned int i;

	for (i = 0; known_params[i]; i++) {
		known_param = known_params[i];
		if (len == strlen(known_param) &&
				!strncmp(param, known_param, len))
			return true;
	}

	return false;
}

static int parse_nvram_params(struct platform_powerpc *platform,
		char *buf, int len)
{
	char *pos, *name, *value;
	unsigned int paramlen;
	int i, count;

	/* discard 2 header lines:
	 * "common" partiton"
	 * ------------------
	 */
	pos = buf;
	count = 0;

	for (i = 0; i < len; i++) {
		if (pos[i] == '\n')
			count++;
		if (count == 2)
			break;
	}

	if (i == len) {
		fprintf(stderr, "failure parsing nvram output\n");
		return -1;
	}

	for (pos = buf + i; pos < buf + len; pos += paramlen + 1) {
		unsigned int namelen;
		struct param *param;
		char *newline;

		newline = strchr(pos, '\n');
		if (!newline)
			break;

		*newline = '\0';

		paramlen = strlen(pos);

		name = pos;
		value = strchr(pos, '=');
		if (!value)
			continue;

		namelen = value - name;
		if (namelen == 0)
			continue;

		if (!param_is_known(name, namelen))
			continue;

		value++;

		param = talloc(platform, struct param);
		param->modified = false;
		param->name = talloc_strndup(platform, name, namelen);
		param->value = talloc_strdup(platform, value);
		list_add(&platform->params, &param->list);
	}

	return 0;
}

static int parse_nvram(struct platform_powerpc *platform)
{
	struct process *process;
	const char *argv[5];
	int rc;

	argv[0] = "nvram";
	argv[1] = "--print-config";
	argv[2] = "--partition";
	argv[3] = partition;
	argv[4] = NULL;

	process = process_create(platform);
	process->path = "nvram";
	process->argv = argv;
	process->keep_stdout = true;

	rc = process_run_sync(process);

	if (rc || !process_exit_ok(process)) {
		fprintf(stderr, "nvram process returned "
				"non-zero exit status\n");
		rc = -1;
	} else {
		rc = parse_nvram_params(platform, process->stdout_buf,
					    process->stdout_len);
	}

	process_release(process);
	return rc;
}

static int write_nvram(struct platform_powerpc *platform)
{
	struct process *process;
	struct param *param;
	const char *argv[6];
	int rc = 0;

	argv[0] = "nvram";
	argv[1] = "--update-config";
	argv[2] = NULL;
	argv[3] = "--partition";
	argv[4] = partition;
	argv[5] = NULL;

	process = process_create(platform);
	process->path = "nvram";
	process->argv = argv;

	list_for_each_entry(&platform->params, param, list) {
		char *paramstr;

		if (!param->modified)
			continue;

		paramstr = talloc_asprintf(platform, "%s=%s",
				param->name, param->value);
		argv[2] = paramstr;

		rc = process_run_sync(process);

		talloc_free(paramstr);

		if (rc || !process_exit_ok(process)) {
			rc = -1;
			pb_log("nvram update process returned "
					"non-zero exit status\n");
			break;
		}
	}

	process_release(process);
	return rc;
}

static const char *get_param(struct platform_powerpc *platform,
		const char *name)
{
	struct param *param;

	list_for_each_entry(&platform->params, param, list)
		if (!strcmp(param->name, name))
			return param->value;
	return NULL;
}

static void set_param(struct platform_powerpc *platform, const char *name,
		const char *value)
{
	struct param *param;

	list_for_each_entry(&platform->params, param, list) {
		if (strcmp(param->name, name))
			continue;

		if (!strcmp(param->value, value))
			return;

		talloc_free(param->value);
		param->value = talloc_strdup(param, value);
		param->modified = true;
		return;
	}


	param = talloc(platform, struct param);
	param->modified = true;
	param->name = talloc_strdup(platform, name);
	param->value = talloc_strdup(platform, value);
	list_add(&platform->params, &param->list);
}

static int parse_hwaddr(struct interface_config *ifconf, char *str)
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

static int parse_one_interface_config(struct config *config,
		char *confstr)
{
	struct interface_config *ifconf;
	char *tok, *saveptr;

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

		tok = strtok_r(NULL, ",", &saveptr);
		if (tok) {
			ifconf->static_config.gateway =
				talloc_strdup(ifconf, tok);
		}

		tok = strtok_r(NULL, ",", &saveptr);
		if (tok) {
			ifconf->static_config.url =
				talloc_strdup(ifconf, tok);
		}

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

static int parse_one_dns_config(struct config *config,
		char *confstr)
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

static void populate_network_config(struct platform_powerpc *platform,
		struct config *config)
{
	char *val, *saveptr = NULL;
	const char *cval;
	int i;

	cval = get_param(platform, "petitboot,network");
	if (!cval || !strlen(cval))
		return;

	val = talloc_strdup(config, cval);

	for (i = 0; ; i++) {
		char *tok;

		tok = strtok_r(i == 0 ? val : NULL, " ", &saveptr);
		if (!tok)
			break;

		if (!strncasecmp(tok, "dns,", strlen("dns,")))
			parse_one_dns_config(config, tok + strlen("dns,"));
		else
			parse_one_interface_config(config, tok);

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
		rc = 0;
	} else if (!strncmp(*pos, "mac:", strlen("mac:"))) {
		prefix = strlen("mac:");
		opt->boot_type = BOOT_DEVICE_UUID;
		rc = 0;
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
			len = strlen(*pos);

		opt->uuid = talloc_strndup(ctx, *pos + prefix, len);
	}

	/* Always advance pointer to next option or end */
	if (delim)
		*pos = delim + 1;
	else
		*pos += strlen(*pos);

	return rc;
}

static void populate_bootdev_config(struct platform_powerpc *platform,
		struct config *config)
{
	struct autoboot_option *opt, *new = NULL;
	char *pos, *end, *old_dev = NULL;
	unsigned int n_new = 0;
	const char *val;
	bool conflict;

	/* Check for old-style bootdev */
	val = get_param(platform, "petitboot,bootdev");
	if (val && strlen(val)) {
		pos = talloc_strdup(config, val);
		if (!strncmp(val, "uuid:", strlen("uuid:")))
			old_dev = talloc_strdup(config,
						val + strlen("uuid:"));
		else if (!strncmp(val, "mac:", strlen("mac:")))
			old_dev = talloc_strdup(config,
						val + strlen("mac:"));
	}

	/* Check for ordered bootdevs */
	val = get_param(platform, "petitboot,bootdevs");
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

	if (!n_new && !old_dev) {
		/* If autoboot has been disabled, clear the default options */
		if (!config->autoboot_enabled) {
			talloc_free(config->autoboot_opts);
			config->n_autoboot_opts = 0;
		}
		return;
	}

	conflict = old_dev && (!n_new ||
				    new[0].boot_type == BOOT_DEVICE_TYPE ||
				    /* Canonical UUIDs are 36 characters long */
				    strncmp(new[0].uuid, old_dev, 36));

	if (!conflict) {
		talloc_free(config->autoboot_opts);
		config->autoboot_opts = new;
		config->n_autoboot_opts = n_new;
		return;
	}

	/*
	 * Difference detected, defer to old format in case it has been updated
	 * recently
	 */
	pb_debug("Old autoboot bootdev detected\n");
	talloc_free(config->autoboot_opts);
	config->autoboot_opts = talloc(config, struct autoboot_option);
	config->autoboot_opts[0].boot_type = BOOT_DEVICE_UUID;
	config->autoboot_opts[0].uuid = talloc_strdup(config, old_dev);
	config->n_autoboot_opts = 1;
}

static void populate_config(struct platform_powerpc *platform,
		struct config *config)
{
	const char *val;
	char *end;
	unsigned long timeout;

	/* if the "auto-boot?' property is present and "false", disable auto
	 * boot */
	val = get_param(platform, "auto-boot?");
	config->autoboot_enabled = !val || strcmp(val, "false");

	val = get_param(platform, "petitboot,timeout");
	if (val) {
		timeout = strtoul(val, &end, 10);
		if (end != val) {
			if (timeout >= INT_MAX)
				timeout = INT_MAX;
			config->autoboot_timeout_sec = (int)timeout;
		}
	}

	val = get_param(platform, "petitboot,language");
	config->lang = val ? talloc_strdup(config, val) : NULL;

	populate_network_config(platform, config);

	populate_bootdev_config(platform, config);

	if (!config->debug) {
		val = get_param(platform, "petitboot,debug?");
		config->debug = val && !strcmp(val, "true");
	}

	val = get_param(platform, "petitboot,write?");
	if (val)
		config->allow_writes = !strcmp(val, "true");

	val = get_param(platform, "petitboot,snapshots?");
	if (val)
		config->disable_snapshots = !strcmp(val, "false");

	val = get_param(platform, "petitboot,tty");
	if (val)
		config->boot_console = talloc_strdup(config, val);
}

static char *iface_config_str(void *ctx, struct interface_config *config)
{
	char *str;

	/* todo: HWADDR size is hardcoded as 6, but we may need to handle
	 * different hardware address formats */
	str = talloc_asprintf(ctx, "%02x:%02x:%02x:%02x:%02x:%02x,",
			config->hwaddr[0], config->hwaddr[1],
			config->hwaddr[2], config->hwaddr[3],
			config->hwaddr[4], config->hwaddr[5]);

	if (config->ignore) {
		str = talloc_asprintf_append(str, "ignore");

	} else if (config->method == CONFIG_METHOD_DHCP) {
		str = talloc_asprintf_append(str, "dhcp");

	} else if (config->method == CONFIG_METHOD_STATIC) {
		str = talloc_asprintf_append(str, "static,%s%s%s%s%s",
				config->static_config.address,
				config->static_config.gateway ? "," : "",
				config->static_config.gateway ?: "",
				config->static_config.url ? "," : "",
				config->static_config.url ?: "");
	}
	return str;
}

static char *dns_config_str(void *ctx, const char **dns_servers, int n)
{
	char *str;
	int i;

	str = talloc_strdup(ctx, "dns,");
	for (i = 0; i < n; i++) {
		str = talloc_asprintf_append(str, "%s%s",
				i == 0 ? "" : ",",
				dns_servers[i]);
	}

	return str;
}

static void update_string_config(struct platform_powerpc *platform,
		const char *name, const char *value)
{
	const char *cur;

	cur = get_param(platform, name);

	/* don't set an empty parameter if it doesn't already exist */
	if (!cur && !strlen(value))
		return;

	set_param(platform, name, value);
}

static void update_network_config(struct platform_powerpc *platform,
	struct config *config)
{
	unsigned int i;
	char *val;

	/*
	 * Don't store IPMI overrides to NVRAM. If this was a persistent
	 * override it was already stored in NVRAM by
	 * get_ipmi_network_override()
	 */
	if (config->network.n_interfaces &&
		config->network.interfaces[0]->override)
		return;

	val = talloc_strdup(platform, "");

	for (i = 0; i < config->network.n_interfaces; i++) {
		char *iface_str = iface_config_str(platform,
					config->network.interfaces[i]);
		val = talloc_asprintf_append(val, "%s%s",
				*val == '\0' ? "" : " ", iface_str);
		talloc_free(iface_str);
	}

	if (config->network.n_dns_servers) {
		char *dns_str = dns_config_str(platform,
						config->network.dns_servers,
						config->network.n_dns_servers);
		val = talloc_asprintf_append(val, "%s%s",
				*val == '\0' ? "" : " ", dns_str);
		talloc_free(dns_str);
	}

	update_string_config(platform, "petitboot,network", val);

	talloc_free(val);
}

static void update_bootdev_config(struct platform_powerpc *platform,
		struct config *config)
{
	char *val = NULL, *boot_str = NULL, *tmp = NULL, *first = NULL;
	struct autoboot_option *opt;
	const char delim = ' ';
	unsigned int i;

	if (!config->n_autoboot_opts)
		first = val = "";
	else if (config->autoboot_opts[0].boot_type == BOOT_DEVICE_UUID)
		first = talloc_asprintf(config, "uuid:%s",
					config->autoboot_opts[0].uuid);
	else
		first = "";

	for (i = 0; i < config->n_autoboot_opts; i++) {
		opt = &config->autoboot_opts[i];
		switch (opt->boot_type) {
			case BOOT_DEVICE_TYPE:
				boot_str = talloc_asprintf(config, "%s%c",
						device_type_name(opt->type),
						delim);
				break;
			case BOOT_DEVICE_UUID:
				boot_str = talloc_asprintf(config, "uuid:%s%c",
						opt->uuid, delim);
				break;
			}
			tmp = val = talloc_asprintf_append(val, "%s", boot_str);
	}

	update_string_config(platform, "petitboot,bootdevs", val);
	update_string_config(platform, "petitboot,bootdev", first);
	talloc_free(tmp);
	if (boot_str)
		talloc_free(boot_str);
}

static int update_config(struct platform_powerpc *platform,
		struct config *config, struct config *defaults)
{
	char *tmp = NULL;
	const char *val;

	if (config->autoboot_enabled == defaults->autoboot_enabled)
		val = "";
	else
		val = config->autoboot_enabled ? "true" : "false";
	update_string_config(platform, "auto-boot?", val);

	if (config->autoboot_timeout_sec == defaults->autoboot_timeout_sec)
		val = "";
	else
		val = tmp = talloc_asprintf(platform, "%d",
				config->autoboot_timeout_sec);

	if (config->ipmi_bootdev == IPMI_BOOTDEV_INVALID &&
	    platform->clear_ipmi_bootdev) {
		platform->clear_ipmi_bootdev(platform,
				config->ipmi_bootdev_persistent);
		config->ipmi_bootdev = IPMI_BOOTDEV_NONE;
		config->ipmi_bootdev_persistent = false;
	}

	update_string_config(platform, "petitboot,timeout", val);
	if (tmp)
		talloc_free(tmp);

	val = config->lang ?: "";
	update_string_config(platform, "petitboot,language", val);

	if (config->allow_writes == defaults->allow_writes)
		val = "";
	else
		val = config->allow_writes ? "true" : "false";
	update_string_config(platform, "petitboot,write?", val);

	val = config->boot_console ?: "";
	update_string_config(platform, "petitboot,tty", val);

	update_network_config(platform, config);

	update_bootdev_config(platform, config);

	return write_nvram(platform);
}

static void set_ipmi_bootdev(struct config *config, enum ipmi_bootdev bootdev,
		bool persistent)
{
	config->ipmi_bootdev = bootdev;
	config->ipmi_bootdev_persistent = persistent;

	switch (bootdev) {
	case IPMI_BOOTDEV_NONE:
	case IPMI_BOOTDEV_DISK:
	case IPMI_BOOTDEV_NETWORK:
	case IPMI_BOOTDEV_CDROM:
	default:
		break;
	case IPMI_BOOTDEV_SETUP:
		config->autoboot_enabled = false;
		break;
	case IPMI_BOOTDEV_SAFE:
		config->autoboot_enabled = false;
		config->safe_mode = true;
		break;
	}
}

static int read_bootdev_sysparam(const char *name, uint8_t *val)
{
	uint8_t buf[2];
	char path[50];
	int fd, rc;

	assert(strlen(sysparams_dir) + strlen(name) < sizeof(path));
	snprintf(path, sizeof(path), "%s%s", sysparams_dir, name);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		pb_debug("powerpc: can't access sysparam %s\n",
				name);
		return -1;
	}

	rc = read(fd, buf, sizeof(buf));

	close(fd);

	/* bootdev definitions should only be one byte in size */
	if (rc != 1) {
		pb_debug("powerpc: sysparam %s read returned %d\n",
				name, rc);
		return -1;
	}

	pb_debug("powerpc: sysparam %s: 0x%02x\n", name, buf[0]);

	if (!ipmi_bootdev_is_valid(buf[0]))
		return -1;

	*val = buf[0];
	return 0;
}

static int write_bootdev_sysparam(const char *name, uint8_t val)
{
	char path[50];
	int fd, rc;

	assert(strlen(sysparams_dir) + strlen(name) < sizeof(path));
	snprintf(path, sizeof(path), "%s%s", sysparams_dir, name);

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		pb_debug("powerpc: can't access sysparam %s for writing\n",
				name);
		return -1;
	}

	for (;;) {
		errno = 0;
		rc = write(fd, &val, sizeof(val));
		if (rc == sizeof(val)) {
			rc = 0;
			break;
		}

		if (rc <= 0 && errno != EINTR) {
			pb_log("powerpc: error updating sysparam %s: %s",
					name, strerror(errno));
			rc = -1;
			break;
		}
	}

	close(fd);

	if (!rc)
		pb_debug("powerpc: set sysparam %s: 0x%02x\n", name, val);

	return rc;
}

static int clear_ipmi_bootdev_sysparams(
		struct platform_powerpc *platform __attribute__((unused)),
		bool persistent)
{
	if (persistent) {
		/* invalidate default-boot-device setting */
		write_bootdev_sysparam("default-boot-device", 0xff);
	} else {
		/* invalidate next-boot-device setting */
		write_bootdev_sysparam("next-boot-device", 0xff);
	}
	return 0;
}

static int get_ipmi_bootdev_sysparams(
		struct platform_powerpc *platform __attribute__((unused)),
		uint8_t *bootdev, bool *persistent)
{
	uint8_t next_bootdev, default_bootdev;
	bool next_valid, default_valid;
	int rc;

	rc = read_bootdev_sysparam("next-boot-device", &next_bootdev);
	next_valid = rc == 0;

	rc = read_bootdev_sysparam("default-boot-device", &default_bootdev);
	default_valid = rc == 0;

	/* nothing valid? no need to change the config */
	if (!next_valid && !default_valid)
		return -1;

	*persistent = !next_valid;
	*bootdev = next_valid ? next_bootdev : default_bootdev;
	return 0;
}

static int clear_ipmi_bootdev_ipmi(struct platform_powerpc *platform,
				   bool persistent __attribute__((unused)))
{
	uint16_t resp_len;
	uint8_t resp[1];
	uint8_t req[] = {
		0x05, /* parameter selector: boot flags */
		0x80, /* data 1: valid */
		0x00, /* data 2: bootdev: no override */
		0x00, /* data 3: system defaults */
		0x00, /* data 4: no request for shared mode, mux defaults */
		0x00, /* data 5: no instance request */
	};

	resp_len = sizeof(resp);

	ipmi_transaction(platform->ipmi, IPMI_NETFN_CHASSIS,
			IPMI_CMD_CHASSIS_SET_SYSTEM_BOOT_OPTIONS,
			req, sizeof(req),
			resp, &resp_len,
			ipmi_timeout);
	return 0;
}

static int get_ipmi_bootdev_ipmi(struct platform_powerpc *platform,
		uint8_t *bootdev, bool *persistent)
{
	uint16_t resp_len;
	uint8_t resp[8];
	int rc;
	uint8_t req[] = {
		0x05, /* parameter selector: boot flags */
		0x00, /* no set selector */
		0x00, /* no block selector */
	};

	resp_len = sizeof(resp);
	rc = ipmi_transaction(platform->ipmi, IPMI_NETFN_CHASSIS,
			IPMI_CMD_CHASSIS_GET_SYSTEM_BOOT_OPTIONS,
			req, sizeof(req),
			resp, &resp_len,
			ipmi_timeout);
	if (rc) {
		pb_log("platform: error reading IPMI boot options\n");
		return -1;
	}

	if (resp_len != sizeof(resp)) {
		pb_log("platform: unexpected length (%d) in "
				"boot options response\n", resp_len);
		return -1;
	}

	pb_debug("IPMI get_bootdev response:\n");
	for (int i = 0; i < resp_len; i++)
		pb_debug("%x ", resp[i]);
	pb_debug("\n");

	if (resp[0] != 0) {
		pb_log("platform: non-zero completion code %d from IPMI req\n",
				resp[0]);
		return -1;
	}

	/* check for correct parameter version */
	if ((resp[1] & 0xf) != 0x1) {
		pb_log("platform: unexpected version (0x%x) in "
				"boot options response\n", resp[0]);
		return -1;
	}

	/* check for valid paramters */
	if (resp[2] & 0x80) {
		pb_debug("platform: boot options are invalid/locked\n");
		return -1;
	}

	*persistent = false;

	/* check for valid flags */
	if (!(resp[3] & 0x80)) {
		pb_debug("platform: boot flags are invalid, ignoring\n");
		return -1;
	}

	*persistent = resp[3] & 0x40;
	*bootdev = (resp[4] >> 2) & 0x0f;
	return 0;
}

static int set_ipmi_os_boot_sensor(struct platform_powerpc *platform)
{
	int sensor_number;
	uint16_t resp_len;
	uint8_t resp[1];
	uint8_t req[] = {
		0x00, /* sensor number: os boot */
		0xA9, /* operation: set everything */
		0x00, /* sensor reading: none */
		0x40, /* assertion mask lsb: set state 6 */
		0x00, /* assertion mask msb: none */
		0x00, /* deassertion mask lsb: none */
		0x00, /* deassertion mask msb: none */
		0x00, /* event data 1: none */
		0x00, /* event data 2: none */
		0x00, /* event data 3: none */
	};

	sensor_number = get_ipmi_sensor(platform, IPMI_SENSOR_ID_OS_BOOT);
	if (sensor_number < 0) {
		pb_log("Couldn't find OS boot sensor in device tree\n");
		return -1;
	}

	req[0] = sensor_number;

	resp_len = sizeof(resp);

	ipmi_transaction(platform->ipmi, IPMI_NETFN_SE,
			IPMI_CMD_SENSOR_SET,
			req, sizeof(req),
			resp, &resp_len,
			ipmi_timeout); return 0;

	return 0;
}

static void get_ipmi_bmc_mac(struct platform *p, uint8_t *buf)
{
	struct platform_powerpc *platform = p->platform_data;
	uint16_t resp_len = 8;
	uint8_t resp[8];
	uint8_t req[] = { 0x1, 0x5, 0x0, 0x0 };
	int i, rc;

	rc = ipmi_transaction(platform->ipmi, IPMI_NETFN_TRANSPORT,
			IPMI_CMD_TRANSPORT_GET_LAN_PARAMS,
			req, sizeof(req),
			resp, &resp_len,
			ipmi_timeout);

	pb_debug("BMC MAC resp [%d][%d]:\n", rc, resp_len);

	if (rc == 0 && resp_len > 0) {
		for (i = 2; i < resp_len; i++) {
		        pb_debug(" %x", resp[i]);
			buf[i - 2] = resp[i];
		}
		pb_debug("\n");
	}

}

/*
 * Retrieve info from the "Get Device ID" IPMI commands.
 * See Chapter 20.1 in the IPMIv2 specification.
 */
static void get_ipmi_bmc_versions(struct platform *p, struct system_info *info)
{
	struct platform_powerpc *platform = p->platform_data;
	uint16_t resp_len = 16;
	uint8_t resp[16], bcd;
	uint32_t aux_version;
	int i, rc;

	/* Retrieve info from current side */
	rc = ipmi_transaction(platform->ipmi, IPMI_NETFN_APP,
			IPMI_CMD_APP_GET_DEVICE_ID,
			NULL, 0,
			resp, &resp_len,
			ipmi_timeout);

	pb_debug("BMC version resp [%d][%d]:\n", rc, resp_len);
	if (resp_len > 0) {
		for (i = 0; i < resp_len; i++) {
		        pb_debug(" %x", resp[i]);
		}
		pb_debug("\n");
	}

	if (rc == 0 && resp_len == 16) {
		info->bmc_current = talloc_array(info, char *, 4);
		info->n_bmc_current = 4;

		info->bmc_current[0] = talloc_asprintf(info, "Device ID: 0x%x",
						resp[1]);
		info->bmc_current[1] = talloc_asprintf(info, "Device Rev: 0x%x",
						resp[2]);
		bcd = resp[4] & 0x0f;
		bcd += 10 * (resp[4] >> 4);
		memcpy(&aux_version, &resp[12], sizeof(aux_version));
		info->bmc_current[2] = talloc_asprintf(info,
						"Firmware version: %u.%02u.%05u",
						resp[3], bcd, aux_version);
		bcd = resp[5] & 0x0f;
		bcd += 10 * (resp[5] >> 4);
		info->bmc_current[3] = talloc_asprintf(info, "IPMI version: %u",
						bcd);
	} else
		pb_log("Failed to retrieve Device ID from IPMI\n");

	/* Retrieve info from golden side */
	memset(resp, 0, sizeof(resp));
	resp_len = 16;
	rc = ipmi_transaction(platform->ipmi, IPMI_NETFN_AMI,
			IPMI_CMD_APP_GET_DEVICE_ID_GOLDEN,
			NULL, 0,
			resp, &resp_len,
			ipmi_timeout);

	pb_debug("BMC golden resp [%d][%d]:\n", rc, resp_len);
	if (resp_len > 0) {
		for (i = 0; i < resp_len; i++) {
		        pb_debug(" %x", resp[i]);
		}
		pb_debug("\n");
	}

	if (rc == 0 && resp_len == 16) {
		info->bmc_golden = talloc_array(info, char *, 4);
		info->n_bmc_golden = 4;

		info->bmc_golden[0] = talloc_asprintf(info, "Device ID: 0x%x",
						resp[1]);
		info->bmc_golden[1] = talloc_asprintf(info, "Device Rev: 0x%x",
						resp[2]);
		bcd = resp[4] & 0x0f;
		bcd += 10 * (resp[4] >> 4);
		memcpy(&aux_version, &resp[12], sizeof(aux_version));
		info->bmc_golden[2] = talloc_asprintf(info,
						"Firmware version: %u.%02u.%u",
						resp[3], bcd, aux_version);
		bcd = resp[5] & 0x0f;
		bcd += 10 * (resp[5] >> 4);
		info->bmc_golden[3] = talloc_asprintf(info, "IPMI version: %u",
						bcd);
	} else
		pb_log("Failed to retrieve Golden Device ID from IPMI\n");
}

static void get_ipmi_network_override(struct platform_powerpc *platform,
			struct config *config)
{
	uint16_t min_len = 12, resp_len = 53, version;
	const uint32_t magic_value = 0x21706221;
	uint8_t resp[resp_len];
	uint32_t cookie;
	bool persistent;
	int i, rc;
	uint8_t req[] = {
		0x61, /* parameter selector: OEM section (network) */
		0x00, /* no set selector */
		0x00, /* no block selector */
	};

	rc = ipmi_transaction(platform->ipmi, IPMI_NETFN_CHASSIS,
			IPMI_CMD_CHASSIS_GET_SYSTEM_BOOT_OPTIONS,
			req, sizeof(req),
			resp, &resp_len,
			ipmi_timeout);

	pb_debug("IPMI net override resp [%d][%d]:\n", rc, resp_len);
	if (resp_len > 0) {
		for (i = 0; i < resp_len; i++) {
		        pb_debug(" %02x", resp[i]);
			if (i && (i + 1) % 16 == 0 && i != resp_len - 1)
				pb_debug("\n");
			else if (i && (i + 1) % 8 == 0)
				pb_debug(" ");
		}
		pb_debug("\n");
	}

	if (rc) {
		pb_debug("IPMI network config option unavailable\n");
		return;
	}

	if (resp_len < min_len) {
		pb_debug("IPMI net response too small\n");
		return;
	}

	if (resp[0] != 0) {
		pb_log("platform: non-zero completion code %d from IPMI network req\n",
		       resp[0]);
		return;
	}

	/* Check for correct parameter version */
	if ((resp[1] & 0xf) != 0x1) {
		pb_log("platform: unexpected version (0x%x) in network override response\n",
		       resp[0]);
		return;
	}

	/* Check that the parameters are valid */
	if (resp[2] & 0x80) {
		pb_debug("platform: network override is invalid/locked\n");
		return;
	}

	/* Check for valid parameters in the boot flags section */
	if (!(resp[3] & 0x80)) {
		pb_debug("platform: network override valid flag not set\n");
		return;
	}
	/* Read the persistent flag; if it is set we need to save this config */
	persistent = resp[3] & 0x40;
	if (persistent)
		pb_debug("platform: network override is persistent\n");

	/* Check 4-byte cookie value */
	i = 4;
	memcpy(&cookie, &resp[i], sizeof(cookie));
	cookie = __be32_to_cpu(cookie);
	if (cookie != magic_value) {
		pb_log("%s: Incorrect cookie %x\n", __func__, cookie);
		return;
	}
	i += sizeof(cookie);

	/* Check 2-byte version number */
	memcpy(&version, &resp[i], sizeof(version));
	version = __be16_to_cpu(version);
	i += sizeof(version);
	if (version != 1) {
		pb_debug("Unexpected version: %u\n", version);
		return;
	}

	/* Interpret the rest of the interface config */
	rc = parse_ipmi_interface_override(config, &resp[i], resp_len - i);

	if (!rc && persistent) {
		/* Write this new config to NVRAM */
		update_network_config(platform, config);
		rc = write_nvram(platform);
		if (rc)
			pb_log("platform: Failed to save persistent interface override\n");
	}
}

static void get_active_consoles(struct config *config)
{
	struct stat sbuf;
	char *fsp_prop = NULL;

	config->n_consoles = 2;
	config->consoles = talloc_array(config, char *, config->n_consoles);
	if (!config->consoles)
		goto err;

	config->consoles[0] = talloc_asprintf(config->consoles,
					"/dev/hvc0 [IPMI / Serial]");
	config->consoles[1] = talloc_asprintf(config->consoles,
					"/dev/tty1 [VGA]");

	fsp_prop = talloc_asprintf(config, "%sfsps", devtree_dir);
	if (stat(fsp_prop, &sbuf) == 0) {
		/* FSP based machines also have a separate serial console */
		config->consoles = talloc_realloc(config, config->consoles,
						char *,	config->n_consoles + 1);
		if (!config->consoles)
			goto err;
		config->consoles[config->n_consoles++] = talloc_asprintf(
						config->consoles,
						"/dev/hvc1 [Serial]");
	}

	return;
err:
	config->n_consoles = 0;
	pb_log("Failed to allocate memory for consoles\n");
}

static int load_config(struct platform *p, struct config *config)
{
	struct platform_powerpc *platform = to_platform_powerpc(p);
	int rc;

	rc = parse_nvram(platform);
	if (rc)
		return rc;

	populate_config(platform, config);

	if (platform->get_ipmi_bootdev) {
		bool bootdev_persistent;
		uint8_t bootdev = IPMI_BOOTDEV_INVALID;
		rc = platform->get_ipmi_bootdev(platform, &bootdev,
				&bootdev_persistent);
		if (!rc && ipmi_bootdev_is_valid(bootdev)) {
			set_ipmi_bootdev(config, bootdev, bootdev_persistent);
		}
	}

	if (platform->ipmi)
		get_ipmi_network_override(platform, config);

	get_active_consoles(config);

	return 0;
}

static int save_config(struct platform *p, struct config *config)
{
	struct platform_powerpc *platform = to_platform_powerpc(p);
	struct config *defaults;
	int rc;

	defaults = talloc_zero(platform, struct config);
	config_set_defaults(defaults);

	rc = update_config(platform, config, defaults);

	talloc_free(defaults);
	return rc;
}

static void pre_boot(struct platform *p, const struct config *config)
{
	struct platform_powerpc *platform = to_platform_powerpc(p);

	if (!config->ipmi_bootdev_persistent && platform->clear_ipmi_bootdev)
		platform->clear_ipmi_bootdev(platform, false);

	if (platform->set_os_boot_sensor)
		platform->set_os_boot_sensor(platform);
}

static int get_sysinfo(struct platform *p, struct system_info *sysinfo)
{
	struct platform_powerpc *platform = p->platform_data;
	char *buf, *filename;
	int len, rc;

	filename = talloc_asprintf(platform, "%smodel", devtree_dir);
	rc = read_file(platform, filename, &buf, &len);
	if (rc == 0)
		sysinfo->type = talloc_steal(sysinfo, buf);
	talloc_free(filename);

	filename = talloc_asprintf(platform, "%ssystem-id", devtree_dir);
	rc = read_file(platform, filename, &buf, &len);
	if (rc == 0)
		sysinfo->identifier = talloc_steal(sysinfo, buf);
	talloc_free(filename);

	sysinfo->bmc_mac = talloc_zero_size(sysinfo, HWADDR_SIZE);
	if (platform->ipmi)
		get_ipmi_bmc_mac(p, sysinfo->bmc_mac);

	if (platform->ipmi)
		get_ipmi_bmc_versions(p, sysinfo);

	if (platform->get_platform_versions)
		platform->get_platform_versions(sysinfo);

	return 0;
}

static bool probe(struct platform *p, void *ctx)
{
	struct platform_powerpc *platform;
	struct stat statbuf;
	bool bmc_present;
	int rc;

	/* we need a device tree */
	rc = stat("/proc/device-tree", &statbuf);
	if (rc)
		return false;

	if (!S_ISDIR(statbuf.st_mode))
		return false;

	platform = talloc_zero(ctx, struct platform_powerpc);
	list_init(&platform->params);

	p->platform_data = platform;

	bmc_present = stat("/proc/device-tree/bmc", &statbuf) == 0;

	if (ipmi_present() && bmc_present) {
		pb_debug("platform: using direct IPMI for IPMI paramters\n");
		platform->ipmi = ipmi_open(platform);
		platform->get_ipmi_bootdev = get_ipmi_bootdev_ipmi;
		platform->clear_ipmi_bootdev = clear_ipmi_bootdev_ipmi;
		platform->set_os_boot_sensor = set_ipmi_os_boot_sensor;
	} else if (!stat(sysparams_dir, &statbuf)) {
		pb_debug("platform: using sysparams for IPMI paramters\n");
		platform->get_ipmi_bootdev = get_ipmi_bootdev_sysparams;
		platform->clear_ipmi_bootdev = clear_ipmi_bootdev_sysparams;

	} else {
		pb_log("platform: no IPMI parameter support\n");
	}

	if (bmc_present)
		platform->get_platform_versions = hostboot_load_versions;

	return true;
}


static struct platform platform_powerpc = {
	.name			= "powerpc",
	.dhcp_arch_id		= 0x000e,
	.probe			= probe,
	.load_config		= load_config,
	.save_config		= save_config,
	.pre_boot		= pre_boot,
	.get_sysinfo		= get_sysinfo,
};

register_platform(platform_powerpc);
