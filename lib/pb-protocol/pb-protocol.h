#ifndef _PB_PROTOCOL_H
#define _PB_PROTOCOL_H

#include <stdint.h>
#include <stdio.h>

#include <list/list.h>
#include <types/types.h>

#define PB_SOCKET_PATH "/tmp/petitboot.ui"

#define PB_PROTOCOL_MAX_PAYLOAD_SIZE (64 * 1024)

enum pb_protocol_action {
	PB_PROTOCOL_ACTION_DEVICE_ADD		= 0x1,
	PB_PROTOCOL_ACTION_BOOT_OPTION_ADD	= 0x2,
	PB_PROTOCOL_ACTION_DEVICE_REMOVE	= 0x3,
/*	PB_PROTOCOL_ACTION_BOOT_OPTION_REMOVE	= 0x4, */
	PB_PROTOCOL_ACTION_BOOT			= 0x5,
	PB_PROTOCOL_ACTION_STATUS		= 0x6,
};

struct pb_protocol_message {
	uint32_t action;
	uint32_t payload_len;
	char     payload[];
};

void pb_protocol_dump_device(const struct device *dev, const char *text,
	FILE *stream);
int pb_protocol_device_len(const struct device *dev);
int pb_protocol_boot_option_len(const struct boot_option *opt);
int pb_protocol_boot_len(const struct boot_command *boot);
int pb_protocol_boot_status_len(const struct boot_status *status);
int pb_protocol_device_cmp(const struct device *a, const struct device *b);

int pb_protocol_boot_option_cmp(const struct boot_option *a,
	const struct boot_option *b);

int pb_protocol_serialise_string(char *pos, const char *str);
char *pb_protocol_deserialise_string(void *ctx,
		const struct pb_protocol_message *message);

int pb_protocol_serialise_device(const struct device *dev,
		char *buf, int buf_len);
int pb_protocol_serialise_boot_option(const struct boot_option *opt,
		char *buf, int buf_len);
int pb_protocol_serialise_boot_command(const struct boot_command *boot,
		char *buf, int buf_len);
int pb_protocol_serialise_boot_status(const struct boot_status *status,
		char *buf, int buf_len);

int pb_protocol_write_message(int fd, struct pb_protocol_message *message);

struct pb_protocol_message *pb_protocol_create_message(void *ctx,
		enum pb_protocol_action action, int payload_len);

struct pb_protocol_message *pb_protocol_read_message(void *ctx, int fd);

int pb_protocol_deserialise_device(struct device *dev,
		const struct pb_protocol_message *message);

int pb_protocol_deserialise_boot_option(struct boot_option *opt,
		const struct pb_protocol_message *message);

int pb_protocol_deserialise_boot_command(struct boot_command *cmd,
		const struct pb_protocol_message *message);

int pb_protocol_deserialise_boot_status(struct boot_status *status,
		const struct pb_protocol_message *message);

#endif /* _PB_PROTOCOL_H */
