
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <asm/byteorder.h>
#include <limits.h>

#include <file/file.h>
#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>
#include <process/process.h>
#include <crypt/crypt.h>

#include "hostboot.h"
#include "platform.h"
#include "ipmi.h"
#include "dt.h"

static const char *partition = "common";
static const char *sysparams_dir = "/sys/firmware/opal/sysparams/";
static const char *devtree_dir = "/proc/device-tree/";

struct platform_powerpc {
	struct param_list *params;
	struct ipmi	*ipmi;
	char		*ipmi_mailbox_original_config;
	int		(*get_ipmi_bootdev)(
				struct platform_powerpc *platform,
				uint8_t *bootdev, bool *persistent);
	int		(*clear_ipmi_bootdev)(
				struct platform_powerpc *platform,
				bool persistent);
	int		(*get_ipmi_boot_mailbox)(
				struct platform_powerpc *platform,
				char **buf);
	int		(*clear_ipmi_boot_mailbox)(
				struct platform_powerpc *platform);
	int 		(*set_os_boot_sensor)(
				struct platform_powerpc *platform);
	void		(*get_platform_versions)(struct system_info *info);
};

#define to_platform_powerpc(p) \
	(struct platform_powerpc *)(p->platform_data)

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

		if (!param_list_is_known_n(platform->params, name, namelen))
			continue;

		*value = '\0';
		value++;

		param_list_set(platform->params, name, value, false);
	}

	return 0;
}

