
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <asm/byteorder.h>

#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>

#include "pb-protocol.h"


/* Message format:
 *
 * 4-byte action, determines the remaining message content
 * 4-byte total payload len
 *   - not including action and payload len header
 *
 * action = 0x1: device add message
 *  payload:
 *   4-byte len, id
 *   1-byte type
 *   4-byte len, name
 *   4-byte len, description
 *   4-byte len, icon_file
 *
 *   4-byte option count
 *   for each option:
 *    4-byte len, id
 *    4-byte len, name
 *    4-byte len, description
 *    4-byte len, icon_file
 *    4-byte len, boot_image_file
 *    4-byte len, initrd_file
 *    4-byte len, dtb_file
 *    4-byte len, boot_args
 *    4-byte len, args_sig_file
 *
 * action = 0x2: device remove message
 *  payload:
 *   4-byte len, id
 *
 * action = 0x3: boot
 *  payload:
 *   4-byte len, boot option id
 *   4-byte len, boot_image_file
 *   4-byte len, initrd_file
 *   4-byte len, dtb_file
 *   4-byte len, boot_args
 *   4-byte len, args_sig_file
 *
 */

void pb_protocol_dump_device(const struct device *dev, const char *text,
	FILE *stream)
{
	struct boot_option *opt;

	fprintf(stream, "%snew dev:\n", text);
	fprintf(stream, "%s\tid:   %s\n", text, dev->id);
	fprintf(stream, "%s\tname: %s\n", text, dev->name);
	fprintf(stream, "%s\tdesc: %s\n", text, dev->description);
	fprintf(stream, "%s\ticon: %s\n", text, dev->icon_file);
	fprintf(stream, "%s\tboot options:\n", text);
	list_for_each_entry(&dev->boot_options, opt, list) {
		fprintf(stream, "%s\t\tid:   %s\n", text, opt->id);
		fprintf(stream, "%s\t\tname: %s\n", text, opt->name);
		fprintf(stream, "%s\t\tdesc: %s\n", text, opt->description);
		fprintf(stream, "%s\t\ticon: %s\n", text, opt->icon_file);
		fprintf(stream, "%s\t\tboot: %s\n", text, opt->boot_image_file);
		fprintf(stream, "%s\t\tinit: %s\n", text, opt->initrd_file);
		fprintf(stream, "%s\t\tdtb:  %s\n", text, opt->dtb_file);
		fprintf(stream, "%s\t\targs: %s\n", text, opt->boot_args);
		fprintf(stream, "%s\t\tasig: %s\n", text, opt->args_sig_file);
	}
}

int pb_protocol_device_cmp(const struct device *a, const struct device *b)
{
	return !strcmp(a->id, b->id);
}

int pb_protocol_boot_option_cmp(const struct boot_option *a,
	const struct boot_option *b)
{
	return !strcmp(a->id, b->id);
}

/* Write a string into the buffer, starting at pos.
 *
 * Returns the total length used for the write, including length header.
 */
int pb_protocol_serialise_string(char *pos, const char *str)
{
	int len = 0;

	if (str)
		len = strlen(str);

	*(uint32_t *)pos = __cpu_to_be32(len);
	pos += sizeof(uint32_t);

	memcpy(pos, str, len);

	return len + sizeof(uint32_t);
}

/* Read a string from a buffer, allocating the new string as necessary.
 *
 * @param[in] ctx	The talloc context to base the allocation on
 * @param[in,out] pos	Where to start reading
 * @param[in,out] len	The amount of data remaining in the buffer
 * @param[out] str	Pointer to resuling string
 * @return		zero on success, non-zero on failure
 */
static int read_string(void *ctx, const char **pos, unsigned int *len,
	char **str)
{
	uint32_t str_len, read_len;

	if (*len < sizeof(uint32_t))
		return -1;

	str_len = __be32_to_cpu(*(uint32_t *)(*pos));
	read_len = sizeof(uint32_t);

	if (read_len + str_len > *len)
		return -1;

	if (str_len == 0)
		*str = NULL;
	else
		*str = talloc_strndup(ctx, *pos + read_len, str_len);

	read_len += str_len;

	/* all ok, update the caller's pointers */
	*pos += read_len;
	*len -= read_len;

	return 0;
}

