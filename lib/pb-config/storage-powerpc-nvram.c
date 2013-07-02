
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>

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

	for (pos = buf + i; pos < buf + len; pos += paramlen) {
		unsigned int namelen;
		struct param *param;

		paramlen = strlen(pos);

		name = pos;
		value = strchr(pos, '=');
		if (!value)
			continue;

		namelen = name - value;

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
	int rc, len, buf_len;
	int pipefds[2], status;
	char *buf;
	pid_t pid;

	rc = pipe(pipefds);
	if (rc) {
		perror("pipe");
		return -1;
	}

	pid = fork();

	if (pid < 0) {
		perror("fork");
		return -1;
	}

	if (pid == 0) {
		close(STDIN_FILENO);
		close(pipefds[0]);
		dup2(pipefds[1], STDOUT_FILENO);
		execlp("nvram", "nvram", "--print-config",
				"--partition", partition, NULL);
		exit(EXIT_FAILURE);
	}

	close(pipefds[1]);

	len = 0;
	buf_len = max_partition_size;
	buf = talloc_array(nv, char, buf_len);

	for (;;) {
		rc = read(pipefds[0], buf + len, buf_len - len);

		if (rc < 0) {
			perror("read");
			break;
		}

		if (rc == 0)
			break;

		len += rc;
	}

	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		fprintf(stderr, "nvram process returned "
				"non-zero exit status\n");
		return -1;
	}

	if (rc < 0)
		return rc;

	return parse_nvram_params(nv, buf, len);
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

static int parse_hwaddr(struct network_config *config, char *str)
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

		config->hwaddr[i] = x & 0xff;
	}

	return 0;
}

static int parse_one_network_config(struct network_config *config,
		char *confstr)
{
	char *tok, *saveptr;

	if (!confstr || !strlen(confstr))
		return -1;

	/* first token should be the mac address */
	tok = strtok_r(confstr, ",", &saveptr);
	if (!tok)
		return -1;

	if (parse_hwaddr(config, tok))
		return -1;

	/* second token is the method */
	tok = strtok_r(NULL, ",", &saveptr);
	if (!tok || !strlen(tok) || !strcmp(tok, "ignore")) {
		config->ignore = true;
		return 0;
	}

	if (!strcmp(tok, "dhcp")) {
		config->method = CONFIG_METHOD_DHCP;

	} else if (!strcmp(tok, "static")) {
		config->method = CONFIG_METHOD_STATIC;

		/* ip/mask, [optional] gateway, [optional] dns */
		tok = strtok_r(NULL, ",", &saveptr);
		if (!tok)
			return -1;
		config->static_config.address =
			talloc_strdup(config, tok);

		tok = strtok_r(NULL, ",", &saveptr);
		if (tok) {
			config->static_config.gateway =
				talloc_strdup(config, tok);
			tok = strtok_r(NULL, ",", &saveptr);
		}

		if (tok) {
			config->static_config.dns =
				talloc_strdup(config, tok);
		}
	} else {
		pb_log("Unknown network configuration method %s\n", tok);
		return -1;
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
		struct network_config *netconf;
		char *tok, *saveptr;
		int rc;

		tok = strtok_r(i == 0 ? val : NULL, " ", &saveptr);
		if (!tok)
			break;

		netconf = talloc(nv, struct network_config);

		rc = parse_one_network_config(netconf, tok);
		if (rc) {
			talloc_free(netconf);
			continue;
		}

		config->network_configs = talloc_realloc(nv,
						config->network_configs,
						struct network_config *,
						++config->n_network_configs);

		config->network_configs[config->n_network_configs - 1] =
						netconf;
	}

	talloc_free(val);
}

static void populate_config(struct powerpc_nvram_storage *nv,
		struct config *config)
{
	const char *val;

	/* if the "auto-boot?' property is present and "false", disable auto
	 * boot */
	val = get_param(nv, "auto-boot?");
	config->autoboot_enabled = !val || strcmp(val, "false");

	populate_network_config(nv, config);
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

struct config_storage *create_powerpc_nvram_storage(void *ctx)
{
	struct powerpc_nvram_storage *nv;

	nv = talloc(ctx, struct powerpc_nvram_storage);
	nv->storage.load = load;
	list_init(&nv->params);

	return &nv->storage;
}