static int parse_nvram(struct platform_powerpc *platform)
{
	struct process_stdout *stdout;
	const char *argv[5];
	int rc;

	argv[0] = "nvram";
	argv[1] = "--print-config";
	argv[2] = "--partition";
	argv[3] = partition;
	argv[4] = NULL;

	rc = process_get_stdout_argv(NULL, &stdout, argv);

	if (rc) {
		fprintf(stderr, "nvram process returned "
				"non-zero exit status\n");
		rc = -1;
	} else {
		rc = parse_nvram_params(platform, stdout->buf, stdout->len);
	}

	talloc_free(stdout);
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

	param_list_for_each(platform->params, param) {
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

static void params_update_all(struct param_list *pl,
	const struct config *config, const struct config *defaults)
{
	char *tmp = NULL;
	const char *val;

	if (config->autoboot_enabled == defaults->autoboot_enabled)
		val = "";
	else
		val = config->autoboot_enabled ? "true" : "false";

	param_list_set_non_empty(pl, "auto-boot?", val, true);

	if (config->autoboot_timeout_sec == defaults->autoboot_timeout_sec)
		val = "";
	else
		val = tmp = talloc_asprintf(pl, "%d",
			config->autoboot_timeout_sec);

	param_list_set_non_empty(pl, "petitboot,timeout", val, true);
	if (tmp)
		talloc_free(tmp);

	val = config->lang ?: "";
	param_list_set_non_empty(pl, "petitboot,language", val, true);

	if (config->allow_writes == defaults->allow_writes)
		val = "";
	else
		val = config->allow_writes ? "true" : "false";
	param_list_set_non_empty(pl, "petitboot,write?", val, true);

	if (!config->manual_console) {
		val = config->boot_console ?: "";
		param_list_set_non_empty(pl, "petitboot,console", val, true);
	}

	val = config->http_proxy ?: "";
	param_list_set_non_empty(pl, "petitboot,http_proxy", val, true);
	val = config->https_proxy ?: "";
	param_list_set_non_empty(pl, "petitboot,https_proxy", val, true);

	params_update_network_values(pl, "petitboot,network", config);
	params_update_bootdev_values(pl, "petitboot,bootdevs", config);
}

static void config_set_ipmi_bootdev(struct config *config, enum ipmi_bootdev bootdev,
		bool persistent)
{
	config->ipmi_bootdev = bootdev;
	config->ipmi_bootdev_persistent = persistent;

	if (bootdev == IPMI_BOOTDEV_SAFE)
		config->safe_mode = true;
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
	char *debug_buf;
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

	debug_buf = format_buffer(platform, resp, resp_len);
	pb_debug_fn("IPMI get_bootdev response:\n%s\n", debug_buf);
	talloc_free(debug_buf);

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

static int get_ipmi_boot_mailbox_block(struct platform_powerpc *platform,
		char *buf, uint8_t block)
{
	size_t blocksize = 16;
	uint8_t resp[3 + 1 + 16];
	uint16_t resp_len;
	char *debug_buf;
	int rc;
	uint8_t req[] = {
		0x07,  /* parameter selector: boot initiator mailbox */
		block, /* set selector */
		0x00,  /* no block selector */
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

	if (resp_len < sizeof(resp)) {
		if (resp_len < 4) {
			pb_log("platform: unexpected length (%d) in "
					"boot options mailbox response\n",
					resp_len);
			return -1;
		}

		if (resp_len == 4) {
			pb_debug_fn("block %hu empty\n", block);
			return 0;
		}

		blocksize = sizeof(resp) - 4;
		pb_debug_fn("Mailbox block %hu returns only %zu bytes in block\n",
				block, blocksize);
	}

	debug_buf = format_buffer(platform, resp, resp_len);
	pb_debug_fn("IPMI bootdev mailbox block %hu:\n%s\n", block, debug_buf);
	talloc_free(debug_buf);

	if (resp[0] != 0) {
		pb_log("platform: non-zero completion code %d from IPMI req\n",
				resp[0]);
		return -1;
	}

	/* check for correct parameter version */
	if ((resp[1] & 0xf) != 0x1) {
		pb_log("platform: unexpected version (0x%x) in "
				"boot mailbox response\n", resp[0]);
		return -1;
	}

	/* check for valid paramters */
	if (resp[2] & 0x80) {
		pb_debug("platform: boot mailbox parameters are invalid/locked\n");
		return -1;
	}

	/* check for block number */
	if (resp[3] != block) {
		pb_debug("platform: returned boot mailbox block doesn't match "
				  "requested\n");
		return -1;
	}

	memcpy(buf, &resp[4], blocksize);

	return blocksize;
}

static int get_ipmi_boot_mailbox(struct platform_powerpc *platform,
		char **buf)
{
	char *mailbox_buffer, *prefix;
	const size_t blocksize = 16;
	char block_buffer[blocksize];
	size_t mailbox_size;
	int content_size;
	uint8_t i;
	int rc;

	mailbox_buffer = NULL;
	mailbox_size = 0;

	/*
	 * The BMC may hold up to 255 blocks of data but more likely the number
	 * will be closer to the minimum of 5 set by the specification and error
	 * on higher numbers.
	 */
	for (i = 0; i < UCHAR_MAX; i++) {
		rc = get_ipmi_boot_mailbox_block(platform, block_buffer, i);
		if (rc < 3 && i == 0) {
			/*
			 * Immediate failure, no blocks read or missing IANA
			 * number.
			 */
			return -1;
		}
		if (rc < 1) {
			/* Error or no bytes read */
			break;
		}

		if (i == 0) {
			/*
			 * The first three bytes of block zero are an IANA
			 * Enterprise ID number. Check it matches the IBM
			 * number, '2'.
			 */
			if (block_buffer[0] != 0x02 ||
				block_buffer[1] != 0x00 ||
				block_buffer[2] != 0x00) {
				pb_log_fn("IANA number unrecognised: 0x%x:0x%x:0x%x\n",
						block_buffer[0],
						block_buffer[1],
						block_buffer[2]);
				return -1;
			}
		}

		mailbox_buffer = talloc_realloc(platform, mailbox_buffer,
				char, mailbox_size + rc);
		if (!mailbox_buffer) {
			pb_log_fn("Failed to allocate mailbox buffer\n");
			return -1;
		}
		memcpy(mailbox_buffer + mailbox_size, block_buffer, rc);
		mailbox_size += rc;
	}

	if (i < 5)
		pb_log_fn("Only %hu blocks read, spec requires at least 5.\n"
			  "Send a bug report to your preferred BMC vendor!\n",
			  i);
	else
		pb_debug_fn("%hu blocks read (%zu bytes)\n", i, mailbox_size);

	if (mailbox_size < 3 + strlen("petitboot,bootdevs="))
		return -1;

	prefix = talloc_strndup(mailbox_buffer, mailbox_buffer + 3,
			strlen("petitboot,bootdevs="));
	if (!prefix) {
		pb_log_fn("Couldn't check prefix\n");
		talloc_free(mailbox_buffer);
		return -1;
	}

	if (strncmp(prefix, "petitboot,bootdevs=",
				strlen("petitboot,bootdevs=")) != 0 ) {
		/* Empty or garbage */
		pb_debug_fn("Buffer looks unconfigured\n");
		talloc_free(mailbox_buffer);
		*buf = NULL;
		return 0;
	}

	/* Don't include IANA number in buffer */
	content_size = mailbox_size - 3 - strlen("petitboot,bootdevs=");
	*buf = talloc_memdup(platform,
			mailbox_buffer + 3 + strlen("petitboot,bootdevs="),
			content_size + 1);
	(*buf)[content_size] = '\0';

	talloc_free(mailbox_buffer);
	return 0;
}

static int clear_ipmi_boot_mailbox(struct platform_powerpc *platform)
{
	uint8_t req[18] = {0}; /* req (2) + blocksize (16) */
	uint16_t resp_len;
	uint8_t resp[1];
	uint8_t i;
	int rc;

	req[0] = 0x07;  /* parameter selector: boot initiator mailbox */

	resp_len = sizeof(resp);

	for (i = 0; i < UCHAR_MAX; i++) {
		req[1] = i; /* set selector */
		rc = ipmi_transaction(platform->ipmi, IPMI_NETFN_CHASSIS,
				IPMI_CMD_CHASSIS_SET_SYSTEM_BOOT_OPTIONS,
				req, sizeof(req),
				resp, &resp_len,
				ipmi_timeout);

		if (rc || resp[0]) {
			if (i == 0) {
				pb_log_fn("error clearing IPMI boot mailbox, "
						"rc %d resp[0] %hu\n",
						rc, resp[0]);
				return -1;
			}
			break;
		}
	}

	pb_debug_fn("Cleared %hu blocks\n", i);

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

static void get_ipmi_network_override(struct platform_powerpc *platform,
			struct config *config)
{
	uint16_t min_len = 12, resp_len = 53, version;
	const uint32_t magic_value = 0x21706221;
	uint8_t resp[resp_len];
	char *debug_buf;
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

	debug_buf = format_buffer(platform, resp, resp_len);
	pb_debug_fn("IPMI net override response:\n%s\n", debug_buf);
	talloc_free(debug_buf);

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
		pb_log_fn("Incorrect cookie %x\n", cookie);
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
		params_update_network_values(platform->params,
			"petitboot,network", config);
		rc = write_nvram(platform);
		if (rc)
			pb_log("platform: Failed to save persistent interface override\n");
	}
}

static void config_get_active_consoles(struct config *config)
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
	const char *hash;
	int rc;

	rc = parse_nvram(platform);
	if (rc)
		pb_log_fn("Failed to parse nvram\n");

	/*
	 * If we have an IPMI mailbox configuration available use it instead of
	 * the boot order found in NVRAM.
	 */
	if (platform->get_ipmi_boot_mailbox) {
		char *mailbox;
		struct param *param;
		rc = platform->get_ipmi_boot_mailbox(platform, &mailbox);
		if (!rc && mailbox) {
			platform->ipmi_mailbox_original_config =
				talloc_strdup(
					platform,
					param_list_get_value(
						platform->params, "petitboot,bootdevs"));
			param_list_set(platform->params, "petitboot,bootdevs",
					mailbox, false);
			param = param_list_get_param(platform->params,
					"petitboot,bootdevs");
			/* Avoid writing this to NVRAM */
			param->modified = false;
			config->ipmi_bootdev_mailbox = true;
			talloc_free(mailbox);
		}
	}

	config_populate_all(config, platform->params);

	if (platform->get_ipmi_bootdev) {
		bool bootdev_persistent;
		uint8_t bootdev = IPMI_BOOTDEV_INVALID;
		rc = platform->get_ipmi_bootdev(platform, &bootdev,
				&bootdev_persistent);
		if (!rc && ipmi_bootdev_is_valid(bootdev)) {
			config_set_ipmi_bootdev(config, bootdev,
				bootdev_persistent);
		}
	}

	if (platform->ipmi)
		get_ipmi_network_override(platform, config);

	config_get_active_consoles(config);


	hash = param_list_get_value(platform->params, "petitboot,password");
	if (hash) {
		rc = crypt_set_password_hash(platform, hash);
		if (rc)
			pb_log("Failed to set password hash\n");
	}

	return 0;
}

static int save_config(struct platform *p, struct config *config)
{
	struct platform_powerpc *platform = to_platform_powerpc(p);
	struct config *defaults;
	struct param *param;

	if (config->ipmi_bootdev == IPMI_BOOTDEV_INVALID &&
	    platform->clear_ipmi_bootdev) {
		platform->clear_ipmi_bootdev(platform,
				config->ipmi_bootdev_persistent);
		config->ipmi_bootdev = IPMI_BOOTDEV_NONE;
		config->ipmi_bootdev_persistent = false;
	}

	if (!config->ipmi_bootdev_mailbox &&
			platform->ipmi_mailbox_original_config) {
		param = param_list_get_param(platform->params,
				"petitboot,bootdevs");
		/* Restore old boot order if unmodified */
		if (!param->modified) {
			param_list_set(platform->params, "petitboot,bootdevs",
					platform->ipmi_mailbox_original_config,
					false);
			param->modified = false;
			config_populate_bootdev(config, platform->params);
		}
		platform->clear_ipmi_boot_mailbox(platform);
		talloc_free(platform->ipmi_mailbox_original_config);
		platform->ipmi_mailbox_original_config = NULL;
	}

	defaults = talloc_zero(platform, struct config);
	config_set_defaults(defaults);

	params_update_all(platform->params, config, defaults);

	talloc_free(defaults);
	return write_nvram(platform);
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

	if (platform->ipmi) {
		ipmi_get_bmc_mac(platform->ipmi, sysinfo->bmc_mac);
		ipmi_get_bmc_versions(platform->ipmi, sysinfo);
	}

	if (platform->get_platform_versions)
		platform->get_platform_versions(sysinfo);

	return 0;
}

static bool restrict_clients(struct platform *p)
{
	struct platform_powerpc *platform = to_platform_powerpc(p);

	return param_list_get_value(platform->params, "petitboot,password") != NULL;
}

static int set_password(struct platform *p, const char *hash)
{
	struct platform_powerpc *platform = to_platform_powerpc(p);

	param_list_set(platform->params, "petitboot,password", hash, true);
	write_nvram(platform);

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
	platform->params = talloc_zero(platform, struct param_list);
	param_list_init(platform->params, common_known_params());

	p->platform_data = platform;

	bmc_present = stat("/proc/device-tree/bmc", &statbuf) == 0;

	if (ipmi_present() && bmc_present) {
		pb_debug("platform: using direct IPMI for IPMI paramters\n");
		platform->ipmi = ipmi_open(platform);
		platform->get_ipmi_bootdev = get_ipmi_bootdev_ipmi;
		platform->clear_ipmi_bootdev = clear_ipmi_bootdev_ipmi;
		platform->get_ipmi_boot_mailbox = get_ipmi_boot_mailbox;
		platform->clear_ipmi_boot_mailbox = clear_ipmi_boot_mailbox;
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
	.restrict_clients	= restrict_clients,
	.set_password		= set_password,
};

register_platform(platform_powerpc);