static int read_u32(const char **pos, unsigned int *len, unsigned int *p)
{
	if (*len < sizeof(uint32_t))
		return -1;

	*p = (unsigned int)__be32_to_cpu(*(uint32_t *)(*pos));
	*pos += sizeof(uint32_t);
	*len -= sizeof(uint32_t);

	return 0;
}

char *pb_protocol_deserialise_string(void *ctx,
		const struct pb_protocol_message *message)
{
	const char *buf;
	char *str;
	unsigned int len;

	len = message->payload_len;
	buf = message->payload;

	if (read_string(ctx, &buf, &len, &str))
		return NULL;

	return str;
}

static int optional_strlen(const char *str)
{
	if (!str)
		return 0;
	return strlen(str);
}

int pb_protocol_device_len(const struct device *dev)
{
	return	4 + optional_strlen(dev->id) +
		sizeof(dev->type) +
		4 + optional_strlen(dev->name) +
		4 + optional_strlen(dev->description) +
		4 + optional_strlen(dev->icon_file);
}

int pb_protocol_boot_option_len(const struct boot_option *opt)
{

	return	4 + optional_strlen(opt->device_id) +
		4 + optional_strlen(opt->id) +
		4 + optional_strlen(opt->name) +
		4 + optional_strlen(opt->description) +
		4 + optional_strlen(opt->icon_file) +
		4 + optional_strlen(opt->boot_image_file) +
		4 + optional_strlen(opt->initrd_file) +
		4 + optional_strlen(opt->dtb_file) +
		4 + optional_strlen(opt->boot_args) +
		4 + optional_strlen(opt->args_sig_file) +
		sizeof(opt->is_default);
}

int pb_protocol_boot_len(const struct boot_command *boot)
{
	return  4 + optional_strlen(boot->option_id) +
		4 + optional_strlen(boot->boot_image_file) +
		4 + optional_strlen(boot->initrd_file) +
		4 + optional_strlen(boot->dtb_file) +
		4 + optional_strlen(boot->boot_args) +
		4 + optional_strlen(boot->args_sig_file) +
		4 + optional_strlen(boot->console);
}

int pb_protocol_boot_status_len(const struct boot_status *status)
{
	return  4 +
		4 + optional_strlen(status->message) +
		4 + optional_strlen(status->detail) +
		4;
}

int pb_protocol_system_info_len(const struct system_info *sysinfo)
{
	unsigned int len, i;

	len =	4 + optional_strlen(sysinfo->type) +
		4 + optional_strlen(sysinfo->identifier) +
		4 + 4;

	len +=	4;
	for (i = 0; i < sysinfo->n_primary; i++)
		len += 4 + optional_strlen(sysinfo->platform_primary[i]);
	len +=	4;
	for (i = 0; i < sysinfo->n_other; i++)
		len += 4 + optional_strlen(sysinfo->platform_other[i]);

	len +=	4;
	for (i = 0; i < sysinfo->n_bmc_current; i++)
		len += 4 + optional_strlen(sysinfo->bmc_current[i]);
	len +=	4;
	for (i = 0; i < sysinfo->n_bmc_golden; i++)
		len += 4 + optional_strlen(sysinfo->bmc_golden[i]);

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *if_info = sysinfo->interfaces[i];
		len +=	4 + if_info->hwaddr_size +
			4 + optional_strlen(if_info->name) +
			sizeof(if_info->link);
	}

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *bd_info = sysinfo->blockdevs[i];
		len +=	4 + optional_strlen(bd_info->name) +
			4 + optional_strlen(bd_info->uuid) +
			4 + optional_strlen(bd_info->mountpoint);
	}

	/* BMC MAC */
	len += HWADDR_SIZE;

	return len;
}

static int pb_protocol_interface_config_len(struct interface_config *conf)
{
	unsigned int len;

	len =	sizeof(conf->hwaddr) +
		4 /* conf->ignore */;

	if (conf->ignore)
		return len;

	len += 4 /* conf->method */;

	if (conf->method == CONFIG_METHOD_STATIC) {
		len += 4 + optional_strlen(conf->static_config.address);
		len += 4 + optional_strlen(conf->static_config.gateway);
		len += 4 + optional_strlen(conf->static_config.url);
	}

	len += 4 /* conf->override */;

	return len;
}

