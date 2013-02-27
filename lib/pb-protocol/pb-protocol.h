#ifndef _PB_PROTOCOL_H
#define _PB_PROTOCOL_H

#include <stdint.h>
#include <stdio.h>

#include <list/list.h>
#include <types/types.h>

#define PB_SOCKET_PATH "/tmp/petitboot.ui"

#define PB_PROTOCOL_MAX_PAYLOAD_SIZE (64 * 1024)

enum pb_protocol_action {
	PB_PROTOCOL_ACTION_ADD		= 0x1,
	PB_PROTOCOL_ACTION_REMOVE	= 0x2,
	PB_PROTOCOL_ACTION_BOOT		= 0x3,
};

struct pb_protocol_message {
	uint32_t action;
	uint32_t payload_len;
	char     payload[];
};

void pb_protocol_dump_device(const struct device *dev, const char *text,
	FILE *stream);
int pb_protocol_device_len(const struct device *dev);
int pb_protocol_device_cmp(const struct device *a, const struct device *b);

int pb_protocol_boot_option_cmp(const struct boot_option *a,
	const struct boot_option *b);

int pb_protocol_serialise_string(char *pos, const char *str);
char *pb_protocol_deserialise_string(void *ctx,
		const struct pb_protocol_message *message);

int pb_protocol_serialise_device(const struct device *dev, char *buf, int buf_len);

int pb_protocol_write_message(int fd, struct pb_protocol_message *message);

struct pb_protocol_message *pb_protocol_create_message(void *ctx,
		enum pb_protocol_action action, int payload_len);

struct pb_protocol_message *pb_protocol_read_message(void *ctx, int fd);

struct device *pb_protocol_deserialise_device(void *ctx,
		const struct pb_protocol_message *message);

#endif /* _PB_PROTOCOL_H */
