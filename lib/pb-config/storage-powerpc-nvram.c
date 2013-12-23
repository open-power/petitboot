
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>
#include <process/process.h>

#include "pb-config.h"
#include "storage.h"

static const char *partition = "common";

struct param {
	char			*name;
	char			*value;
	bool			modified;
	struct list_item	list;
};

struct powerpc_nvram_storage {
	struct config_storage	storage;
	struct list		params;
};

static const char *known_params[] = {
	"auto-boot?",
	"petitboot,network",
	"petitboot,timeout",
	NULL,
};

#define to_powerpc_nvram_storage(s) \
	container_of(s, struct powerpc_nvram_storage, storage)

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

static int parse_nvram_params(struct powerpc_nvram_storage *nv,
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

		param = talloc(nv, struct param);
		param->modified = false;
		param->name = talloc_strndup(nv, name, namelen);
		param->value = talloc_strdup(nv, value);
		list_add(&nv->params, &param->list);
	}

	return 0;
}

static int parse_nvram(struct powerpc_nvram_storage *nv)
{
	struct process *process;
	const char *argv[5];
	int rc;

	argv[0] = "nvram";
	argv[1] = "--print-config";
	argv[2] = "--partition";
	argv[3] = partition;
	argv[4] = NULL;

	process = process_create(nv);
	process->path = "nvram";
	process->argv = argv;
	process->keep_stdout = true;

	rc = process_run_sync(process);

	if (rc || !WIFEXITED(process->exit_status)
			|| WEXITSTATUS(process->exit_status)) {
		fprintf(stderr, "nvram process returned "
				"non-zero exit status\n");
		rc = -1;
	} else {
		rc = parse_nvram_params(nv, process->stdout_buf,
					    process->stdout_len);
	}

	process_release(process);
	return rc;
}

static int write_nvram(struct powerpc_nvram_storage *nv)
{
	struct process *process;
	struct param *param;
	const char *argv[6];
	int rc;

	argv[0] = "nvram";
	argv[1] = "--update-config";
	argv[2] = NULL;
	argv[3] = "--partition";
	argv[4] = partition;
	argv[5] = NULL;

	process = process_create(nv);
	process->path = "nvram";
	process->argv = argv;

	list_for_each_entry(&nv->params, param, list) {
		char *paramstr;

		if (!param->modified)
			continue;

		paramstr = talloc_asprintf(nv, "%s=%s",
				param->name, param->value);
		argv[2] = paramstr;

		rc = process_run_sync(process);

		talloc_free(paramstr);

		if (rc || !WIFEXITED(process->exit_status)
				|| WEXITSTATUS(process->exit_status)) {
			rc = -1;
			pb_log("nvram update process returned "
					"non-zero exit status\n");
			break;
		}
	}

	process_release(process);
	return rc;
}

static const char *get_param(struct powerpc_nvram_storage *nv,
		const char *name)
{
	struct param *param;

	list_for_each_entry(&nv->params, param, list)
		if (!strcmp(param->name, name))
			return param->value;
	return NULL;
}

static void set_param(struct powerpc_nvram_storage *nv, const char *name,
		const char *value)
{
	struct param *param;

	list_for_each_entry(&nv->params, param, list) {
		if (strcmp(param->name, name))
			continue;

		if (!strcmp(param->value, value))
			return;

		talloc_free(param->value);
		param->value = talloc_strdup(param, value);
		param->modified = true;
		return;
	}


	param = talloc(nv, struct param);
	param->modified = true;
	param->name = talloc_strdup(nv, name);
	param->value = talloc_strdup(nv, value);
	list_add(&nv->params, &param->list);
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

		/* ip/mask, [optional] gateway */
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
	char *tok, *saveptr;

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

static void populate_network_config(struct powerpc_nvram_storage *nv,
		struct config *config)
{
	const char *cval;
	char *val;
	int i;

	cval = get_param(nv, "petitboot,network");
	if (!cval || !strlen(cval))
		return;

	val = talloc_strdup(config, cval);

	for (i = 0; ; i++) {
		char *tok, *saveptr;

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

static void populate_config(struct powerpc_nvram_storage *nv,
		struct config *config)
{
	const char *val;
	char *end;
	unsigned long timeout;

	/* if the "auto-boot?' property is present and "false", disable auto
	 * boot */
	val = get_param(nv, "auto-boot?");
	config->autoboot_enabled = !val || strcmp(val, "false");

	val = get_param(nv, "petitboot,timeout");
	if (val) {
		timeout = strtoul(val, &end, 10);
		if (end != val) {
			if (timeout >= INT_MAX)
				timeout = INT_MAX;
			config->autoboot_timeout_sec = (int)timeout;
		}
	}

	populate_network_config(nv, config);
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
		str = talloc_asprintf_append(str, "static,%s%s%s",
				config->static_config.address,
				config->static_config.gateway ? "," : "",
				config->static_config.gateway ?: "");
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

static void update_network_config(struct powerpc_nvram_storage *nv,
	struct config *config)
{
	unsigned int i;
	char *val;

	val = talloc_strdup(nv, "");

	for (i = 0; i < config->network.n_interfaces; i++) {
		char *iface_str = iface_config_str(nv,
					config->network.interfaces[i]);
		val = talloc_asprintf_append(val, "%s%s",
				*val == '\0' ? "" : " ", iface_str);
		talloc_free(iface_str);
	}

	if (config->network.n_dns_servers) {
		char *dns_str = dns_config_str(nv, config->network.dns_servers,
						config->network.n_dns_servers);
		val = talloc_asprintf_append(val, "%s%s",
				*val == '\0' ? "" : " ", dns_str);
		talloc_free(dns_str);
	}

	set_param(nv, "petitboot,network", val);

	talloc_free(val);
}

static int update_config(struct powerpc_nvram_storage *nv,
		struct config *config, struct config *defaults)
{
	char *val;

	if (config->autoboot_enabled != defaults->autoboot_enabled) {
		val = config->autoboot_enabled ? "true" : "false";
		set_param(nv, "auto-boot?", val);
	}

	if (config->autoboot_timeout_sec != defaults->autoboot_timeout_sec) {
		val = talloc_asprintf(nv, "%d", config->autoboot_timeout_sec);
		set_param(nv, "petitboot,timeout", val);
		talloc_free(val);
	}

	update_network_config(nv, config);

	return write_nvram(nv);
}

static int load(struct config_storage *st, struct config *config)
{
	struct powerpc_nvram_storage *nv = to_powerpc_nvram_storage(st);
	int rc;

	rc = parse_nvram(nv);
	if (rc)
		return rc;

	populate_config(nv, config);

	return 0;
}

static int save(struct config_storage *st, struct config *config)
{
	struct powerpc_nvram_storage *nv = to_powerpc_nvram_storage(st);
	struct config *defaults;
	int rc;

	defaults = talloc_zero(nv, struct config);
	config_set_defaults(defaults);

	rc = update_config(nv, config, defaults);

	talloc_free(defaults);
	return rc;
}

struct config_storage *create_powerpc_nvram_storage(void *ctx)
{
	struct powerpc_nvram_storage *nv;

	nv = talloc(ctx, struct powerpc_nvram_storage);
	nv->storage.load = load;
	nv->storage.save = save;
	list_init(&nv->params);

	return &nv->storage;
}