int pb_protocol_config_len(const struct config *config)
{
	unsigned int i, len;

	len =	4 /* config->autoboot_enabled */ +
		4 /* config->autoboot_timeout_sec */ +
		4 /* config->safe_mode */;

	len += 4;
	for (i = 0; i < config->network.n_interfaces; i++)
		len += pb_protocol_interface_config_len(
				config->network.interfaces[i]);

	len += 4;
	for (i = 0; i < config->network.n_dns_servers; i++)
		len += 4 + optional_strlen(config->network.dns_servers[i]);

	len += 4 + optional_strlen(config->http_proxy);
	len += 4 + optional_strlen(config->https_proxy);

	len += 4;
	for (i = 0; i < config->n_autoboot_opts; i++) {
		if (config->autoboot_opts[i].boot_type == BOOT_DEVICE_TYPE)
			len += 4 + 4;
		else
			len += 4 + 4 +
				optional_strlen(config->autoboot_opts[i].uuid);
	}

	len += 4 + 4; /* ipmi_bootdev, ipmi_bootdev_persistent */

	len += 4; /* allow_writes */

	len += 4; /* n_consoles */
	for (i = 0; i < config->n_consoles; i++)
		len += 4 + optional_strlen(config->consoles[i]);

	len += 4 + optional_strlen(config->boot_console);
	len += 4; /* manual_console */

	len += 4 + optional_strlen(config->lang);

	return len;
}

int pb_protocol_url_len(const char *url)
{
	/* url + length field */
	return 4 + optional_strlen(url);
}

int pb_protocol_serialise_device(const struct device *dev,
		char *buf, int buf_len)
{
	char *pos = buf;

	pos += pb_protocol_serialise_string(pos, dev->id);
	*(enum device_type *)pos = dev->type;
	pos += sizeof(enum device_type);
	pos += pb_protocol_serialise_string(pos, dev->name);
	pos += pb_protocol_serialise_string(pos, dev->description);
	pos += pb_protocol_serialise_string(pos, dev->icon_file);

	assert(pos <= buf + buf_len);
	(void)buf_len;

	return 0;
}

int pb_protocol_serialise_boot_option(const struct boot_option *opt,
		char *buf, int buf_len)
{
	char *pos = buf;

	pos += pb_protocol_serialise_string(pos, opt->device_id);
	pos += pb_protocol_serialise_string(pos, opt->id);
	pos += pb_protocol_serialise_string(pos, opt->name);
	pos += pb_protocol_serialise_string(pos, opt->description);
	pos += pb_protocol_serialise_string(pos, opt->icon_file);
	pos += pb_protocol_serialise_string(pos, opt->boot_image_file);
	pos += pb_protocol_serialise_string(pos, opt->initrd_file);
	pos += pb_protocol_serialise_string(pos, opt->dtb_file);
	pos += pb_protocol_serialise_string(pos, opt->boot_args);
	pos += pb_protocol_serialise_string(pos, opt->args_sig_file);

	*(bool *)pos = opt->is_default;
	pos += sizeof(bool);

	assert(pos <= buf + buf_len);
	(void)buf_len;

	return 0;
}

int pb_protocol_serialise_boot_command(const struct boot_command *boot,
		char *buf, int buf_len)
{
	char *pos = buf;

	pos += pb_protocol_serialise_string(pos, boot->option_id);
	pos += pb_protocol_serialise_string(pos, boot->boot_image_file);
	pos += pb_protocol_serialise_string(pos, boot->initrd_file);
	pos += pb_protocol_serialise_string(pos, boot->dtb_file);
	pos += pb_protocol_serialise_string(pos, boot->boot_args);
	pos += pb_protocol_serialise_string(pos, boot->args_sig_file);
	pos += pb_protocol_serialise_string(pos, boot->console);

	assert(pos <= buf + buf_len);
	(void)buf_len;

	return 0;
}

int pb_protocol_serialise_boot_status(const struct boot_status *status,
		char *buf, int buf_len)
{
	char *pos = buf;

	*(uint32_t *)pos = __cpu_to_be32(status->type);
	pos += sizeof(uint32_t);

	pos += pb_protocol_serialise_string(pos, status->message);
	pos += pb_protocol_serialise_string(pos, status->detail);

	*(uint32_t *)pos = __cpu_to_be32(status->type);
	pos += sizeof(uint32_t);

	assert(pos <= buf + buf_len);
	(void)buf_len;

	return 0;
}

