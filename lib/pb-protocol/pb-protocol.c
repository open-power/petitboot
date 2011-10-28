
#include <assert.h>
#include <errno.h>
#include <string.h>
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
 *    4-byte len, boot_args
 *
 * action = 0x2: device remove message
 *  payload:
 *   4-byte len, id
 */

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
	struct boot_option *opt;
	int len;

	len =	4 + optional_strlen(dev->id) +
		4 + optional_strlen(dev->name) +
		4 + optional_strlen(dev->description) +
		4 + optional_strlen(dev->icon_file) +
		4;

	list_for_each_entry(&dev->boot_options, opt, list) {
		len +=	4 + optional_strlen(opt->id) +
			4 + optional_strlen(opt->name) +
			4 + optional_strlen(opt->description) +
			4 + optional_strlen(opt->icon_file) +
			4 + optional_strlen(opt->boot_image_file) +
			4 + optional_strlen(opt->initrd_file) +
			4 + optional_strlen(opt->boot_args);
	}

	return len;
}

int pb_protocol_serialise_device(const struct device *dev, char *buf, int buf_len)
{
	struct boot_option *opt;
	uint32_t n;
	char *pos;

	pos = buf;

	/* construct payload into buffer */
	pos += pb_protocol_serialise_string(pos, dev->id);
	pos += pb_protocol_serialise_string(pos, dev->name);
	pos += pb_protocol_serialise_string(pos, dev->description);
	pos += pb_protocol_serialise_string(pos, dev->icon_file);

	/* write option count */
	n = 0;

	list_for_each_entry(&dev->boot_options, opt, list)
		n++;

	*(uint32_t *)pos = __cpu_to_be32(n);
	pos += sizeof(uint32_t);

	/* write each option */
	list_for_each_entry(&dev->boot_options, opt, list) {
		pos += pb_protocol_serialise_string(pos, opt->id);
		pos += pb_protocol_serialise_string(pos, opt->name);
		pos += pb_protocol_serialise_string(pos, opt->description);
		pos += pb_protocol_serialise_string(pos, opt->icon_file);
		pos += pb_protocol_serialise_string(pos, opt->boot_image_file);
		pos += pb_protocol_serialise_string(pos, opt->initrd_file);
		pos += pb_protocol_serialise_string(pos, opt->boot_args);
	}

	assert(pos <= buf + buf_len);

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


struct device *pb_protocol_deserialise_device(void *ctx,
		const struct pb_protocol_message *message)
{
	struct device *dev;
	const char *pos;
	int i, n_options;
	unsigned int len;

	len = message->payload_len;
	pos = message->payload;

	dev = talloc(ctx, struct device);

	if (read_string(dev, &pos, &len, &dev->id))
		goto out_err;

	if (read_string(dev, &pos, &len, &dev->name))
		goto out_err;

	if (read_string(dev, &pos, &len, &dev->description))
		goto out_err;

	if (read_string(dev, &pos, &len, &dev->icon_file))
		goto out_err;

	n_options = __be32_to_cpu(*(uint32_t *)pos);
	pos += sizeof(uint32_t);

	dev->n_options = n_options;

	list_init(&dev->boot_options);

	for (i = 0; i < n_options; i++) {
		struct boot_option *opt;

		opt = talloc(dev, struct boot_option);

		if (read_string(opt, &pos, &len, &opt->id))
			goto out_err;
		if (read_string(opt, &pos, &len, &opt->name))
			goto out_err;
		if (read_string(opt, &pos, &len,
					&opt->description))
			goto out_err;
		if (read_string(opt, &pos, &len,
					&opt->icon_file))
			goto out_err;
		if (read_string(opt, &pos, &len,
					&opt->boot_image_file))
			goto out_err;
		if (read_string(opt, &pos, &len,
					&opt->initrd_file))
			goto out_err;
		if (read_string(opt, &pos, &len,
					&opt->boot_args))
			goto out_err;

		list_add(&dev->boot_options, &opt->list);
	}

	return dev;

out_err:
	talloc_free(dev);
	return NULL;
}
