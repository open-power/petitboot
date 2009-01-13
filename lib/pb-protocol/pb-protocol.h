#ifndef _PB_PROTOCOL_H
#define _PB_PROTOCOL_H

#include <stdint.h>

#include <list/list.h>

#define PB_SOCKET_PATH "/tmp/petitboot.ui"

#define PB_PROTOCOL_MAX_PAYLOAD_SIZE 4096

enum pb_protocol_action {
	PB_PROTOCOL_ACTION_ADD		= 0x1,
	PB_PROTOCOL_ACTION_REMOVE	= 0x2,
};

struct pb_protocol_message {
	uint32_t action;
	uint32_t payload_len;
	char     payload[];
};

struct device {
	char *id;
	char *name;
	char *description;
	char *icon_file;

	struct list boot_options;
};

struct boot_option {
	char *id;
	char *name;
	char *description;
	char *icon_file;
	char *boot_image_file;
	char *initrd_file;
	char *boot_args;

	struct list_item list;
};

int pb_protocol_device_len(struct device *dev);

int pb_protocol_serialise_string(char *pos, const char *str);
char *pb_protocol_deserialise_string(void *ctx,
		struct pb_protocol_message *message);

int pb_protocol_serialise_device(struct device *dev, char *buf, int buf_len);

int pb_protocol_write_message(int fd, struct pb_protocol_message *message);

struct pb_protocol_message *pb_protocol_create_message(void *ctx,
		enum pb_protocol_action action, int payload_len);

struct pb_protocol_message *pb_protocol_read_message(void *ctx, int fd);

struct device *pb_protocol_deserialise_device(void *ctx,
		struct pb_protocol_message *message);

#endif /* _PB_PROTOCOL_H */