int pb_protocol_serialise_system_info(const struct system_info *sysinfo,
		char *buf, int buf_len)
{
	char *pos = buf;
	unsigned int i;

	pos += pb_protocol_serialise_string(pos, sysinfo->type);
	pos += pb_protocol_serialise_string(pos, sysinfo->identifier);

	*(uint32_t *)pos = __cpu_to_be32(sysinfo->n_primary);
	pos += sizeof(uint32_t);
	for (i = 0; i < sysinfo->n_primary; i++)
		pos += pb_protocol_serialise_string(pos, sysinfo->platform_primary[i]);

	*(uint32_t *)pos = __cpu_to_be32(sysinfo->n_other);
	pos += sizeof(uint32_t);
	for (i = 0; i < sysinfo->n_other; i++)
		pos += pb_protocol_serialise_string(pos, sysinfo->platform_other[i]);

	*(uint32_t *)pos = __cpu_to_be32(sysinfo->n_bmc_current);
	pos += sizeof(uint32_t);
	for (i = 0; i < sysinfo->n_bmc_current; i++)
		pos += pb_protocol_serialise_string(pos, sysinfo->bmc_current[i]);

	*(uint32_t *)pos = __cpu_to_be32(sysinfo->n_bmc_golden);
	pos += sizeof(uint32_t);
	for (i = 0; i < sysinfo->n_bmc_golden; i++)
		pos += pb_protocol_serialise_string(pos, sysinfo->bmc_golden[i]);

	*(uint32_t *)pos = __cpu_to_be32(sysinfo->n_interfaces);
	pos += sizeof(uint32_t);

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *if_info = sysinfo->interfaces[i];

		*(uint32_t *)pos = __cpu_to_be32(if_info->hwaddr_size);
		pos += sizeof(uint32_t);

		memcpy(pos, if_info->hwaddr, if_info->hwaddr_size);
		pos += if_info->hwaddr_size;

		pos += pb_protocol_serialise_string(pos, if_info->name);

		*(bool *)pos = if_info->link;
		pos += sizeof(bool);
	}

	*(uint32_t *)pos = __cpu_to_be32(sysinfo->n_blockdevs);
	pos += sizeof(uint32_t);

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *bd_info = sysinfo->blockdevs[i];

		pos += pb_protocol_serialise_string(pos, bd_info->name);
		pos += pb_protocol_serialise_string(pos, bd_info->uuid);
		pos += pb_protocol_serialise_string(pos, bd_info->mountpoint);
	}

	if (sysinfo->bmc_mac)
		memcpy(pos, sysinfo->bmc_mac, HWADDR_SIZE);
	else
		memset(pos, 0, HWADDR_SIZE);
	pos += HWADDR_SIZE;

	assert(pos <= buf + buf_len);
	(void)buf_len;

	return 0;
}

static int pb_protocol_serialise_config_interface(char *buf,
		struct interface_config *conf)
{
	char *pos = buf;

	memcpy(pos, conf->hwaddr, sizeof(conf->hwaddr));
	pos += sizeof(conf->hwaddr);

	*(uint32_t *)pos = conf->ignore;
	pos += 4;

	if (conf->ignore)
		return pos - buf;

	*(uint32_t *)pos = __cpu_to_be32(conf->method);
	pos += 4;

	if (conf->method == CONFIG_METHOD_STATIC) {
		pos += pb_protocol_serialise_string(pos,
				conf->static_config.address);
		pos += pb_protocol_serialise_string(pos,
				conf->static_config.gateway);
		pos += pb_protocol_serialise_string(pos,
				conf->static_config.url);
	}

	*(uint32_t *)pos = conf->override;
	pos += 4;

	return pos - buf;
}

int pb_protocol_serialise_config(const struct config *config,
		char *buf, int buf_len)
{
	char *pos = buf;
	unsigned int i;

	*(uint32_t *)pos = config->autoboot_enabled;
	pos += 4;

	*(uint32_t *)pos = __cpu_to_be32(config->autoboot_timeout_sec);
	pos += 4;

	*(uint32_t *)pos = config->safe_mode;
	pos += 4;

	*(uint32_t *)pos = __cpu_to_be32(config->network.n_interfaces);
	pos += 4;
	for (i = 0; i < config->network.n_interfaces; i++) {
		struct interface_config *iface =
			config->network.interfaces[i];
		pos += pb_protocol_serialise_config_interface(pos, iface);
	}

	*(uint32_t *)pos = __cpu_to_be32(config->network.n_dns_servers);
	pos += 4;
	for (i = 0; i < config->network.n_dns_servers; i++) {
		pos += pb_protocol_serialise_string(pos,
				config->network.dns_servers[i]);
	}

	pos += pb_protocol_serialise_string(pos, config->http_proxy);
	pos += pb_protocol_serialise_string(pos, config->https_proxy);

	*(uint32_t *)pos = __cpu_to_be32(config->n_autoboot_opts);
	pos += 4;
	for (i = 0; i < config->n_autoboot_opts; i++) {
		*(uint32_t *)pos =
			__cpu_to_be32(config->autoboot_opts[i].boot_type);
		pos += 4;
		if (config->autoboot_opts[i].boot_type == BOOT_DEVICE_TYPE) {
			*(uint32_t *)pos =
				__cpu_to_be32(config->autoboot_opts[i].type);
			pos += 4;
		} else {
			pos += pb_protocol_serialise_string(pos,
						config->autoboot_opts[i].uuid);
		}
	}

	*(uint32_t *)pos = __cpu_to_be32(config->ipmi_bootdev);
	pos += 4;
	*(uint32_t *)pos = config->ipmi_bootdev_persistent;
	pos += 4;

	*(uint32_t *)pos = config->allow_writes;
	pos += 4;

	*(uint32_t *)pos = __cpu_to_be32(config->n_consoles);
	pos += 4;
	for (i = 0; i < config->n_consoles; i++)
		pos += pb_protocol_serialise_string(pos, config->consoles[i]);

	pos += pb_protocol_serialise_string(pos, config->boot_console);
	*(uint32_t *)pos = config->manual_console;
	pos += 4;

	pos += pb_protocol_serialise_string(pos, config->lang);

	assert(pos <= buf + buf_len);
	(void)buf_len;

	return 0;
}

int pb_protocol_serialise_url(const char *url, char *buf, int buf_len)
{
	char *pos = buf;

	pos += pb_protocol_serialise_string(pos, url);

	assert(pos <=buf+buf_len);
	(void)buf_len;

	return 0;
}

int pb_protocol_write_message(int fd, struct pb_protocol_message *message)
{
	int total_len, rc;
	char *pos;

	total_len = sizeof(*message) + message->payload_len;

	message->payload_len = __cpu_to_be32(message->payload_len);
	message->action = __cpu_to_be32(message->action);

	for (pos = (void *)message; total_len;) {
		rc = write(fd, pos, total_len);

		if (rc <= 0)
			break;

		total_len -= rc;
		pos += rc;
	}

	talloc_free(message);

	if (!total_len)
		return 0;

	pb_log("%s: failed: %s\n", __func__, strerror(errno));
	return -1;
}

struct pb_protocol_message *pb_protocol_create_message(void *ctx,
		enum pb_protocol_action action, int payload_len)
{
	struct pb_protocol_message *message;

	if (payload_len > PB_PROTOCOL_MAX_PAYLOAD_SIZE) {
		pb_log("%s: payload too big %u/%u\n", __func__, payload_len,
			PB_PROTOCOL_MAX_PAYLOAD_SIZE);
		return NULL;
	}

	message = talloc_size(ctx, sizeof(*message) + payload_len);

	/* we convert these to big-endian in write_message() */
	message->action = action;
	message->payload_len = payload_len;

	return message;

}

struct pb_protocol_message *pb_protocol_read_message(void *ctx, int fd)
{
	struct pb_protocol_message *message, m;
	int rc;
	unsigned int len;

	/* use the stack for the initial 8-byte read */

	rc = read(fd, &m, sizeof(m));
	if (rc != sizeof(m))
		return NULL;

	m.payload_len = __be32_to_cpu(m.payload_len);
	m.action = __be32_to_cpu(m.action);

	if (m.payload_len > PB_PROTOCOL_MAX_PAYLOAD_SIZE) {
		pb_log("%s: payload too big %u/%u\n", __func__, m.payload_len,
			PB_PROTOCOL_MAX_PAYLOAD_SIZE);
		return NULL;
	}

	message = talloc_size(ctx, sizeof(m) + m.payload_len);
	memcpy(message, &m, sizeof(m));

	for (len = 0; len < m.payload_len;) {
		rc = read(fd, message->payload + len, m.payload_len - len);

		if (rc <= 0) {
			talloc_free(message);
			pb_log("%s: failed (%u): %s\n", __func__, len,
				strerror(errno));
			return NULL;
		}

		len += rc;
	}

	return message;
}


int pb_protocol_deserialise_device(struct device *dev,
		const struct pb_protocol_message *message)
{
	unsigned int len;
	const char *pos;
	int rc = -1;

	len = message->payload_len;
	pos = message->payload;

	if (read_string(dev, &pos, &len, &dev->id))
		goto out;

	if (len < sizeof(enum device_type))
		goto out;
	dev->type = *(enum device_type *)(pos);
	pos += sizeof(enum device_type);
	len -= sizeof(enum device_type);

	if (read_string(dev, &pos, &len, &dev->name))
		goto out;

	if (read_string(dev, &pos, &len, &dev->description))
		goto out;

	if (read_string(dev, &pos, &len, &dev->icon_file))
		goto out;

	rc = 0;

out:
	return rc;
}

int pb_protocol_deserialise_boot_option(struct boot_option *opt,
		const struct pb_protocol_message *message)
{
	unsigned int len;
	const char *pos;
	int rc = -1;

	len = message->payload_len;
	pos = message->payload;

	if (read_string(opt, &pos, &len, &opt->device_id))
		goto out;

	if (read_string(opt, &pos, &len, &opt->id))
		goto out;

	if (read_string(opt, &pos, &len, &opt->name))
		goto out;

	if (read_string(opt, &pos, &len, &opt->description))
		goto out;

	if (read_string(opt, &pos, &len, &opt->icon_file))
		goto out;

	if (read_string(opt, &pos, &len, &opt->boot_image_file))
		goto out;

	if (read_string(opt, &pos, &len, &opt->initrd_file))
		goto out;

	if (read_string(opt, &pos, &len, &opt->dtb_file))
		goto out;

	if (read_string(opt, &pos, &len, &opt->boot_args))
		goto out;

	if (read_string(opt, &pos, &len, &opt->args_sig_file))
		goto out;

	if (len < sizeof(bool))
		goto out;
	opt->is_default = *(bool *)(pos);

	rc = 0;

out:
	return rc;
}

int pb_protocol_deserialise_boot_command(struct boot_command *cmd,
		const struct pb_protocol_message *message)
{
	unsigned int len;
	const char *pos;
	int rc = -1;

	len = message->payload_len;
	pos = message->payload;

	if (read_string(cmd, &pos, &len, &cmd->option_id))
		goto out;

	if (read_string(cmd, &pos, &len, &cmd->boot_image_file))
		goto out;

	if (read_string(cmd, &pos, &len, &cmd->initrd_file))
		goto out;

	if (read_string(cmd, &pos, &len, &cmd->dtb_file))
		goto out;

	if (read_string(cmd, &pos, &len, &cmd->boot_args))
		goto out;

	if (read_string(cmd, &pos, &len, &cmd->args_sig_file))
		goto out;

	if (read_string(cmd, &pos, &len, &cmd->console))
		goto out;

	rc = 0;

out:
	return rc;
}

int pb_protocol_deserialise_boot_status(struct boot_status *status,
		const struct pb_protocol_message *message)
{
	unsigned int len;
	const char *pos;
	int rc = -1;

	len = message->payload_len;
	pos = message->payload;

	/* first up, the type enum... */
	if (len < sizeof(uint32_t))
		goto out;

	status->type = __be32_to_cpu(*(uint32_t *)(pos));

	switch (status->type) {
	case BOOT_STATUS_ERROR:
	case BOOT_STATUS_INFO:
		break;
	default:
		goto out;
	}

	pos += sizeof(uint32_t);
	len -= sizeof(uint32_t);

	/* message and detail strings */
	if (read_string(status, &pos, &len, &status->message))
		goto out;

	if (read_string(status, &pos, &len, &status->detail))
		goto out;

	/* and finally, progress */
	if (len < sizeof(uint32_t))
		goto out;

	status->progress = __be32_to_cpu(*(uint32_t *)(pos));

	/* clamp to 100% */
	if (status->progress > 100)
		status->progress = 100;

	rc = 0;

out:
	return rc;
}

int pb_protocol_deserialise_system_info(struct system_info *sysinfo,
		const struct pb_protocol_message *message)
{
	unsigned int len, i;
	const char *pos;
	int rc = -1;
	char *tmp;

	len = message->payload_len;
	pos = message->payload;

	/* type and identifier strings */
	if (read_string(sysinfo, &pos, &len, &sysinfo->type))
		goto out;

	if (read_string(sysinfo, &pos, &len, &sysinfo->identifier))
		goto out;

	/* Platform version strings for openpower platforms */
	if (read_u32(&pos, &len, &sysinfo->n_primary))
		goto out;
	sysinfo->platform_primary = talloc_array(sysinfo, char *,
						sysinfo->n_primary);
	for (i = 0; i < sysinfo->n_primary; i++) {
		if (read_string(sysinfo, &pos, &len, &tmp))
			goto out;
		sysinfo->platform_primary[i] = talloc_strdup(sysinfo, tmp);
	}

	if (read_u32(&pos, &len, &sysinfo->n_other))
		goto out;
	sysinfo->platform_other = talloc_array(sysinfo, char *,
						sysinfo->n_other);
	for (i = 0; i < sysinfo->n_other; i++) {
		if (read_string(sysinfo, &pos, &len, &tmp))
			goto out;
		sysinfo->platform_other[i] = talloc_strdup(sysinfo, tmp);
	}

	/* BMC version strings for openpower platforms */
	if (read_u32(&pos, &len, &sysinfo->n_bmc_current))
		goto out;
	sysinfo->bmc_current = talloc_array(sysinfo, char *,
						sysinfo->n_bmc_current);
	for (i = 0; i < sysinfo->n_bmc_current; i++) {
		if (read_string(sysinfo, &pos, &len, &tmp))
			goto out;
		sysinfo->bmc_current[i] = talloc_strdup(sysinfo, tmp);
	}

	if (read_u32(&pos, &len, &sysinfo->n_bmc_golden))
		goto out;
	sysinfo->bmc_golden = talloc_array(sysinfo, char *,
						sysinfo->n_bmc_golden);
	for (i = 0; i < sysinfo->n_bmc_golden; i++) {
		if (read_string(sysinfo, &pos, &len, &tmp))
			goto out;
		sysinfo->bmc_golden[i] = talloc_strdup(sysinfo, tmp);
	}

	/* number of interfaces */
	if (read_u32(&pos, &len, &sysinfo->n_interfaces))
		goto out;

	sysinfo->interfaces = talloc_array(sysinfo, struct interface_info *,
			sysinfo->n_interfaces);

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *if_info = talloc(sysinfo,
							struct interface_info);

		if (read_u32(&pos, &len, &if_info->hwaddr_size))
			goto out;

		if (len < if_info->hwaddr_size)
			goto out;

		if_info->hwaddr = talloc_memdup(if_info, pos,
						if_info->hwaddr_size);
		pos += if_info->hwaddr_size;
		len -= if_info->hwaddr_size;

		if (read_string(if_info, &pos, &len, &if_info->name))
			goto out;

		if_info->link = *(bool *)pos;
		pos += sizeof(if_info->link);

		sysinfo->interfaces[i] = if_info;
	}

	/* number of interfaces */
	if (read_u32(&pos, &len, &sysinfo->n_blockdevs))
		goto out;

	sysinfo->blockdevs = talloc_array(sysinfo, struct blockdev_info *,
			sysinfo->n_blockdevs);

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *bd_info = talloc(sysinfo,
							struct blockdev_info);

		if (read_string(bd_info, &pos, &len, &bd_info->name))
			goto out;

		if (read_string(bd_info, &pos, &len, &bd_info->uuid))
			goto out;

		if (read_string(bd_info, &pos, &len, &bd_info->mountpoint))
			goto out;

		sysinfo->blockdevs[i] = bd_info;
	}

	for (i = 0; i < HWADDR_SIZE; i++) {
		if (pos[i] != 0) {
			sysinfo->bmc_mac = talloc_memdup(sysinfo, pos, HWADDR_SIZE);
			break;
		}
	}

	pos += HWADDR_SIZE;
	len -= HWADDR_SIZE;

	rc = 0;
out:
	return rc;
}

static int pb_protocol_deserialise_config_interface(const char **buf,
		unsigned int *len, struct interface_config *iface)
{
	unsigned int tmp;

	if (*len < sizeof(iface->hwaddr))
		return -1;

	memcpy(iface->hwaddr, *buf, sizeof(iface->hwaddr));
	*buf += sizeof(iface->hwaddr);
	*len -= sizeof(iface->hwaddr);

	if (read_u32(buf, len, &tmp))
		return -1;
	iface->ignore = !!tmp;

	if (iface->ignore)
		return 0;

	if (read_u32(buf, len, &iface->method))
		return -1;

	if (iface->method == CONFIG_METHOD_STATIC) {
		if (read_string(iface, buf, len, &iface->static_config.address))
			return -1;

		if (read_string(iface, buf, len, &iface->static_config.gateway))
			return -1;

		if (read_string(iface, buf, len, &iface->static_config.url))
			return -1;
	}

	if (read_u32(buf, len, &tmp))
		return -1;
	iface->override = !!tmp;

	return 0;
}

int pb_protocol_deserialise_config(struct config *config,
		const struct pb_protocol_message *message)
{
	unsigned int len, i, tmp;
	const char *pos;
	int rc = -1;
	char *str;

	len = message->payload_len;
	pos = message->payload;

	if (read_u32(&pos, &len, &tmp))
		goto out;
	config->autoboot_enabled = !!tmp;

	if (read_u32(&pos, &len, &config->autoboot_timeout_sec))
		goto out;

	if (read_u32(&pos, &len, &tmp))
		goto out;
	config->safe_mode = !!tmp;

	if (read_u32(&pos, &len, &config->network.n_interfaces))
		goto out;

	config->network.interfaces = talloc_array(config,
			struct interface_config *, config->network.n_interfaces);

	for (i = 0; i < config->network.n_interfaces; i++) {
		struct interface_config *iface = talloc_zero(
				config->network.interfaces,
				struct interface_config);
		if (pb_protocol_deserialise_config_interface(&pos, &len, iface))
			goto out;
		config->network.interfaces[i] = iface;
	}

	if (read_u32(&pos, &len, &config->network.n_dns_servers))
		goto out;
	config->network.dns_servers = talloc_array(config, const char *,
			config->network.n_dns_servers);

	for (i = 0; i < config->network.n_dns_servers; i++) {
		if (read_string(config->network.dns_servers, &pos, &len, &str))
			goto out;
		config->network.dns_servers[i] = str;
	}

	if (read_string(config, &pos, &len, &str))
		goto out;
	config->http_proxy = str;
	if (read_string(config, &pos, &len, &str))
		goto out;
	config->https_proxy = str;

	if (read_u32(&pos, &len, &config->n_autoboot_opts))
		goto out;
	config->autoboot_opts = talloc_array(config, struct autoboot_option,
			config->n_autoboot_opts);

	for (i = 0; i < config->n_autoboot_opts; i++) {
		if (read_u32(&pos, &len, &tmp))
			goto out;
		config->autoboot_opts[i].boot_type = (int)tmp;
		if (config->autoboot_opts[i].boot_type == BOOT_DEVICE_TYPE) {
			if (read_u32(&pos, &len, &tmp))
				goto out;
			config->autoboot_opts[i].type = tmp;
		} else {
			if (read_string(config, &pos, &len, &str))
				goto out;
			config->autoboot_opts[i].uuid = str;
		}
	}

	if (read_u32(&pos, &len, &config->ipmi_bootdev))
		goto out;
	if (read_u32(&pos, &len, &tmp))
		goto out;
	config->ipmi_bootdev_persistent = !!tmp;

	if (read_u32(&pos, &len, &tmp))
		goto out;
	config->allow_writes = !!tmp;

	if (read_u32(&pos, &len, &config->n_consoles))
		goto out;

	config->consoles = talloc_array(config, char *, config->n_consoles);
	for (i = 0; i < config->n_consoles; i++) {
		if (read_string(config->consoles, &pos, &len, &str))
			goto out;
		config->consoles[i] = str;
	}

	if (read_string(config, &pos, &len, &str))
		goto out;

	config->boot_console = str;

	if (read_u32(&pos, &len, &tmp))
		goto out;
	config->manual_console = !!tmp;

	if (read_string(config, &pos, &len, &str))
		goto out;

	config->lang = str;

	rc = 0;

out:
	return rc;
}
